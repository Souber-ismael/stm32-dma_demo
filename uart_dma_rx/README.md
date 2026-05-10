🚀 STM32 UART Communication using DMA (TX + RX)
A bare-metal UART driver for STM32F1 built around DMA and IDLE line detection. Receives variable-length ASCII commands from a host PC and echoes them back — no RTOS, no blocking calls.

✨ Overview
This project implements a full-duplex UART communication driver on STM32F103 using DMA for both transmission and reception. The goal is to handle incoming commands of variable size efficiently, without polling or blocking the CPU.

🎯 Features

DMA-based TX — non-blocking transmission with completion flag
DMA-based RX — continuous reception into a circular buffer
IDLE line detection — triggers on end of message regardless of length
\r\n parsing — extracts clean command strings automatically
No RTOS required — runs entirely in a while(1) loop
Safe TX/RX synchronization — prevents handle conflicts on shared gState


🧱 Project Structure
Core/
├── Inc/
│   └── uart.h        — TX driver declarations and extern flags
├── Src/
│   ├── main.c        — init, IDLE enable, main loop with RX processing
│   └── uart.c        — DMA TX driver, TxCplt callback, IRQ handler

⚙️ System Architecture
[PC Terminal]
     │  SOUBER\r\n
     ▼
[USART1 RX pin]
     │
     ▼
[Shift Register]  ←  receives bits one by one, assembles one byte
     │
     ▼
[RDR Register]    ←  holds exactly 1 byte at a time
     │
     ▼
[DMA Channel 5]   ←  transfers byte into RAM without CPU
     │
     ▼
[rx_buffer[64]]   ←  your data sits here
     │
     ▼
[IDLE Interrupt]  ←  fires when line goes silent after last byte
     │
     ▼
[while(1) parses] ←  extracts command, sends response via DMA TX

📥 Reception Strategy
Reception is handled with DMA normal mode combined with the UART IDLE interrupt.
The DMA is configured for a 64-byte buffer. It runs continuously and writes incoming bytes into rx_buffer. When the sender stops transmitting, the UART line goes idle and triggers the IDLE interrupt — regardless of how many bytes were actually received.
Inside the interrupt, a volatile flag dma is set to 1. The main loop checks this flag, parses the buffer line by line looking for \n, strips the trailing \r, null-terminates the string, and processes the command.
After processing, HAL_UART_DMAStop() is called to reset RxState to READY, then HAL_UART_Receive_DMA() restarts the reception from the beginning of the buffer.

📤 Transmission Strategy
Transmission uses HAL_UART_Transmit_DMA() with a static internal buffer in uart.c. Before every send, the driver waits for the tx_done flag to be 1 — meaning the previous transfer has fully completed as confirmed by TxCpltCallback. The data is copied into the static buffer, tx_done is cleared, and the DMA transfer starts.
This guarantees the buffer is never overwritten mid-transfer and prevents the TX DMA from conflicting with the RX restart.

🧠 Key Concepts
DMA normal mode — transfers exactly N bytes then stops. RxState stays BUSY_RX until all N bytes are received or the transfer is manually stopped.
IDLE interrupt — fires when the UART line stays silent for one full frame after the last received byte. It is independent from the DMA — the DMA does not know the message is complete, it just keeps waiting for the remaining bytes.
RxState vs gState — the HAL handle has two state fields. gState covers the global peripheral state and is shared by TX and RX. RxState covers only the receive path. HAL_UART_Receive_DMA() checks both before accepting a new transfer.
tx_done flag — a volatile uint8_t set to 1 in HAL_UART_TxCpltCallback(). It must be volatile so the compiler always reads its actual value from memory and does not optimize the while(!tx_done) loop away.

🐞 Debug Notes
HAL_UART_Receive_DMA() returns 2 (HAL_BUSY) — RxState is still BUSY_RX because the DMA never received its full N bytes. Call HAL_UART_DMAStop() first to force RxState back to READY.
received counter keeps growing (8, 16, 22, 28...) — the DMA restart is failing silently. The DMA is accumulating all messages into the same buffer starting from where it left off. Root cause is always HAL_BUSY on the restart call.
TX and RX conflict on gState — TX DMA sets gState = BUSY_TX and returns immediately. If RX is restarted before TxCpltCallback fires, HAL refuses with HAL_BUSY. Always wait for tx_done = 1 before restarting RX.
line_pos corruption across callbacks — line_pos is a global variable. If a buffer ends without a \n, the leftover bytes stay in line_buffer and get prepended to the next message. Always break the for loop immediately after processing \n.
Overrun Error (ORE) — the RDR holds only one byte. If a new byte arrives before the DMA reads the current one, ORE is set and the UART freezes. Handle it in HAL_UART_ErrorCallback() by clearing the flag and restarting the DMA.

⚠️ Important Notes

dma and tx_done must both be declared volatile or the compiler may optimize away the polling loops
MX_USART1_UART_Init() must be called before UART_TX_Init() — otherwise HAL_UART_Init() overwrites the DMA TX link
DMA IRQ priority should be equal to or higher than UART IRQ priority to avoid the IDLE interrupt preempting an ongoing DMA transfer
IDLE and DMA are fully independent — IDLE firing does not stop or reset the DMA in any way
TX and RX share gState in the HAL handle even though they are physically independent on the hardware


🚀 Possible Improvements

Replace the while(!tx_done) busy-wait with a proper state machine to make the main loop fully non-blocking
Add a command dispatch table in main.c instead of chained strcmp calls
Move RX parsing into a dedicated uart_rx.c with its own ring buffer for multi-message queuing
Add timeout detection to flush a partial line_buffer if no \n arrives within a defined window
Migrate to HAL_UARTEx_ReceiveToIdle_DMA() for a cleaner IDLE-based reception without manual IRQ handling


📝 Conclusion
This project started as a simple UART echo and turned into a deep dive into how HAL manages DMA state internally. The main lesson is that IDLE and DMA are independent subsystems — IDLE tells you the message is done, but DMA does not know that and keeps its RxState as BUSY_RX. Understanding gState, RxState, HAL_BUSY, and the TX/RX handle conflict is what makes the difference between a driver that works and one that breaks after the second message.
