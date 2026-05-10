/*
 * uart.h
 *
 * Driver UART complet pour USART1 sur STM32F1 — mode DMA HAL.
 *
 * TX : DMA1 Channel4 → USART1 TX  (mapping fixe hardware)
 * RX : DMA1 Channel5 → USART1 RX  (mapping fixe hardware)
 *      Mode circulaire + détection IDLE pour paquets de longueur variable.
 *
 * Séquence d'initialisation dans main() :
 *   1. MX_DMA_Init()          — horloge DMA + config Channel5 RX
 *   2. MX_USART1_UART_Init()  — config USART1
 *   3. UART_TX_Init()         — config Channel4 TX + lien hdmatx
 *   4. UART_Driver_Init()     — arme le DMA RX + active l'IRQ IDLE
 */

#ifndef INC_UART_H_
#define INC_UART_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Taille du buffer circulaire RX — doit être >= la plus grande trame attendue */
#define UART_RX_BUF_SIZE  64

/* Taille du buffer statique TX utilisé par UART_TX_send_string() */
#define UART_TX_BUF_SIZE  128

/*
 * UART_TX_Init
 * Configure DMA1 Channel4 pour USART1 TX en mode normal (one-shot).
 * Lie le handle DMA au handle UART via __HAL_LINKDMA.
 * À appeler après MX_DMA_Init() et MX_USART1_UART_Init().
 */
void UART_TX_Init(UART_HandleTypeDef *huart);

/*
 * UART_Driver_Init
 * Arme le DMA RX en mode circulaire et active l'interruption IDLE.
 * À appeler après UART_TX_Init().
 */
void UART_Driver_Init(UART_HandleTypeDef *huart);

/*
 * UART_TX_send
 * Envoie `len` octets depuis `buf` via DMA.
 * Attend la fin du transfert précédent avant de démarrer.
 * `buf` doit rester valide jusqu'à ce que HAL_UART_TxCpltCallback soit appelé.
 */
void UART_TX_send(UART_HandleTypeDef *huart, const uint8_t *buf, uint16_t len);

/*
 * UART_TX_send_string
 * Copie la chaîne dans un buffer statique interne puis envoie via DMA.
 * L'attente avant memcpy garantit que le buffer n'est plus utilisé par le DMA.
 * Longueur max : UART_TX_BUF_SIZE - 1 caractères.
 */
void UART_TX_send_string(UART_HandleTypeDef *huart, const char *str);

/*
 * UART_RX_Process
 * À appeler dans la boucle while(1) de main().
 * Traite le contenu de rx_buffer quand rx_ready == 1 (positionné par IDLE).
 * Reconstruit les lignes caractère par caractère, les echo sur TX,
 * puis réarme le DMA RX.
 */
void UART_RX_Process(void);

/*
 * UART_IDLE_Callback
 * Appelée depuis USART1_IRQHandler quand le flag IDLE est détecté.
 * Positionne rx_ready = 1 pour signaler à UART_RX_Process() qu'une
 * trame est disponible dans rx_buffer.
 */
void UART_IDLE_Callback(UART_HandleTypeDef *huart);

/*
 * HAL_UART_TxCpltCallback
 * Callback HAL appelé à la fin de chaque transfert DMA TX.
 * Remet tx_done = 1 pour débloquer le prochain envoi.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INC_UART_H_ */
