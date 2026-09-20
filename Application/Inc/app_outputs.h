#ifndef __APP_OUTPUTS_H
#define __APP_OUTPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ====== 硬件延时 (TIM4 Base 模式 + HAL_Delay) ====== */
void delay_us(uint16_t us);
void delay_ms(uint32_t ms);

/* ====== RGB LED 控制 (共阳, 低电平点亮) ====== */
void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b);

/* ====== 蜂鸣器控制 (低电平触发) ====== */
void Buzzer_On(void);
void Buzzer_Off(void);

/* ====== 红外避障检测 (PB13, 低电平=检测到障碍) ====== */
uint8_t ir_obstacle_detected(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OUTPUTS_H */
