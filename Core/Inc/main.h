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
#include "stm32h7xx_hal.h"

#include "stm32h7xx_nucleo.h"
#include <stdio.h>

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
#define MOTOR_OUT_4_Pin GPIO_PIN_3
#define MOTOR_OUT_4_GPIO_Port GPIOF
#define Encoder_Button_Interrupt_Pin GPIO_PIN_5
#define Encoder_Button_Interrupt_GPIO_Port GPIOA
#define Encoder_Button_Interrupt_EXTI_IRQn EXTI9_5_IRQn
#define Encoder_DT_Timer_Pin GPIO_PIN_6
#define Encoder_DT_Timer_GPIO_Port GPIOA
#define MOTOR_OUT_2_Pin GPIO_PIN_11
#define MOTOR_OUT_2_GPIO_Port GPIOE
#define MOTOR_OUT_1_Pin GPIO_PIN_14
#define MOTOR_OUT_1_GPIO_Port GPIOE
#define MOTOR_OUT_3_Pin GPIO_PIN_12
#define MOTOR_OUT_3_GPIO_Port GPIOG
#define Adafruit_IR_Sensor_Pin GPIO_PIN_14
#define Adafruit_IR_Sensor_GPIO_Port GPIOG
#define Adafruit_IR_Sensor_EXTI_IRQn EXTI15_10_IRQn
#define Encoder_CLK_Timer_Pin GPIO_PIN_5
#define Encoder_CLK_Timer_GPIO_Port GPIOB
#define LCD_SCL_Pin GPIO_PIN_8
#define LCD_SCL_GPIO_Port GPIOB
#define LCD_SDA_Pin GPIO_PIN_9
#define LCD_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
