#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"           /* SysConfig typedef + 引脚定义 */
#include "mpu6050.h"        /* Mpu6050Result typedef */

/* ====== 共享数据 (定义在 app_tasks.c 顶部, 任意 .c include 本文件即可访问) ====== */
extern SysConfig       sys_config;       /* 系统配置参数 */
extern Mpu6050Result   g_mpu;            /* MPU6050 姿态最新结果 */
extern float           g_distance;       /* 距离 cm, -1=测距失败 */
extern float           g_temp;           /* 温度 °C */
extern uint8_t         g_zone;           /* 报警区: 0=EMG 1=DNG 2=WRN 3=SAFE 4=ERR */
extern uint8_t         g_tilt;           /* 倾斜告警 0/1 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_CONFIG_H */
