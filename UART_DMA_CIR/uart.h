/*
 * uart.h
 *
 * UART TX driver for USART1 on STM32F1 — HAL DMA mode.
 * All send functions are non-blocking: the DMA transfers the data
 * while the CPU continues.  The caller must ensure the buffer stays
 * valid until the transfer completes (check UART_TX_busy()).
 *
 * DMA channel mapping on STM32F1 (fixed in silicon):
 *   DMA1 Channel 4 → USART1 TX   (used here)
 *   DMA1 Channel 5 → USART1 RX
 */

#ifndef UART_H
#define UART_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/*
 * UART_TX_Init
 * Configure DMA1 Channel 4 for USART1 TX and link it to huart1.
 * Must be called once, after HAL_UART_Init() and after
 * __HAL_RCC_DMA1_CLK_ENABLE().
 */
void UART_TX_Init(UART_HandleTypeDef *huart);

/*
 * UART_TX_busy
 * Returns 1 while a DMA TX transfer is in progress, 0 when idle.
 * Use this before starting a new transfer if the buffer may still
 * be in use.
 */
uint8_t UART_TX_busy(UART_HandleTypeDef *huart);

/*
 * UART_TX_send
 * Start a non-blocking DMA transfer of `len` bytes from `buf`.
 * `buf` must remain valid until UART_TX_busy() returns 0.
 */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len);

/*
 * UART_TX_send_string
 * Send a null-terminated string via DMA.
 * Uses an internal static buffer — safe for string literals and
 * short messages.  Not re-entrant: wait for UART_TX_busy() == 0
 * before calling again.
 */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str);

#endif /* UART_H */
