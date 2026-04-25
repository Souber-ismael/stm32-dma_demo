#include "main.h"
#include "stm32f1xx.h"

DMA_HandleTypeDef  hdma_memtomem;

#define BUF_SIZE 16

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void led_on(void);
static void led_blink_fast(void);

/* Buffers are uint32_t because the DMA is configured WORD (32-bit) aligned */
static const uint32_t src[BUF_SIZE] = {
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    0xAA, 0xBB, 0xCC, 0xDD,
    0x11, 0x22, 0x33, 0x44
};

static uint32_t dst[BUF_SIZE];

static volatile uint8_t dma_done = 0;

/* -----------------------------------------------------------------------
 * DMA transfer-complete callback.
 * HAL_DMA_Start_IT() stores this pointer in hdma->XferCpltCallback and
 * calls it from HAL_DMA_IRQHandler() when the TC flag fires.
 * The global weak HAL_DMA_XferCpltCallback() does NOT exist in STM32F1
 * HAL — you must assign the callback to the handle directly.
 * ----------------------------------------------------------------------- */
static void DMA_XferCplt(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    dma_done = 1;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();        /* must come before any peripheral that uses DMA */
   

    /* Register the completion callback on the handle */
    hdma_memtomem.XferCpltCallback = DMA_XferCplt;

    /* Start mem-to-mem DMA transfer with interrupt */
    if (HAL_DMA_Start_IT(&hdma_memtomem,
                         (uint32_t)src,
                         (uint32_t)dst,
                         BUF_SIZE) != HAL_OK)
    {
        led_blink_fast();   /* should never happen */
    }

    /* Wait for DMA completion (set in DMA_XferCplt via IRQ) */
    while (!dma_done);

    /* Verify the copy */
    uint8_t ok = 1;
    for (uint16_t i = 0; i < BUF_SIZE; i++)
    {
        if (dst[i] != src[i])
        {
            ok = 0;
            break;
        }
    }

    if (ok)
        led_on();           /* solid LED = success */
    else
        led_blink_fast();   /* fast blink = mismatch */

    while (1);  /* never reached */
}

/* -----------------------------------------------------------------------
 * IRQ handler — routes the DMA1 Ch1 interrupt to the HAL handler,
 * which reads the flags and calls hdma->XferCpltCallback (= DMA_XferCplt).
 * ----------------------------------------------------------------------- */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_memtomem);
}

static void led_on(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    while (1);
}

static void led_blink_fast(void)
{
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(100);
    }
}

/* -----------------------------------------------------------------------
 * System Clock: HSI → PLL ×16 → 64 MHz
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * DMA init — only the mem-to-mem channel (DMA1 Ch1)
 * ----------------------------------------------------------------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_memtomem.Instance                 = DMA1_Channel1;
    hdma_memtomem.Init.Direction           = DMA_MEMORY_TO_MEMORY;
    hdma_memtomem.Init.PeriphInc           = DMA_PINC_ENABLE;      /* source increment      */
    hdma_memtomem.Init.MemInc              = DMA_MINC_ENABLE;      /* destination increment */
    hdma_memtomem.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;  /* 32-bit words          */
    hdma_memtomem.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    hdma_memtomem.Init.Mode                = DMA_NORMAL;
    hdma_memtomem.Init.Priority            = DMA_PRIORITY_HIGH;

    if (HAL_DMA_Init(&hdma_memtomem) != HAL_OK)
        Error_Handler();

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}






static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

    /* User button */
    GPIO_InitStruct.Pin  = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* USART1 TX/RX */
    GPIO_InitStruct.Pin   = USART_TX_Pin | USART_RX_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* LED PA5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB12 output */
    GPIO_InitStruct.Pin   = GPIO_PIN_12;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PA0 external interrupt */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
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
