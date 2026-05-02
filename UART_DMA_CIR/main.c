#include "main.h"
#include "stm32f1xx.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_peritomem;

/*
 * UART DMA RX — circular buffer + IDLE line detection
 * STM32F1 — USART1 RX → DMA1 Channel5 → rx_buffer
 *
 * The DMA writes every incoming byte into rx_buffer in circular
 * mode and wraps back to index 0 automatically.
 * When the RX line goes silent for one full frame the UART raises
 * the IDLE flag, which triggers UART_IDLE_Callback().
 * The callback uses NDTR (DMA down-counter) to find the write head
 * and echoes the new bytes back over TX.
 *
 * NDTR counts DOWN from RX_BUF_SIZE to 0.
 * write head = RX_BUF_SIZE - NDTR
 */

#define RX_BUF_SIZE 12

uint8_t         rx_buffer[RX_BUF_SIZE];
static uint16_t old_pos   = 0;  /* read pointer — tracks last processed position */

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();           /* DMA before UART: __HAL_LINKDMA must run
                                before HAL_UART_Init() touches hdmarx   */
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_SPI2_Init();

    /* Arm DMA reception — circular mode runs forever, no restart needed */
    if (HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE) == HAL_OK) {
        UART_BM_send_string("READY\r\n");
    } else {
        UART_BM_send_string("ERROR\r\n");
    }

    /* HAL_UART_Receive_DMA() does not enable IDLE — enable it manually.
     * The flag fires when the line stays high for one full frame after
     * the last received byte, signalling end of packet. */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    while (1)
    {
        /* All RX processing happens inside UART_IDLE_Callback().
         * The CPU is free here for application logic. */
    }
}

/* ----------------------------------------------------------------
 * USART1 IRQ Handler
 *
 * HAL_UART_IRQHandler() clears RXNE, TC, and error flags.
 * IDLE is not handled by HAL so we check it manually after.
 * Calling HAL first avoids a race with an in-progress byte.
 * ---------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);

    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE))
    {
        /* On STM32F1 clearing IDLE requires reading SR then DR.
         * __HAL_UART_CLEAR_IDLEFLAG() performs that sequence. */
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        UART_IDLE_Callback(&huart1);

        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  /* heartbeat LED */
    }
}

/* ----------------------------------------------------------------
 * UART_IDLE_Callback
 *
 * Computes how many bytes arrived since the last call using NDTR,
 * then echoes them back over TX.
 *
 * Three cases handled:
 *
 *   Normal  (new_pos > old_pos):
 *     [....old####new....]  → send old..new-1
 *
 *   Wrap    (new_pos < old_pos):
 *     [####new....old####] → send old..end, then 0..new-1
 *
 *   Overrun (new_pos == old_pos but DMA moved):
 *     Sender filled the entire buffer before IDLE fired.
 *     Detected by comparing NDTR against its previous value.
 *     → send all RX_BUF_SIZE bytes starting from old_pos.
 * ---------------------------------------------------------------- */
void UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    (void)huart;

    uint16_t ndtr    = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    uint16_t new_pos = RX_BUF_SIZE - ndtr;

    /* Track previous NDTR to detect the overrun case where
     * new_pos and old_pos coincide after a full-buffer write. */
    static uint16_t prev_ndtr = 0;
    uint8_t overrun = (new_pos == old_pos) && (ndtr != prev_ndtr);
    prev_ndtr = ndtr;

    if (new_pos != old_pos || overrun)
    {
        if (overrun || new_pos > old_pos)
        {
            /* Normal or overrun: data is contiguous */
            uint16_t len = overrun ? RX_BUF_SIZE : (new_pos - old_pos);
            HAL_UART_Transmit(&huart1, &rx_buffer[old_pos], len, 100);
        }
        else
        {
            /* Wrap-around: send tail then head */
            HAL_UART_Transmit(&huart1, &rx_buffer[old_pos],
                              RX_BUF_SIZE - old_pos, 100);
            HAL_UART_Transmit(&huart1, &rx_buffer[0], new_pos, 100);
        }

        /* Advance read pointer to the current DMA write head */
        old_pos = new_pos;
    }
}

/* ----------------------------------------------------------------
 * USART1 Init — 9600 8N1, TX+RX, no flow control
 * NVIC enabled after HAL_UART_Init() so the IRQ is armed last.
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
 * DMA1 Init — Channel5 is the USART1 RX channel on STM32F1
 *   DMA1 Ch4 → USART1 TX
 *   DMA1 Ch5 → USART1 RX  (used here)
 *
 * Circular mode: NDTR reloads automatically at 0, reception
 * never stops and no software restart is needed.
 * ---------------------------------------------------------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_peritomem.Instance                 = DMA1_Channel5;
    hdma_peritomem.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_peritomem.Init.PeriphInc           = DMA_PINC_DISABLE;   /* UART DR is one fixed register */
    hdma_peritomem.Init.MemInc              = DMA_MINC_ENABLE;    /* advance through rx_buffer     */
    hdma_peritomem.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_peritomem.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_peritomem.Init.Mode                = DMA_CIRCULAR;
    hdma_peritomem.Init.Priority            = DMA_PRIORITY_MEDIUM;

    if (HAL_DMA_Init(&hdma_peritomem) != HAL_OK)
        Error_Handler();

    /* Link to UART RX path so HAL_UART_Receive_DMA() knows which channel to arm */
    __HAL_LINKDMA(&huart1, hdmarx, hdma_peritomem);

    /* DMA IRQ needed by HAL_DMA_Start_IT() for error handling */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/* ----------------------------------------------------------------
 * DMA1 Channel5 IRQ Handler — routes to HAL for error handling
 * ---------------------------------------------------------------- */
void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_peritomem);
}
