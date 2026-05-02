# 🚀 STM32 DMA Learning Project (Memory, UART RX & TX)

## 📌 Overview

This repository contains a complete learning path to understand and master the **Direct Memory Access** on STM32.

The project progresses from basic memory transfers to advanced UART communication using DMA in both RX and TX directions.

---

## 🎯 Objectives

* Understand how DMA works at hardware level
* Use DMA for memory-to-memory transfers
* Implement UART reception using DMA
* Implement UART transmission using DMA
* Build a non-blocking, interrupt-driven firmware architecture

---

## 🧱 Project Structure

```text id="w3j9qv"
.
├── mem_to_mem_dma/     → Basic DMA transfer (memory → memory)
├── uart_rx_dma/        → UART reception using DMA (normal & circular)
├── uart_dma_CIR/      → UART RX + TX using DMA + Interrupt
│
├── Core/
├── Drivers/
└── README.md
```

---

## ⚙️ Implemented Modules

---

### 🔹 1. Memory-to-Memory DMA

* First step to understand DMA behavior
* Transfers data between two memory buffers
* Uses DMA interrupt to detect completion

```text id="i9y0dz"
Memory → DMA → Memory
```

✔ Learn:

* NDTR usage
* DMA configuration
* Transfer complete interrupt

---

### 🔹 2. UART RX with DMA

#### ✔ Normal Mode

* One-shot transfer
* DMA stops when buffer is full
* Callback triggered on completion

#### ✔ Circular Mode

* Continuous reception
* DMA never stops
* Buffer handled using NDTR

```text id="i3xz6k"
UART → DMA → Buffer
```

✔ Learn:

* Peripheral vs memory increment
* Buffer management
* Difference between normal and circular mode

---

### 🔹 3. UART RX + TX with DMA (Full System)

* RX using DMA (circular mode)
* TX using DMA + interrupt
* IDLE line detection for frame processing

```text id="lf4shx"
RX:
UART → DMA → buffer → IDLE → processing

TX:
Memory → DMA → UART → Interrupt
```

✔ Learn:

* Full duplex communication
* Interrupt-driven architecture
* Efficient CPU usage

---

## 🧠 Key Concepts Covered

* DMA transfer mechanism
* DMA request vs interrupt
* UART vs DMA roles
* NDTR register usage
* Circular buffer management
* IDLE line detection (manual handling on STM32F1)
* Callback vs event-driven design

---

## ⚠️ Important Notes

* DMA must be initialized before peripherals
* Peripheral increment must be disabled for UART
* Memory increment must be enabled
* DMA circular mode requires buffer tracking logic
* IDLE interrupt is required for variable-length frames

---

## 🐞 Debug Lessons Learned

* DMA callback not triggered → buffer not full (normal mode)
* Wrong DMA linkage → no data transfer
* Peripheral increment enabled → corrupted data
* Confusion between UART IT and DMA IT
* IDLE detection must be handled manually on STM32F1

---

## 🚀 Future Improvements

* Ring buffer abstraction
* Double buffering (ping-pong DMA)
* Protocol parsing (command frames)
* Support for SPI / ADC DMA
* RTOS integration

---

## 🧠 Conclusion

This project demonstrates a complete progression from basic DMA usage to a real embedded communication system.

```text id="xwd0dr"
From simple transfers → to real-time data streaming
```

It builds a strong foundation for advanced embedded firmware development.
