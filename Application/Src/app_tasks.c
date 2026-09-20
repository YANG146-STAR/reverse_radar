#include "app_tasks.h"
#include "app_config.h"
#include "app_outputs.h"
#include "app_ultrasonic.h"
#include "app_alarm.h"
#include "app_menu.h"
#include "oled.h"
#include "ds18b20.h"
#include "mpu6050.h"
#include "tim.h"             /* htim4 */
#include "i2c.h"              /* hi2c1 (MPU6050 / OLED 共用) */
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
//  共享数据定义 (app_config.h extern 暴露给应用层)
// ============================================================================
SysConfig       sys_config;          /* 系统配置参数 */
Mpu6050Result   g_mpu;               /* MPU6050 姿态最新结果 */
float           g_distance = -1.0f;  /* 距离 cm, -1=测距失败 */
float           g_temp     = 25.0f;  /* 温度 °C */
uint8_t         g_zone     = 4;      /* 报警区: 0=EMG 1=DNG 2=WRN 3=SAFE 4=ERR */
uint8_t         g_tilt     = 0;      /* 1=姿态告警 */

// ============================================================================
//  RTOS 对象定义 (app_tasks.h extern 暴露)
// ============================================================================
QueueHandle_t      xQueueEncoder;
SemaphoreHandle_t  xMutexSysConfig;
SemaphoreHandle_t  xMutexSensor;
SemaphoreHandle_t  xMutexI2C;

// ============================================================================
//  FreeRTOS 任务函数 (5 个任务, 优先级 6>5>4=4>2>0=IDLE, stack 单位为 words)
//    6 = InitTask    (上电初始化, 完成后 vTaskDelete 自删)
//    5 = SensorTask  (超声+MPU+温度, 50ms 周期)
//    4 = AlarmTask   (RGB+蜂鸣器, 10ms 周期)
//    4 = EncoderTask (编码器 polling, 2ms 周期)
//    2 = DisplayTask (OLED 显示+菜单, 50ms 周期)
// ============================================================================

// ====== Task 1: InitTask — 上电初始化 (单次执行, 完成后 vTaskDelete 自删) ======
static void InitTask(void *arg) {
    (void)arg;
    Mpu6050Result mpu;
    static Mpu6050Result s_mpu_last;
    memset(&mpu, 0, sizeof(mpu));

    // 1. OLED + 启动画面 (必须持 xMutexI2C, 防止与 SensorTask/DisplayTask 抢 hi2c1)
    xSemaphoreTake(xMutexI2C, portMAX_DELAY);
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, "Reverse Radar  ");
    OLED_Refresh();
    xSemaphoreGive(xMutexI2C);

    // 2. 亮度默认值
    xSemaphoreTake(xMutexI2C, portMAX_DELAY);
    OLED_SetBrightness(sys_config.brightness);
    xSemaphoreGive(xMutexI2C);

    // 3. MPU6050 初始化 + 陀螺仪校准 (~2s, 阻塞)
    {
        xSemaphoreTake(xMutexI2C, portMAX_DELAY);
        MPU6050_Init(&mpu);
        xSemaphoreGive(xMutexI2C);

        if (mpu.init_ok) {
            xSemaphoreTake(xMutexI2C, portMAX_DELAY);
            OLED_ShowString(0, 4, "DON'T MOVE !!  ");
            OLED_Refresh();
            MPU6050_Calibrate();
            OLED_ShowString(0, 4, "               ");
            OLED_Refresh();
            xSemaphoreGive(xMutexI2C);
            vTaskDelay(pdMS_TO_TICKS(600));
        }
        s_mpu_last = mpu;
    }

    // 4. 初始化 g_* 共享变量
    xSemaphoreTake(xMutexSensor, portMAX_DELAY);
    g_mpu      = s_mpu_last;
    g_distance = -1.0f;
    g_temp     = 25.0f;
    g_zone     = 4;
    g_tilt     = 0;
    xSemaphoreGive(xMutexSensor);

    // 5. 启动完成 → 切到主界面 (DisplayTask 会接管 OLED)
    xSemaphoreTake(xMutexI2C, portMAX_DELAY);
    OLED_Clear();
    OLED_ShowString(0, 0, "Reverse Radar  ");
    OLED_ShowString(0, 2, "Dist: ---cm   ");
    OLED_Refresh();
    xSemaphoreGive(xMutexI2C);

    // 7. 自删 (InitTask 完成, 不再消耗 RAM)
    vTaskDelete(NULL);
}

// ====== Task 2: SensorTask — 超声+MPU+温度 → 共享数据 (周期 50ms) ======
static void SensorTask(void *arg) {
    (void)arg;
    Mpu6050Result mpu_local;
    memset(&mpu_local, 0, sizeof(mpu_local));
    float temp_local = 25.0f;
    uint8_t temp_counter = 0;

    // 等 InitTask 完成 (粗略等 5s, InitTask 阻塞 ~4s 完成)
    vTaskDelay(pdMS_TO_TICKS(5000));

    for (;;) {
        // A. MPU6050 姿态 (~10ms I2C polling) — 必须持 xMutexI2C, 否则与 DisplayTask OLED_Refresh 抢 hi2c1
        xSemaphoreTake(xMutexI2C, portMAX_DELAY);
        MPU6050_Update(&mpu_local);
        xSemaphoreGive(xMutexI2C);

        // B. 温度: 每 5 次启动一次后台转换 (非阻塞); 每圈检查是否 ready
        if (temp_counter % 5 == 0) {
            ds18b20_start();
        }
        (void)ds18b20_get_if_ready(&temp_local);
        temp_counter++;

        // C. 超声波测距 (5 次单点 + 中值 + 滑窗, 阻塞 ~125ms, 但 SensorTask 单任务)
        float dist_raw = get_distance_with_ir(temp_local);
        float dist = dist_raw;

        uint8_t zone = 4;   // 默认 ERR
        uint8_t tilt = 0;

        if (dist_raw >= 0) {
            // D. 姿态坡度修正: 真实水平距离 = 斜距 × cos(pitch)
            float k = MPU6050_PitchCorrection(mpu_local.pitch_deg);
            dist = dist_raw * k;

            // 报警等级 (与 alarm_update_config 一致, 供 DisplayTask 显示)
            if      (dist < sys_config.danger_dist) zone = 0;
            else if (dist < sys_config.warn_dist)   zone = 1;
            else if (dist < sys_config.safe_dist)   zone = 2;
            else                                    zone = 3;
            if (fabsf(mpu_local.pitch_deg) >= 4.0f || fabsf(mpu_local.roll_deg) >= 5.0f) tilt = 1;
        }

        // E. 写共享数据 (持锁时间极短: 仅赋值)
        xSemaphoreTake(xMutexSensor, portMAX_DELAY);
        g_mpu      = mpu_local;
        g_distance = dist;
        g_temp     = temp_local;
        g_zone     = zone;
        g_tilt     = tilt;
        xSemaphoreGive(xMutexSensor);

        // 50ms 周期 (含上述 ~135ms 阻塞, 实际周期 ~135ms+50ms)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ====== Task 3: AlarmTask — RGB + 蜂鸣器节奏 (周期 10ms) ======
static void AlarmTask(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(5000));

    for (;;) {
        float dist_local;
        xSemaphoreTake(xMutexSensor, portMAX_DELAY);
        dist_local = g_distance;
        xSemaphoreGive(xMutexSensor);

        alarm_update_config(dist_local);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ====== Task 4: EncoderTask — 编码器 polling + 投递事件 (周期 2ms) ======
static void EncoderTask(void *arg) {
    (void)arg;
    for (;;) {
        EncoderEvent event = encoder_read();
        if (event != ENC_NONE) {
            xQueueSend(xQueueEncoder, &event, 0);   // 队列满就丢, 不阻塞
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ====== Task 5: DisplayTask — OLED 显示 + 菜单事件处理 (周期 50ms) ======
static void DisplayTask(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(5000));

    for (;;) {
        // ★ 心跳: LED (PC13) 翻转作为 DisplayTask 心跳, OLED 卡死时 LED 也会停
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

        // 1. 取编码器事件 (非阻塞)
        EncoderEvent event;
        if (xQueueReceive(xQueueEncoder, &event, 0) == pdTRUE) {
            uint8_t old_brightness = sys_config.brightness;
            menu_handle_event(event);
            if (sys_config.brightness != old_brightness) {
                xSemaphoreTake(xMutexI2C, portMAX_DELAY);
                OLED_SetBrightness(sys_config.brightness);
                xSemaphoreGive(xMutexI2C);
            }
        }

        // 2. 读共享数据 (本地拷贝, 减少持锁时间)
        Mpu6050Result mpu_local;
        float dist_local, temp_local;
        uint8_t zone_local, tilt_local;
        xSemaphoreTake(xMutexSensor, portMAX_DELAY);
        mpu_local = g_mpu;
        dist_local = g_distance;
        temp_local = g_temp;
        zone_local = g_zone;
        tilt_local = g_tilt;
        xSemaphoreGive(xMutexSensor);

        // 3. 根据状态显示 — OLED_ShowString 只写本地显存 (不调 I2C), 不需要 I2C 锁;
        //    但 OLED_Refresh / OLED_SetBrightness / menu_display 内部调 I2C, 必须持 xMutexI2C
        char buf[24];
        if (current_state == STATE_MAIN_DISPLAY) {
            if (dist_local >= 0) {
                OLED_ShowString(0, 0, "Reverse Radar  ");
                sprintf(buf, "Dist:%5.1fcm", dist_local);
                OLED_ShowString(0, 2, buf);
                sprintf(buf, "T:%4.1f%cC            ", temp_local, 0xB0);
                OLED_ShowString(0, 4, buf);

                if (mpu_local.init_ok) {
                    const char *slope = "LEVEL ";
                    if      (mpu_local.pitch_deg >  4.0f) slope = "UP   ";
                    else if (mpu_local.pitch_deg < -4.0f) slope = "DOWN ";
                    if      (mpu_local.roll_deg  >  5.0f) slope = "TILT R";
                    else if (mpu_local.roll_deg  < -5.0f) slope = "TILT L";
                    sprintf(buf, "R%+5.1f P%+5.1f ", mpu_local.roll_deg, mpu_local.pitch_deg);
                    OLED_ShowString(0, 5, buf);
                    OLED_ShowString(0, 6, slope);
                } else {
                    OLED_ShowString(0, 5, "R: --  P: --  ");
                    OLED_ShowString(0, 6, "                ");
                }
            } else {
                OLED_ShowString(0, 0, "Reverse Radar  ");
                OLED_ShowString(0, 2, "Dist: Error    ");
                OLED_ShowString(0, 4, "                ");
                if (mpu_local.init_ok) {
                    sprintf(buf, "R%+5.1f P%+5.1f ", mpu_local.roll_deg, mpu_local.pitch_deg);
                    OLED_ShowString(0, 5, buf);
                    const char *slope = "LEVEL ";
                    if      (mpu_local.pitch_deg >  4.0f) slope = "UP   ";
                    else if (mpu_local.pitch_deg < -4.0f) slope = "DOWN ";
                    if      (mpu_local.roll_deg  >  5.0f) slope = "TILT R";
                    else if (mpu_local.roll_deg  < -5.0f) slope = "TILT L";
                    OLED_ShowString(0, 6, slope);
                } else {
                    OLED_ShowString(0, 5, "                ");
                    OLED_ShowString(0, 6, "                ");
                }
                RGB_SetColor(0, 0, 0);
                Buzzer_Off();
            }

            // Status 行
            {
                const char *lvl = "--";
                if      (zone_local == 0) lvl = "EMG";
                else if (zone_local == 1) lvl = "DNG";
                else if (zone_local == 2) lvl = "WRN";
                else if (zone_local == 3) lvl = "SAFE";
                if (tilt_local)  sprintf(buf, "Status:%s TILT ", lvl);
                else              sprintf(buf, "Status:%s      ", lvl);
                for (int i = (int)strlen(buf); i < 16; i++) buf[i] = ' ';
                buf[16] = '\0';
                OLED_ShowString(0, 7, buf);
            }
            // OLED_Refresh 调 I2C, 必须持 xMutexI2C
            xSemaphoreTake(xMutexI2C, portMAX_DELAY);
            OLED_Refresh();
            xSemaphoreGive(xMutexI2C);
        } else {
            // 菜单界面 (menu_display 内部 OLED_Refresh 调 I2C, 也要持锁)
            xSemaphoreTake(xMutexI2C, portMAX_DELAY);
            menu_display();
            xSemaphoreGive(xMutexI2C);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
//  FreeRTOS Hook 函数
// ============================================================================

void vApplicationMallocFailedHook(void) {
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

// ============================================================================
//  应用入口 (main.c::USER CODE BEGIN 2 调用)
// ============================================================================

// app_init: sys_config 默认值 + RTOS 对象创建 + 5 任务创建
// 由 main() 在硬件初始化完毕后调用
void app_init(void) {
    // 系统参数默认值 (纯 RAM 模式: 断电不记忆)
    sys_config.safe_dist     = 50.0f;
    sys_config.warn_dist     = 30.0f;
    sys_config.danger_dist   = 15.0f;
    sys_config.buzzer_enable = 1;
    sys_config.brightness    = 8;

    current_state = STATE_MAIN_DISPLAY;

    // RTOS 对象: 1 个队列 + 3 个互斥锁 (I2C 互斥锁防 OLED/MPU6050 抢总线)
    xQueueEncoder   = xQueueCreate(8, sizeof(EncoderEvent));
    xMutexSysConfig = xSemaphoreCreateMutex();
    xMutexSensor    = xSemaphoreCreateMutex();
    xMutexI2C       = xSemaphoreCreateMutex();

    // 创建 5 个任务
    (void)xTaskCreate(InitTask,    "Init",    512, NULL, 6, NULL);
    (void)xTaskCreate(SensorTask,  "Sensor",  512, NULL, 5, NULL);
    (void)xTaskCreate(AlarmTask,   "Alarm",   256, NULL, 4, NULL);
    (void)xTaskCreate(EncoderTask, "Encoder", 256, NULL, 4, NULL);
    (void)xTaskCreate(DisplayTask, "Display", 384, NULL, 2, NULL);
}

// app_start: 启动调度器 (永不返回), 失败则 Error_Handler
void app_start(void) {
    vTaskStartScheduler();

    // 仅当 heap 不足或 scheduler 启动失败才到这
    OLED_ShowString(0, 4, "SCHED FAILED");
    OLED_Refresh();
    Error_Handler();
}
