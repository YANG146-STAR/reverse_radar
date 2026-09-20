#ifndef __DS18B20_H
#define __DS18B20_H

#include "main.h"

uint8_t DS18B20_Reset(void);
void DS18B20_WriteByte(uint8_t data);
uint8_t DS18B20_ReadByte(void);
void DS18B20_StartConvert(void);
float DS18B20_ReadTemp(void);
uint8_t DS18B20_ReadTempChecked(float *out_temp);   // 带 CRC 校验的读取（推荐）
float ds18b20_get_temp(void);

// ========== 非阻塞接口（新！去掉 HAL_Delay(750) 的阻塞等待）==========
// ds18b20_start()   : 触发一次温度转换（立即返回，~1.5ms）
// ds18b20_get_if_ready(float *out) : 如果距离 start 过 750ms 就读温度写到 *out，返回 1；否则返回 0 不写
// 用法：每 5 次测距调一次 start(); 主循环每圈调一次 get_if_ready()，拿不到就用上一次温度。
void  ds18b20_start(void);
uint8_t ds18b20_get_if_ready(float *out_temp_c);

#endif
