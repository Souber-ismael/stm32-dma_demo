/*
 * uart.h
 *
 * Full-duplex UART driver for USART1 on STM32F1 — HAL DMA mode.
 *
 * TX : DMA1 Channel 4 → USART1 TX  (hardware-fixed mapping)
 * RX : DMA1 Channel 5 → USART1 RX  (hardware-fixed mapping)
 *      Circular mode + IDLE interrupt for variable-length frames.
 *
 * Required initialisation order in main():
 *   1. MX_DMA_Init()          — enable DMA1 clock, configure Ch5 RX
 *   2. MX_USART1_UART_Init()  — configure USART1 peripheral
 *   3. UART_TX_Init()         — configure Ch4 TX, link hdmatx
 *   4. UART_Driver_Init()     — arm circular RX DMA, enable IDLE IRQ
 */

#ifndef INC_UART_H_
#define INC_UART_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Size of the circular DMA RX buffer.
 * Must be >= the longest expected frame. */
#define UART_RX_BUF_SIZE  64

/* Size of the internal static TX buffer used by UART_TX_send_string(). */
#define UART_TX_BUF_SIZE  128

/* Configure DMA1 Ch4 for USART1 TX and link it to the UART handle.
 * Call after MX_DMA_Init() and MX_USART1_UART_Init(). */
void UART_TX_Init(UART_HandleTypeDef *huart);

/* Arm the circular DMA RX and enable the IDLE interrupt.
 * Call after UART_TX_Init(). */
void UART_Driver_Init(UART_HandleTypeDef *huart);

/* Send `len` bytes from `buf` via DMA TX.
 * Blocks until any previous transfer finishes, then starts a new one.
 * `buf` must remain valid until HAL_UART_TxCpltCallback() fires. */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len);

/* Copy `str` into an internal static buffer and send via DMA TX.
 * Waits before the copy so the buffer is not overwritten mid-transfer.
 * Maximum length: UART_TX_BUF_SIZE - 1 characters. */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str);

/* Call in the main while(1) loop.
 * When rx_ready == 1 (set by IDLE callback), reconstructs a line from
 * rx_buffer, echoes it back over TX, then re-arms the DMA RX. */
void UART_RX_Process(void);

/* Called from USART1_IRQHandler when the IDLE flag fires.
 * Sets rx_ready = 1 to signal that a frame is available in rx_buffer. */
void UART_IDLE_Callback(UART_HandleTypeDef *huart);

/* HAL callback — fires when a DMA TX transfer completes.
 * Sets tx_done = 1 to unblock the next send call. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INC_UART_H_ */
