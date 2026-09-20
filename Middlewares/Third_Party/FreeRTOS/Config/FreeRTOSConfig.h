/*
 * FreeRTOSConfig.h for STM32F103C8T6 reverse_radar project
 * adapted for Keil ARMCC V5.06 + RVDS/ARM_CM3 port
 *
 * 关键约束:
 *   - STM32F103C8 = 64KB Flash / 20KB RAM, heap_4 配 6KB
 *   - 72MHz CPU, SysTick 1ms
 *   - NVIC_PRIORITYGROUP_4 (4-bit preemption, 0-bit sub) — Cortex-M3 port 要求
 *   - 通过 vApplicationTickHook 调 HAL_IncTick 维护 HAL 时间基准
 *   - 通过 #define 把 SVC_Handler/PendSV_Handler/SysTick_Handler 别名给 port.c
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Application specific definitions
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1   /* Cortex-M3 有 CLZ 指令, 启用优化选择 */
#define configUSE_TICKLESS_IDLE                  0   /* 关闭 tickless 以省复杂度 */
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000UL )
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000U )
#define configMAX_PRIORITIES                     ( 7 )            /* 0~6, 数值越大优先级越高 */
#define configMINIMAL_STACK_SIZE                 ( ( unsigned short ) 64 )    /* words, IDLE 任务 = 256 字节 */
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) 10240 )         /* 10KB heap_4 — 容纳 5 任务 + IDLE + Timer + 队列/互斥锁 */
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATION              1
#define configTASK_NOTIFICATION_QUEUE_ENTRIES    3
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_QUEUE_SETS                     0
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  0
#define configSTACK_DEPTH_TYPE                   uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/* Tick type width — must be 0 or 1 (FreeRTOS.h checks with #error if missing) */
#define configUSE_16_BIT_TICKS                   0   /* 32-bit TickType_t, 64 位秒级覆盖 */

/* Includes for API — 必须显式置 1, 否则 tasks.c 里 #if (INCLUDE_xxx == 1) 会跳过函数定义 */
#define INCLUDE_vTaskDelay                       1   /* vTaskDelay */
#define INCLUDE_vTaskDelayUntil                  1   /* vTaskDelayUntil */
#define INCLUDE_vTaskDelete                      1   /* vTaskDelete */
#define INCLUDE_vTaskSuspend                     1   /* vTaskSuspend / vTaskResume */
#define INCLUDE_vTaskSuspendAll                  1   /* vTaskSuspendAll */
#define INCLUDE_vTaskPriorityGet                 1
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_vTaskPriorityInherit              0
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        0
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_xQueueGetMutexHolder             0
#define INCLUDE_uxTaskGetStackHighWaterMark      1   /* 给 configCHECK_FOR_STACK_OVERFLOW=2 用 */
#define INCLUDE_eTaskGetState                    0
#define INCLUDE_xEventGroupSetBitsFromISR        0
#define INCLUDE_xTimerPendFunctionCall           0
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0

/* Memory allocation related */
#define configSUPPORT_STATIC_ALLOCATION         0   /* 仅动态分配, 简化 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* Hook function related */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 方式 2: 检查栈尾蜜罐 + 指针比对 */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Run time and task stats gathering */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0   /* 关闭以省 RAM */
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Co-routine definitions */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          2

/* Software timer related */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 3 )
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            ( 64 )   /* words = 256 字节 */

/* Interrupt nesting behaviour (Cortex-M3 specific) */
/* STM32F1 NVIC 只有 4 个优先级位 (高 4 位有效), 全部用作抢占优先级 (NVIC_PRIORITYGROUP_4) */
#define configPRIO_BITS                         4
/* 最低优先级 = 0xFF (Cortex-M 内核, 不允许调 FreeRTOS API) */
#define configKERNEL_INTERRUPT_PRIORITY         ( 0xFF << ( 8 - configPRIO_BITS ) )  /* = 0xF0u */
/* 优先级值 5~15 可调 FromISR API; 0~4 不允许 (kernel 保留) */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 << ( 8 - configPRIO_BITS ) )    /* = 0x50u */

/* Define to trap memory errors during development */
#define configASSERT( x )                       if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }

/* Optional: 释放堆给 BSP 用的 sem count */
#define configUSE_APPLICATION_TASK_TAG          0

/* Definitions that map the FreeRTOS port interrupt handlers to the CMSIS name */
#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler      PendSV_Handler
/* SysTick_Handler 不在此处接管, 在 stm32f1xx_it.c 自定义:
   - 调度器未启动时调 HAL_IncTick (维持 HAL_Delay/HAL_GetTick 工作)
   - 调度器启动后调 xPortSysTickHandler (FreeRTOS tick + 通过 tick hook 调 HAL_IncTick) */

/* Includes */
#include <stdint.h>
/* 这个头由 STM32 HAL 包提供 (stm32f1xx.h → core_cm3.h), 必须在使用 uint32_t 之前可见 */
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
