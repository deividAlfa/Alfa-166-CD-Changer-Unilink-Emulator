/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "config.h"
#include "files.h"
#include "unilink.h"
#include "gpio.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern TIM_HandleTypeDef htim10;
extern TIM_HandleTypeDef htim11;
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */


#define ff_malloc  _malloc
#define ff_free  _free

#ifdef DEBUG_ALLOC
extern uint32_t        max_allocated;
extern struct mallinfo mi;

#define _malloc(x)    malloc(x); debug_heap()
#define _calloc(x,y)  calloc(x,y); debug_heap()
#define _free(x)      free(x); debug_heap()

void debug_heap(void);

#else
#define _malloc     malloc
#define _calloc     calloc
#define _free       free
#endif

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void DebugPulse(uint8_t pulses);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define BTN_Pin GPIO_PIN_0
#define BTN_GPIO_Port GPIOA
#define UNILINK_CLOCK_Pin GPIO_PIN_5
#define UNILINK_CLOCK_GPIO_Port GPIOA
#define UNILINK_DATA_Pin GPIO_PIN_6
#define UNILINK_DATA_GPIO_Port GPIOA
#define BCK_Pin GPIO_PIN_0
#define BCK_GPIO_Port GPIOB
#define LCK_Pin GPIO_PIN_1
#define LCK_GPIO_Port GPIOB
#define BT_PWR_Pin GPIO_PIN_2
#define BT_PWR_GPIO_Port GPIOB
#define LED3_Pin GPIO_PIN_10
#define LED3_GPIO_Port GPIOB
#define BT_ON_Pin GPIO_PIN_12
#define BT_ON_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_13
#define LED2_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_14
#define LED1_GPIO_Port GPIOB
#define PLAY_Pin GPIO_PIN_15
#define PLAY_GPIO_Port GPIOB
#define STOP_Pin GPIO_PIN_8
#define STOP_GPIO_Port GPIOA
#define PREV_Pin GPIO_PIN_9
#define PREV_GPIO_Port GPIOA
#define NEXT_Pin GPIO_PIN_10
#define NEXT_GPIO_Port GPIOA
#define DOUT_Pin GPIO_PIN_8
#define DOUT_GPIO_Port GPIOB
#define PWR_ON_Pin GPIO_PIN_9
#define PWR_ON_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define LED_Pin_Pos   __get_GPIO_Pos(LED_Pin)
#define VBUS_Pin_Pos  __get_GPIO_Pos(VBUS_Pin)
#define SCK_Pin_Pos   __get_GPIO_Pos(SCK_Pin)
#define DATA_Pin_Pos  __get_GPIO_Pos(DATA_Pin)
#define TEST_Pin_Pos  __get_GPIO_Pos(TEST_Pin)
#define BREAK_Pin_Pos __get_GPIO_Pos(BREAK_Pin)
#define PREV_Pin_Pos  __get_GPIO_Pos(PREV_Pin)
#define NEXT_Pin_Pos  __get_GPIO_Pos(NEXT_Pin)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
