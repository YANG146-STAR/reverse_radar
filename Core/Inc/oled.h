#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_Refresh(void);
void OLED_SetBrightness(uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
