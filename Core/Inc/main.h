/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define DS18B20_Pin GPIO_PIN_1
#define DS18B20_GPIO_Port GPIOA
#define LED_R_Pin GPIO_PIN_2
#define LED_R_GPIO_Port GPIOA
#define ENC_CLK_Pin GPIO_PIN_0
#define ENC_CLK_GPIO_Port GPIOB
#define ENC_SW_Pin GPIO_PIN_1
#define ENC_SW_GPIO_Port GPIOB
#define ENC_DT_Pin GPIO_PIN_12
#define ENC_DT_GPIO_Port GPIOB
#define IR_Pin GPIO_PIN_13
#define IR_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_8
#define BUZZER_GPIO_Port GPIOA
#define LED_G_Pin GPIO_PIN_15
#define LED_G_GPIO_Port GPIOA
#define LED_B_Pin GPIO_PIN_3
#define LED_B_GPIO_Port GPIOB
#define Echo_Pin GPIO_PIN_8
#define Echo_GPIO_Port GPIOB
#define Trig_Pin GPIO_PIN_9
#define Trig_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
// ====== 系统参数（必须 __packed，main.c 里直接存取）======
typedef __packed struct {
    float safe_dist;
    float warn_dist;
    float danger_dist;
    uint8_t buzzer_enable;
    uint8_t brightness;
} SysConfig;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
