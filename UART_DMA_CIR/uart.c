/*
 * uart.c
 *
 * Full-duplex UART driver for USART1 on STM32F1 — HAL DMA mode.
 *
 * TX : DMA1 Channel 4, normal (one-shot) mode, non-blocking.
 * RX : DMA1 Channel 5, circular mode + IDLE interrupt.
 *      Uses NDTR to track the DMA write head and handles wrap-around.
 *
 * Hardware-fixed DMA mapping on STM32F1:
 *   DMA1 Channel 4 → USART1 TX
 *   DMA1 Channel 5 → USART1 RX
 */

#include "uart.h"

/* TX DMA handle — declared here, extern'd in main.c for the IRQ handler */
DMA_HandleTypeDef hdma_tx;

/* Saved UART handle — set once in UART_Driver_Init() */
static UART_HandleTypeDef *_huart;

/* ── RX state ──────────────────────────────────────────────────── */

/* Set to 1 by UART_IDLE_Callback(), cleared by UART_RX_Process() */
static volatile uint8_t rx_ready = 0;

/* Circular DMA destination — the DMA writes here continuously */
static uint8_t rx_buffer[UART_RX_BUF_SIZE];

/* ── TX state ──────────────────────────────────────────────────── */

/* 1 = DMA TX idle, 0 = transfer in progress.
 * Initialised to 1 because no transfer is running at startup. */
static volatile uint8_t tx_done = 1;

/* ── RX read pointer ───────────────────────────────────────────── */

/* Last known DMA write position in rx_buffer.
 * Updated at the end of every UART_RX_Process() call. */
static uint8_t old_pos = 0;


/* ----------------------------------------------------------------
 * UART_TX_Init
 * Configure DMA1 Channel 4 for USART1 TX in normal (one-shot) mode.
 * Each call to UART_TX_send() starts one transfer; the DMA stops
 * automatically after `len` bytes and raises the TC interrupt.
 * ---------------------------------------------------------------- */
void UART_TX_Init(UART_HandleTypeDef *huart)
{
    hdma_tx.Instance                 = DMA1_Channel4; /* USART1 TX — fixed by hardware */
    hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;  /* DR is a single fixed register */
    hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;   /* advance through source buffer */
    hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx.Init.Mode                = DMA_NORMAL;        /* stops after the transfer      */
    hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;

    if (HAL_DMA_Init(&hdma_tx) != HAL_OK)
        return;

    /* Link the DMA handle to the UART hdmatx field */
    __HAL_LINKDMA(huart, hdmatx, hdma_tx);

    /* HAL needs this IRQ to clear the TC flag and call TxCpltCallback */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

/* ----------------------------------------------------------------
 * UART_Driver_Init
 * Save the UART handle, arm the circular DMA RX, and enable IDLE.
 *
 * Circular mode: NDTR reloads automatically when it reaches 0 —
 * reception never stops without explicit software intervention.
 *
 * HAL_UART_Receive_DMA() does not enable the IDLE interrupt.
 * We enable it manually so UART_IDLE_Callback() fires at end-of-frame.
 * ---------------------------------------------------------------- */
void UART_Driver_Init(UART_HandleTypeDef *huart)
{
    _huart = huart;
    HAL_UART_Receive_DMA(_huart, rx_buffer, UART_RX_BUF_SIZE);
    __HAL_UART_ENABLE_IT(_huart, UART_IT_IDLE);
}

/* ----------------------------------------------------------------
 * UART_TX_send
 * Wait for any ongoing DMA TX to finish, then start a new transfer.
 * `buf` must remain valid until tx_done returns to 1.
 * ---------------------------------------------------------------- */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len)
{
    while (!tx_done); /* spin until previous transfer completes */
    tx_done = 0;
    HAL_UART_Transmit_DMA(huart, (uint8_t *)buf, len);
}

/* ----------------------------------------------------------------
 * UART_TX_send_string
 * Copy the string into a static buffer, then send via DMA.
 * Waiting before memcpy ensures the DMA has finished reading the
 * buffer before we overwrite it with new data.
 * ---------------------------------------------------------------- */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str)
{
    static uint8_t tx_buf[UART_TX_BUF_SIZE];
    uint16_t len = (uint16_t)strlen(str);

    if (len == 0 || len >= sizeof(tx_buf))
        return;

    while (!tx_done); /* wait before touching the static buffer */
    tx_done = 0;
    memcpy(tx_buf, str, len);
    HAL_UART_Transmit_DMA(huart, tx_buf, len);
}

/* ----------------------------------------------------------------
 * UART_RX_Process
 * Called from the main while(1) loop.
 *
 * Uses NDTR (DMA down-counter) to find the current DMA write head:
 *   new_pos = UART_RX_BUF_SIZE - NDTR
 *
 * Two cases:
 *
 *   Normal  (new_pos > old_pos):
 *     [....old####new....]  → copy old..new-1
 *
 *   Wrap    (new_pos < old_pos):
 *     [####new....old####] → copy old..end, then 0..new-1
 *
 * The received data is null-terminated and echoed back over TX.
 * old_pos is updated to new_pos at the end of each call.
 * ---------------------------------------------------------------- */
void UART_RX_Process(void)
{
    if (!rx_ready) return;
    rx_ready = 0;

    /* NDTR counts down from UART_RX_BUF_SIZE to 0.
     * new_pos is the index of the next byte the DMA will write. */
    uint16_t counter = __HAL_DMA_GET_COUNTER(_huart->hdmarx);
    uint16_t new_pos = UART_RX_BUF_SIZE - counter;

    if (new_pos == old_pos) return; /* no new data */

    char data[UART_RX_BUF_SIZE + 1]; /* +1 for null terminator */

    if (new_pos > old_pos)
    {
        /* Normal case — data is contiguous */
        uint16_t len = new_pos - old_pos;
        memcpy(data, &rx_buffer[old_pos], len);
        data[len] = '\0';
        UART_TX_send_string(_huart, data);
    }
    else
    {
        /* Wrap-around — copy tail then head */
        uint16_t len1 = UART_RX_BUF_SIZE - old_pos; /* tail: old_pos → end   */
        uint16_t len2 = new_pos;                     /* head: 0 → new_pos-1   */
        memcpy(data,        &rx_buffer[old_pos], len1);
        memcpy(data + len1, &rx_buffer[0],       len2);
        data[len1 + len2] = '\0';
        UART_TX_send_string(_huart, data);
    }

    /* Advance read pointer — modulo handles the wrap case cleanly */
    old_pos = new_pos % UART_RX_BUF_SIZE;
}

/* ----------------------------------------------------------------
 * UART_IDLE_Callback
 * Called from USART1_IRQHandler when the IDLE flag fires.
 * Sets rx_ready = 1 so UART_RX_Process() knows a frame arrived.
 * ---------------------------------------------------------------- */
void UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == _huart->Instance)
        rx_ready = 1;
}

/* ----------------------------------------------------------------
 * HAL_UART_TxCpltCallback
 * Called by HAL from DMA1_Channel4_IRQHandler when the TX transfer
 * is fully complete. Sets tx_done = 1 to unblock the next send.
 * ---------------------------------------------------------------- */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
    tx_done = 1;
}
