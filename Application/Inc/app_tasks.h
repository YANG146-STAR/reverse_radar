#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "app_menu.h"          /* EncoderEvent (队列元素类型) */
#include "app_config.h"        /* SysConfig + 共享数据 */

/* ====== RTOS 对象 (定义在 app_tasks.c, extern 暴露给应用层) ====== */
extern QueueHandle_t      xQueueEncoder;       /* EncoderEvent 投递通道 */
extern SemaphoreHandle_t xMutexSysConfig;      /* sys_config 读写保护 */
extern SemaphoreHandle_t xMutexSensor;         /* g_distance/g_temp/g_mpu/g_zone/g_tilt 保护 */
extern SemaphoreHandle_t xMutexI2C;             /* I2C1 总线互斥 (OLED + MPU6050 共用 hi2c1) */

/* ====== 应用入口 (main.c::USER CODE BEGIN 2 调用) ======
   app_init: sys_config 默认值赋值 + RTOS 对象创建 + 5 任务创建
   app_start: vTaskStartScheduler (失败走 Error_Handler) */
void app_init(void);
void app_start(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_TASKS_H */
