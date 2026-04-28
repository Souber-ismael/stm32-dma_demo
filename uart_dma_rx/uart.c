/*
 * uart.c
 *
 * Bare metal UART driver for USART1 on STM32F1.
 * Operates directly on USART1->SR and USART1->DR registers.
 *
 * DMA TX uses DMA1 Channel 4 (USART1_TX on STM32F1).
 * Call UART_DMA_Init() once before using any DMA send functions.
 */

#include "uart.h"

void UART_BM_send_byte(uint8_t data)
{
    /* Wait until transmit data register is empty (TXE = 1) */
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = data;
}

void UART_BM_send_string(const char *str)
{
    while (*str)
        UART_BM_send_byte((uint8_t)*str++);

    /* Wait until transmission is fully complete (TC = 1)
     * before returning — prevents data loss on back-to-back calls */
    while (!(USART1->SR & USART_SR_TC));
}

void UART_BM_send_int(int value)
{
    char buffer[10];
    int  i = 0;

    if (value == 0)
    {
        UART_BM_send_byte('0');
        return;
    }

    if (value < 0)
    {
        UART_BM_send_byte('-');
        value = -value;
    }

    /* Build digits in reverse order */
    while (value > 0)
    {
        buffer[i++] = (char)((value % 10) + '0');
        value /= 10;
    }

    /* Send digits in correct order */
    while (i--)
        UART_BM_send_byte((uint8_t)buffer[i]);
}

/*
 * Helper — sends a 2-digit fractional part with leading zero if needed.
 * Example: 5 → "05", 36 → "36"
 */
static void send_frac(int frac)
{
    if (frac < 10)
        UART_BM_send_byte('0');
    UART_BM_send_int(frac);
}

void UART_BM_send_aht20(int t_int, int t_frac, int h_int, int h_frac)
{
    UART_BM_send_string("AHT | Temp: ");
    UART_BM_send_int(t_int);
    UART_BM_send_byte('.');
    send_frac(t_frac);

    UART_BM_send_string(" C | Hum: ");
    UART_BM_send_int(h_int);
    UART_BM_send_byte('.');
    send_frac(h_frac);

    UART_BM_send_string(" %\r\n");
}

void UART_BM_send_bmp280(int t_int, int t_frac, int p_int, int p_frac)
{
    UART_BM_send_string("BMP | Temp: ");
    UART_BM_send_int(t_int);
    UART_BM_send_byte('.');
    send_frac(t_frac);

    UART_BM_send_string(" C | Press: ");
    UART_BM_send_int(p_int);
    UART_BM_send_byte('.');
    send_frac(p_frac);

    UART_BM_send_string(" hPa\r\n");
}

