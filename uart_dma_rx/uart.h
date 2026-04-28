/*
 * uart.h
 *
 * Bare metal UART driver for USART1 on STM32F1.
 * Operates directly on registers — no HAL, no LL library calls.
 *
 * Polling API  : UART_BM_*  — blocking, byte-by-byte
 * DMA TX API   : UART_DMA_* — non-blocking, DMA1 Channel 4
 */

#ifndef UART_BM_H
#define UART_BM_H

#include "stm32f1xx.h"
#include <stdint.h>

/* SR register flag masks */
#define USART_SR_TXE   (1U << 7)  /* Transmit data register empty   */
#define USART_SR_TC    (1U << 6)  /* Transmission complete           */
#define USART_SR_RXNE  (1U << 5)  /* Read data register not empty    */

/*
 * Send a single byte over USART1.
 * Blocks until the transmit register is empty before writing.
 */
void UART_BM_send_byte(uint8_t data);

/*
 * Send a null-terminated string over USART1.
 * Blocks until the full string is transmitted.
 */
void UART_BM_send_string(const char *str);

/*
 * Send a signed integer as ASCII digits over USART1.
 * Handles negative values and zero.
 */
void UART_BM_send_int(int value);

/*
 * Send AHT20 temperature and humidity reading over USART1.
 * Format: AHT | Temp: 25.36 C | Hum: 60.12 %
 *
 * t_int  : integer part of temperature
 * t_frac : fractional part of temperature (2 digits)
 * h_int  : integer part of humidity
 * h_frac : fractional part of humidity (2 digits)
 */
void UART_BM_send_aht20(int t_int, int t_frac, int h_int, int h_frac);

/*
 * Send BMP280 temperature and pressure reading over USART1.
 * Format: BMP | Temp: 25.36 C | Press: 1013.25 hPa
 *
 * t_int  : integer part of temperature
 * t_frac : fractional part of temperature (2 digits)
 * p_int  : integer part of pressure
 * p_frac : fractional part of pressure (2 digits)
 */
void UART_BM_send_bmp280(int t_int, int t_frac, int p_int, int p_frac);

/* -----------------------------------------------------------------------
 * DMA TX API — non-blocking, uses DMA1 Channel 4 (USART1_TX)
 * ----------------------------------------------------------------------- */

/*
 * UART_DMA_Init
 * Configure DMA1 Channel 4 for USART1 TX and enable the TC interrupt.
 * Must be called once after USART1 is initialised.
 */
void UART_DMA_Init(void);

/*
 * UART_DMA_busy
 * Returns 1 while a DMA transfer is in progress, 0 when idle.
 */
uint8_t UART_DMA_busy(void);

/*
 * UART_DMA_send
 * Start a non-blocking DMA transfer of `len` bytes from `buf`.
 * `buf` must remain valid until UART_DMA_busy() returns 0.
 */
void UART_DMA_send(const uint8_t *buf, uint16_t len);

/*
 * UART_DMA_send_string
 * Convenience wrapper — sends a null-terminated string via DMA.
 */
void UART_DMA_send_string(const char *str);

#endif /* UART_BM_H */
