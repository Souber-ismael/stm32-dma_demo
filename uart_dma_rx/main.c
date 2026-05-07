#include "main.h"
#include "stm32f1xx.h"
#include "uart.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;

/*
 * UART DMA RX — normal (one-shot) mode
 * STM32F1 — USART1 RX → DMA1 Channel5 → rx_buffer
 *
 * The DMA transfers exactly RX_BUF_SIZE bytes then stops.
 * HAL_UART_RxCpltCallback() fires when the transfer is complete
 * and immediately re-arms the DMA for the next packet.
 *
 * Use this mode when every packet has a fixed, known length.
 * For variable-length packets use the circular + IDLE project
 * (UART_DMA_CIR) instead.
 *
 * DMA channel mapping on STM32F1 (fixed in silicon):
 *   DMA1 Channel 4 → USART1 TX
 *   DMA1 Channel 5 → USART1 RX
 */

#define RX_BUF_SIZE 12

static uint8_t rx_buffer[RX_BUF_SIZE];

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();           /* DMA clock must be enabled before any
                                peripheral that uses DMA is initialised */
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_SPI2_Init();

    UART_TX_Init(&huart1);

    /* Arm the first reception — the callback re-arms it automatically */
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE);

    UART_TX_send_string(&huart1, "READY\r\n");

    while (1)
    {
        /* All RX processing happens inside HAL_UART_RxCpltCallback().
         * The CPU is free here for application logic. */
    }
}

/* ----------------------------------------------------------------
 * HAL_UART_RxCpltCallback
 *
 * Called by HAL when the DMA has transferred exactly RX_BUF_SIZE
 * bytes into rx_buffer.  Process the data, then re-arm the DMA
 * so the next packet can be received.
 * ---------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

    /* Echo the received packet back over TX */
    UART_TX_send(&huart1, rx_buffer, RX_BUF_SIZE);

    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  /* heartbeat LED */

    /* Re-arm DMA for the next fixed-size packet */
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE);
}

/* ----------------------------------------------------------------
 * USART1 IRQ Handler — delegates entirely to HAL.
 * HAL_UART_RxCpltCallback() is called from here when TC fires.
 * ---------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* ----------------------------------------------------------------
 * USART1 Init — 9600 8N1, TX+RX, no flow control
 * ---------------------------------------------------------------- */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 9600;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();

    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ----------------------------------------------------------------
 * DMA1 Init — Channel 5 for USART1 RX in normal (one-shot) mode.
 * The DMA stops after RX_BUF_SIZE bytes and raises TC interrupt.
 * HAL_UART_RxCpltCallback() must call HAL_UART_Receive_DMA()
 * again to receive the next packet.
 * Channel 4 (USART1 TX) is configured in UART_TX_Init().
 * ---------------------------------------------------------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;   /* UART DR is one fixed register */
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;    /* advance through rx_buffer     */
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_NORMAL;         /* stops after RX_BUF_SIZE bytes */
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;

    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        Error_Handler();

    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/* ----------------------------------------------------------------
 * DMA IRQ Handlers
 * ---------------------------------------------------------------- */
void DMA1_Channel4_IRQHandler(void)   /* USART1 TX */
{
    HAL_DMA_IRQHandler(huart1.hdmatx);
}

void DMA1_Channel5_IRQHandler(void)   /* USART1 RX */
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}
