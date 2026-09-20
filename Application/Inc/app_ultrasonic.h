#ifndef __APP_ULTRASONIC_H
#define __APP_ULTRASONIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ====== 超声波测距 (单层 + 5 点中值 + 5 点滑动平均, 失败用上次值填补) ====== */
float ultrasonic_get_distance(float temp_c);

/* ====== 红外避障 + 超声波融合: 红外触发直接返回 2.0 (盲区), 否则调 ultrasonic ====== */
float get_distance_with_ir(float temp_c);

#ifdef __cplusplus
}
#endif
#endif /* __APP_ULTRASONIC_H */
