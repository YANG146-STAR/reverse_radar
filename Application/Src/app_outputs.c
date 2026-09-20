#include "app_outputs.h"
#include "tim.h"            /* htim1(蜂鸣器PWM) htim2(RGB PWM) htim4(delay_us) */

#define PWM_PERIOD   1000    /* TIM1/TIM2 ARR = 999, 占空比范围 0~1000 */

// ====== 微秒延时 (TIM4 Base 手动计时, 首次启动后常开) ======
// TIM4 为 16 位计数器 @1MHz, us 最大 65535 (~65ms)
void delay_us(uint16_t us) {
    static uint8_t s_tim_started = 0;
    if (!s_tim_started) {          // 首次调用启动一次, 之后一直跑
        HAL_TIM_Base_Start(&htim4);
        s_tim_started = 1;
    }
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    while (__HAL_TIM_GET_COUNTER(&htim4) < us);
}

// ====== 毫秒延时 (给 OLED 驱动用, 基于 HAL_GetTick) ======
void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

// ====== 警报灯 RGB 控制 (共阳, 低电平点亮, GPIO 直接驱动) ======
void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, r ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, g ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, b ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

// ====== 蜂鸣器控制 (低电平触发, TIM1_CH1 PWM) ======
//   占空比 100% = 一直高 = 关;  50% = 方波 = 响
void Buzzer_On(void) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD / 2);  /* 50% 方波 */
}

void Buzzer_Off(void) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD);      /* 100% = 高 = 关 */
}

// ====== 红外避障检测 (PB13, 检测到障碍输出低电平) ======
uint8_t ir_obstacle_detected(void) {
    return HAL_GPIO_ReadPin(IR_GPIO_Port, IR_Pin) == GPIO_PIN_RESET ? 1 : 0;
}
