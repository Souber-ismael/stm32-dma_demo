/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   UART DMA TX + RX circulaire avec détection IDLE
  *
  * USART1 TX → DMA1 Channel4  (non-bloquant, one-shot)
  * USART1 RX → DMA1 Channel5  (circulaire, jamais arrêté)
  *
  * Au démarrage : envoie "READY\r\n".
  * En fonctionnement : echo de chaque ligne reçue dans while(1).
  *
  * Mapping DMA fixé par le hardware STM32F1 :
  *   DMA1 Channel4 → USART1 TX
  *   DMA1 Channel5 → USART1 RX
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "uart.h"

/* Handles UART et DMA RX — définis ici, utilisés dans uart.c via extern */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;

/* hdma_tx est défini dans uart.c — on le déclare extern pour les IRQ handlers */
extern DMA_HandleTypeDef hdma_tx;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();            /* horloge DMA + Channel5 RX avant HAL_UART_Init */
    MX_USART1_UART_Init();    /* configure USART1, active l'IRQ USART1 */
    UART_TX_Init(&huart1);    /* configure Channel4 TX, lie hdma_tx à huart1 */
    UART_Driver_Init(&huart1);/* arme le DMA RX circulaire + active IDLE */

    /* Premier message — confirme que l'UART fonctionne */
    UART_TX_send_string(&huart1, "READY\r\n");

    while (1)
    {
        /* Traite les trames reçues quand IDLE a signalé une fin de paquet */
        UART_RX_Process();
    }
}

/* ----------------------------------------------------------------
 * USART1_IRQHandler
 * HAL gère RXNE, TC et les erreurs.
 * IDLE n'est pas géré par HAL — on le vérifie manuellement avant.
 * On vérifie IDLE EN PREMIER pour éviter que HAL_UART_IRQHandler
 * efface des flags avant qu'on ait pu les lire.
 * Sur STM32F1, effacer IDLE nécessite de lire SR puis DR —
 * __HAL_UART_CLEAR_IDLEFLAG() fait cette séquence.
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
 * Appelé quand le transfert TX est terminé (flag TC).
 * HAL_DMA_IRQHandler appelle ensuite HAL_UART_TxCpltCallback.
 * ---------------------------------------------------------------- */
void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_tx);
}

/* ----------------------------------------------------------------
 * DMA1_Channel5_IRQHandler — USART1 RX
 * Appelé sur TC et HT en mode circulaire, et sur les erreurs DMA.
 * En mode circulaire on utilise IDLE pour détecter la fin de trame,
 * pas TC — mais HAL a besoin de cet handler pour gérer les erreurs.
 * ---------------------------------------------------------------- */
void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* ----------------------------------------------------------------
 * MX_USART1_UART_Init — 9600 8N1, TX+RX, sans contrôle de flux
 * Les GPIO PA9/PA10 sont configurés par HAL_UART_MspInit()
 * dans stm32f1xx_hal_msp.c.
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

    /* Priorité 0 = la plus haute — IDLE doit répondre rapidement */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ----------------------------------------------------------------
 * MX_DMA_Init
 * Active l'horloge DMA1 et configure Channel5 pour USART1 RX
 * en mode circulaire.
 * Channel4 (TX) est configuré séparément dans UART_TX_Init().
 *
 * Mode Normal : NDTR ne se recharge pas automatiquement à 0,
 * la réception  s'arrête.
 * ---------------------------------------------------------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;   /* DR est un registre fixe */
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;    /* avance dans rx_buffer   */
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_NORMAL;    
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;

    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        Error_Handler();

    /* Lie le handle DMA au champ hdmarx du handle UART */
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/* ----------------------------------------------------------------
 * MX_GPIO_Init
 * LED LD2 sur PA5 — sortie push-pull, départ éteint.
 * Bouton B1 sur PC13 — entrée EXTI front montant.
 * PA9/PA10 (USART1) sont gérés par HAL_UART_MspInit().
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

    /* Priorité 5 — plus basse que UART(0) et DMA(1) */
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
