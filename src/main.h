#pragma once

// 72000000 / 1000 = 72 khz
#define PWM_PERIOD 1000

// За полный оборот насчитывает примерно 100 импульсов
#define ENCODER_MAX 100

// Если счётчик энкодера изменился на это и более значение, то считает это переполнением
#define ENCODER_OVERFLOW_MAX 90

// Максимальная безопасная температура
#define MAX_SAFE_TEMP 50

// Размер флеш
#define FLASH_PAGE_SZIE 1024
#define FLASH_PAGE_COUNT 64

// Адрес для сохранения уровня яркости
#define CONFIG_PWM_ADDR (FLASH_BASE + (FLASH_PAGE_COUNT - 1) * FLASH_PAGE_SZIE)

#include <stm32f10x.h>
#include <math.h>
#include <stdlib.h>
#include "printf.h"
#include "onewire.h"

void usart1_init(uint32_t baudrate);
void uart_putc(void *p, char c);

void ADC1_2_IRQHandler();
void TIM1_BRK_IRQHandler();
void TIM3_IRQHandler();
void SysTick_Handler();
void EXTI0_IRQHandler();

void init_gpio();
void init_encoder(int max);
void timer_init();
void pwm_init(int divider, uint16_t period, uint16_t pulse);
void recalc_pwm_value();
int get_last_pwm();
uint8_t crc8(const uint8_t *data, uint8_t len);
