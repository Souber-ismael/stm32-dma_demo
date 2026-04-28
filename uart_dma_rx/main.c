/* =========================================================
 * UART RX via DMA — STM32F1
 * UART1 RX  →  DMA1 Channel5  →  rx_buffer
 * ========================================================= */

#define RX_BUF_SIZE 5

uint8_t rx_buffer[RX_BUF_SIZE];

/* =========================================================
 * main
 * ========================================================= */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();         /* DMA must be initialized before peripherals */
    MX_USART1_UART_Init();

    /* Start first DMA reception — refired in callback */
    if (HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE) == HAL_OK) {
        UART_BM_send_string("READY\r\n");
    } else {
        UART_BM_send_string("DMA INIT FAILED\r\n");
    }

    while (1) {
        /* Nothing here — all work done in callback */
        HAL_Delay(1000);
    }
}

/* =========================================================
 * DMA IRQ Handler
 * Fixed name — do not rename
 * Delegates to HAL which clears flags and fires the callback
 * ========================================================= */
void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_peritomem);
}

/* =========================================================
 * UART RX Complete Callback
 * Called by HAL after DMA transfers RX_BUF_SIZE bytes
 * ========================================================= */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {

        /* Echo received bytes back */
        for (int i = 0; i < RX_BUF_SIZE; i++) {
            UART_BM_send_byte(rx_buffer[i]);
        }
        UART_BM_send_string("\r\n");

        /* Toggle LED to confirm reception */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

        /* Rearm DMA for next reception */
        HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE);
    }
}

/* =========================================================
 * USART1 Init
 * ========================================================= */
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

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * DMA1 Init — Channel5 wired to USART1 RX (hardware fixed)
 * ========================================================= */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_peritomem.Instance             = DMA1_Channel5;      /* USART1 RX — fixed by hardware */
    hdma_peritomem.Init.Direction       = DMA_PERIPH_TO_MEMORY;
    hdma_peritomem.Init.PeriphInc       = DMA_PINC_DISABLE;   /* UART DR is a single register */
    hdma_peritomem.Init.MemInc          = DMA_MINC_ENABLE;    /* advance through rx_buffer */
    hdma_peritomem.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_peritomem.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_peritomem.Init.Mode            = DMA_NORMAL;         /* re-armed manually in callback */
    hdma_peritomem.Init.Priority        = DMA_PRIORITY_MEDIUM;

    if (HAL_DMA_Init(&hdma_peritomem) != HAL_OK) {
        Error_Handler();
    }

    /* Link DMA handle to UART RX path */
    __HAL_LINKDMA(&huart1, hdmarx, hdma_peritomem);

    /* Enable DMA interrupt in NVIC */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}