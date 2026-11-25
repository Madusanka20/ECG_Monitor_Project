#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"

void Error_Handler(void);

#define ADS1293_RSTB_Pin             GPIO_PIN_3
#define ADS1293_RSTB_GPIO_Port       GPIOA

#define ADS1293_CS_Pin               GPIO_PIN_4
#define ADS1293_CS_GPIO_Port         GPIOA

#define SPI1_SCK_Pin                 GPIO_PIN_5
#define SPI1_SCK_GPIO_Port           GPIOA

#define SPI1_MISO_Pin                GPIO_PIN_6
#define SPI1_MISO_GPIO_Port          GPIOA

#define SPI1_MOSI_Pin                GPIO_PIN_7
#define SPI1_MOSI_GPIO_Port          GPIOA

#define ADS1293_DRDYB_Pin            GPIO_PIN_0
#define ADS1293_DRDYB_GPIO_Port      GPIOB
#define ADS1293_DRDYB_EXTI_IRQn      EXTI0_IRQn

#define ADS1293_ALARM_Pin            GPIO_PIN_1
#define ADS1293_ALARM_GPIO_Port      GPIOB

#define BLUENRG_IRQ_Pin              GPIO_PIN_8
#define BLUENRG_IRQ_GPIO_Port        GPIOA
#define BLUENRG_IRQ_EXTI_IRQn        EXTI8_IRQn

#define BLUENRG_CS_Pin               GPIO_PIN_10
#define BLUENRG_CS_GPIO_Port         GPIOA

#define BLUENRG_RESETN_Pin           GPIO_PIN_12
#define BLUENRG_RESETN_GPIO_Port     GPIOB

#ifdef __cplusplus
}
#endif

#endif
