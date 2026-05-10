/*
 * uart.c
 *
 * Driver UART complet pour USART1 sur STM32F1 — mode DMA HAL.
 *
 * TX : DMA1 Channel4, mode normal (one-shot), non-bloquant.
 * RX : DMA1 Channel5, mode circulaire + détection IDLE.
 *
 * Mapping DMA fixé par le hardware STM32F1 :
 *   DMA1 Channel4 → USART1 TX
 *   DMA1 Channel5 → USART1 RX
 */

#include "uart.h"

/* Handle DMA TX — exposé en extern pour que main.c puisse déclarer
 * DMA1_Channel4_IRQHandler et appeler HAL_DMA_IRQHandler(&hdma_tx) */
DMA_HandleTypeDef hdma_tx;

/* Pointeur vers le handle UART sauvegardé lors de UART_Driver_Init() */
static UART_HandleTypeDef *_huart;

/* ── RX ─────────────────────────────────────────────────────────── */

/* Flag positionné à 1 par UART_IDLE_Callback() quand une trame arrive.
 * Remis à 0 par UART_RX_Process() après traitement. */
static volatile uint8_t rx_ready = 0;

/* Buffer circulaire DMA RX — le DMA écrit ici en continu */
static uint8_t rx_buffer[UART_RX_BUF_SIZE];

/* ── TX ─────────────────────────────────────────────────────────── */

/* Flag : 1 = DMA TX libre, 0 = transfert en cours.
 * Initialisé à 1 car aucun transfert n'est en cours au démarrage. */
static volatile uint8_t tx_done = 1;

/* ── Reconstruction de ligne ────────────────────────────────────── */

/* Buffer de reconstruction de ligne pour UART_RX_Process() */
static char    line_buffer[UART_RX_BUF_SIZE];
static uint8_t line_pos = 0;   /* index d'écriture dans line_buffer */


/* ----------------------------------------------------------------
 * UART_TX_Init
 * Configure DMA1 Channel4 pour USART1 TX en mode normal.
 * Un seul transfert par appel — le DMA s'arrête après `len` octets.
 * ---------------------------------------------------------------- */
void UART_TX_Init(UART_HandleTypeDef *huart)
{
    hdma_tx.Instance                 = DMA1_Channel4;  /* USART1 TX — fixé par le hardware */
    hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;   /* DR est un registre fixe */
    hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;    /* avance dans le buffer source */
    hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx.Init.Mode                = DMA_NORMAL;         /* s'arrête après le transfert */
    hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;

    if (HAL_DMA_Init(&hdma_tx) != HAL_OK)
        return;

    /* Lie le handle DMA au champ hdmatx du handle UART */
    __HAL_LINKDMA(huart, hdmatx, hdma_tx);

    /* IRQ DMA nécessaire pour que HAL efface le flag TC et appelle TxCpltCallback */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

/* ----------------------------------------------------------------
 * UART_Driver_Init
 * Sauvegarde le handle, arme le DMA RX en mode circulaire,
 * et active l'interruption IDLE sur USART1.
 *
 * Mode circulaire : quand NDTR atteint 0, le DMA recharge
 * automatiquement et recommence depuis le début de rx_buffer.
 * Aucun réarmement logiciel n'est nécessaire.
 *
 * IDLE : HAL_UART_Receive_DMA() n'active pas IDLE — on le fait ici.
 * Le flag IDLE se lève quand la ligne RX reste haute pendant
 * une durée équivalente à une trame complète après le dernier octet.
 * ---------------------------------------------------------------- */
void UART_Driver_Init(UART_HandleTypeDef *huart)
{
    _huart = huart;
    HAL_UART_Receive_DMA(_huart, rx_buffer, UART_RX_BUF_SIZE);
    __HAL_UART_ENABLE_IT(_huart, UART_IT_IDLE);
}

/* ----------------------------------------------------------------
 * UART_TX_send
 * Attend la fin du transfert précédent puis démarre un nouveau.
 * `buf` doit rester valide jusqu'à ce que tx_done repasse à 1.
 * ---------------------------------------------------------------- */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len)
{
    while (!tx_done);   /* attente active — le DMA est rapide */
    tx_done = 0;
    HAL_UART_Transmit_DMA(huart, (uint8_t *)buf, len);
}

/* ----------------------------------------------------------------
 * UART_TX_send_string
 * Copie la chaîne dans un buffer statique interne avant d'envoyer.
 * L'attente avant memcpy garantit que le DMA a fini de lire
 * le buffer avant qu'on l'écrase avec la nouvelle chaîne.
 * ---------------------------------------------------------------- */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str)
{
    static uint8_t tx_buf[UART_TX_BUF_SIZE];
    uint16_t len = (uint16_t)strlen(str);

    if (len == 0 || len >= sizeof(tx_buf))
        return;

    while (!tx_done);   /* attendre avant d'écrire dans le buffer */
    tx_done = 0;
    memcpy(tx_buf, str, len);
    HAL_UART_Transmit_DMA(huart, tx_buf, len);
}

/* ----------------------------------------------------------------
 * UART_RX_Process
 * À appeler dans while(1).
 * Quand rx_ready == 1 (positionné par IDLE), parcourt rx_buffer
 * octet par octet pour reconstruire une ligne terminée par '\n'.
 * Quand '\n' est trouvé, la ligne est echo-ée sur TX puis
 * line_buffer est réinitialisé.
 * Après traitement, réarme le DMA RX.
 * ---------------------------------------------------------------- */
void UART_RX_Process(void)
{
    if (!rx_ready) return;
    rx_ready = 0;

    for (int i = 0; i < UART_RX_BUF_SIZE; i++)
    {
        char c = (char)rx_buffer[i];

        if (c == '\n')
        {
            /* Fin de ligne — supprimer le '\r' éventuel avant '\n' */
            if (line_pos > 0 && line_buffer[line_pos - 1] == '\r')
                line_pos--;

            line_buffer[line_pos] = '\0';   /* terminer la chaîne */

            /* Echo de la ligne reçue */
            UART_TX_send_string(_huart, line_buffer);
            UART_TX_send_string(_huart, "\r\n");

            line_pos = 0;   /* réinitialiser pour la prochaine ligne */
            break;
        }
        else if (line_pos < sizeof(line_buffer) - 1)
        {
            line_buffer[line_pos++] = c;
        }
    }

    /* Attendre la fin du TX avant de réarmer le RX pour éviter
     * que HAL_UART_DMAStop() interrompe un transfert TX en cours */
    while (!tx_done);

    /* Réarmer le DMA RX pour la prochaine trame */
    HAL_UART_DMAStop(_huart);
    HAL_UART_Receive_DMA(_huart, rx_buffer, UART_RX_BUF_SIZE);
}

/* ----------------------------------------------------------------
 * UART_IDLE_Callback
 * Appelée depuis USART1_IRQHandler quand le flag IDLE est détecté.
 * Positionne rx_ready pour signaler à UART_RX_Process() qu'une
 * trame est disponible dans rx_buffer.
 * ---------------------------------------------------------------- */
void UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == _huart->Instance)
        rx_ready = 1;
}

/* ----------------------------------------------------------------
 * HAL_UART_TxCpltCallback
 * Appelée par HAL depuis DMA1_Channel4_IRQHandler quand le
 * transfert TX est terminé. Remet tx_done = 1 pour débloquer
 * le prochain appel à UART_TX_send() ou UART_TX_send_string().
 * ---------------------------------------------------------------- */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
    tx_done = 1;
}
