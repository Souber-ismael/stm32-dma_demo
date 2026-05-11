/*
 * main.c
 *
 * UART DMA TX + circular RX with IDLE line detection.
 * STM32F103RB (Nucleo-64) — USART1 on PA9/PA10.
 *
 * On startup : sends "READY\r\n" over TX.
 * In the loop: echoes every received line back over TX.
 */

#include "main.h"
#include "uart.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;

/* hdma_tx is defined in uart.c — extern here for the IRQ handler */
extern DMA_HandleTypeDef hdma_tx;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

/* ----------------------------------------------------------------
 * main
 * Initialisation order matters:
 *   DMA clock must be enabled before HAL_UART_Init() links handles.
 *   UART_TX_Init() must run before UART_Driver_Init() arms the RX.
 * ---------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();             /* DMA clock + Ch5 RX config */
    MX_USART1_UART_Init();     /* USART1 peripheral + NVIC  */
    UART_TX_Init(&huart1);     /* Ch4 TX config + link hdmatx */
    UART_Driver_Init(&huart1); /* arm circular RX + enable IDLE */

    UART_TX_send_string(&huart1, "READY\r\n");

    while (1)
    {
        UART_RX_Process(); /* process frames signalled by IDLE */
    }
}

/* ----------------------------------------------------------------
 * USART1_IRQHandler
 * IDLE is checked before HAL_UART_IRQHandler() to avoid HAL
 * clearing flags before we read them.
 * On STM32F1, clearing IDLE requires reading SR then DR —
 * __HAL_UART_CLEAR_IDLEFLAG() performs that sequence.
 * ---------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE))
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        UART_IDLE_Callback(&huart1);
    }
    HAL_UART_IRQHandler(&huart1);
}

/* ----------------------------------------------------------------
 * DMA1_Channel4_IRQHandler — USART1 TX
 * Routes to HAL which clears TC and calls HAL_UART_TxCpltCallback.
 * ---------------------------------------------------------------- */
void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_tx);
}

/* ----------------------------------------------------------------
 * DMA1_Channel5_IRQHandler — USART1 RX
 * In circular mode, IDLE is used for frame detection rather than TC.
 * This handler is still required for HAL DMA error management.
 * ---------------------------------------------------------------- */
void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* ----------------------------------------------------------------
 * MX_USART1_UART_Init — 9600 8N1, TX+RX, no flow control.
 * PA9/PA10 GPIO are configured by HAL_UART_MspInit() in hal_msp.c.
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
 * MX_DMA_Init
 * Enables the DMA1 clock and configures Channel 5 for USART1 RX
 * in circular mode (NDTR reloads automatically — never stops).
 * Channel 4 (TX) is configured separately in UART_TX_Init().
 * ---------------------------------------------------------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;  /* DR is a single register */
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;   /* advance through rx_buffer */
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;      /* auto-reload, never stops */
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;

    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        Error_Handler();

    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/* ----------------------------------------------------------------
 * MX_GPIO_Init
 * LED LD2 on PA5 — output push-pull, starts off.
 * Button B1 on PC13 — EXTI rising edge.
 * PA9/PA10 (USART1) are handled by HAL_UART_MspInit().
 * ---------------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LD2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* Priority 5 — lower than UART (0) and DMA (1) */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* ----------------------------------------------------------------
 * SystemClock_Config — HSI → PLL ×16 → 64 MHz
 * ---------------------------------------------------------------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
