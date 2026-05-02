/*
 * uart.c
 *
 * UART TX driver for USART1 on STM32F1 — HAL DMA mode.
 * DMA1 Channel 4 is the USART1 TX channel (fixed by hardware).
 */

#include "uart.h"

/* Internal DMA handle for USART1 TX */
static DMA_HandleTypeDef hdma_tx;

/* ----------------------------------------------------------------
 * UART_TX_Init
 * Configure DMA1 Channel 4 for USART1 TX in normal (one-shot) mode
 * and link it to the UART handle so HAL_UART_Transmit_DMA() can use it.
 * ---------------------------------------------------------------- */
void UART_TX_Init(UART_HandleTypeDef *huart)
{
    hdma_tx.Instance                 = DMA1_Channel4;   /* USART1 TX — fixed by hardware */
    hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;   /* UART DR is one fixed register */
    hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;    /* advance through source buffer */
    hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx.Init.Mode                = DMA_NORMAL;         /* one transfer per call         */
    hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;

    if (HAL_DMA_Init(&hdma_tx) != HAL_OK)
        return;

    /* Link TX DMA handle to the UART so HAL_UART_Transmit_DMA() finds it */
    __HAL_LINKDMA(huart, hdmatx, hdma_tx);

    /* Enable DMA IRQ — HAL needs it to clear flags and signal completion */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

/* ----------------------------------------------------------------
 * UART_TX_busy
 * Returns 1 while a transfer is running, 0 when the channel is free.
 * ---------------------------------------------------------------- */
uint8_t UART_TX_busy(UART_HandleTypeDef *huart)
{
    return (HAL_UART_GetState(huart) & HAL_UART_STATE_BUSY_TX) != 0;
}

/* ----------------------------------------------------------------
 * UART_TX_send
 * Start a non-blocking DMA transfer.
 * The caller is responsible for keeping `buf` valid until the
 * transfer completes (UART_TX_busy() returns 0).
 * ---------------------------------------------------------------- */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len)
{
    /* Wait for any previous transfer to finish before starting a new one */
    while (UART_TX_busy(huart));
    HAL_UART_Transmit_DMA(huart, (uint8_t *)buf, len);
}

/* ----------------------------------------------------------------
 * UART_TX_send_string
 * Copy the string into a static buffer then start a DMA transfer.
 * The static buffer stays valid for the duration of the transfer.
 * Max string length: 128 bytes (including null terminator).
 * ---------------------------------------------------------------- */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str)
{
    static uint8_t tx_buf[128];
    uint16_t len = (uint16_t)strlen(str);

    if (len == 0 || len >= sizeof(tx_buf))
        return;

    memcpy(tx_buf, str, len);
    UART_TX_send(huart, tx_buf, len);
}
