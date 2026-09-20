#include "app_ultrasonic.h"
#include "app_outputs.h"     /* delay_us */
#include "tim.h"             /* htim4 */
#include "filter.h"          /* filter_median / FilterMovingAvg / filter_avg_init / filter_avg_update */
#include <string.h>

#define FILTER_SIZE 5

// HC-SR04 量程 2cm~400cm: Echo 高电平 150us~23.5ms
#define ECHO_RISE_TIMEOUT_US  10000u   // 等 Echo 拉高超时 10ms
#define ECHO_HIGH_TIMEOUT_US  25000u   // Echo 高电平超时 25ms (>400cm 对应 23.5ms)

// ====== 超声波单次原始计时 (GPIO 轮询 Echo + TIM4 1MHz 计数超时) ======
static float ultrasonic_single_raw(float temp_c) {
    float sound_speed = 331.4f + 0.6f * temp_c;

    HAL_GPIO_WritePin(Trig_GPIO_Port, Trig_Pin, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(Trig_GPIO_Port, Trig_Pin, GPIO_PIN_RESET);

    // 等 Echo 拉高: TIM4 计数超时
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == GPIO_PIN_RESET) {
        if (__HAL_TIM_GET_COUNTER(&htim4) > ECHO_RISE_TIMEOUT_US) return -1.0f;
    }

    __HAL_TIM_SET_COUNTER(&htim4, 0);
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == GPIO_PIN_SET) {
        if (__HAL_TIM_GET_COUNTER(&htim4) > ECHO_HIGH_TIMEOUT_US) return -2.0f;
    }
    uint32_t end = __HAL_TIM_GET_COUNTER(&htim4);

    float distance = (sound_speed * (float)end) / 20000.0f;
    return distance;
}

// ====== 超声波测距 (5 次单点 + 中值 + 5 点滑动平均, 失败用最近有效值填补) ======
float ultrasonic_get_distance(float temp_c) {
    static float s_avg_buf[FILTER_SIZE];   // 滑动平均环形缓冲
    static FilterMovingAvg s_avg;          // 滑动平均滤波器状态 (只初始化 1 次)
    static uint8_t         s_avg_inited = 0;
    static float           s_last_valid = -1.0f;  // 最近一次有效 raw 样本 (填补失败样本)
    static float           s_last_avg   = -1.0f;  // 上一次有效滑动均值 (本帧全失败时保持)

    if (!s_avg_inited) {
        filter_avg_init(&s_avg, s_avg_buf, FILTER_SIZE);
        s_avg_inited = 1;
    }

    // 第一层: 采 5 次, 凑齐 5 个样本 (失败位置用最近一次有效值填补)
    float samples[FILTER_SIZE];
    uint8_t ok_this_frame = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        float v = ultrasonic_single_raw(temp_c);
        if (v >= 0.0f) {
            samples[i] = v;
            s_last_valid = v;
            ok_this_frame = 1;
        } else {
            samples[i] = s_last_valid;
        }
    }

    if (!ok_this_frame) {
        return s_last_avg;   // 第一次就全失败: 返回 -1 = 测距失败
    }

    // 第二层: 中值滤波 + 滑动平均
    float med = filter_median(samples, FILTER_SIZE);
    if (med < 0.0f) med = 0.0f;
    float avg = filter_avg_update(&s_avg, med);
    s_last_avg = avg;
    return avg;
}

// ====== 红外补盲 + 超声波融合 (红外触发直接返回盲区最小值) ======
float get_distance_with_ir(float temp) {
    if (ir_obstacle_detected()) {
        return 2.0f;
    }
    return ultrasonic_get_distance(temp);
}
