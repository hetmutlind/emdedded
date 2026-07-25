#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

#define BTN_PORT        GPIOC
#define BTN_PIN         GPIO_PIN_13

#define LED_PORT        GPIOB
#define LED_PIN         GPIO_PIN_2

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

void SystemClock_Config(void);

#endif // __MAIN_H
