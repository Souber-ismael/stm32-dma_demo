# 🚀 STM32 UART Communication using DMA (TX + RX)

## 📌 Overview

This project implements UART communication on STM32 using the **Direct Memory Access** with interrupt-driven design.

It demonstrates how to build an efficient and non-blocking firmware architecture for both transmission (TX) and reception (RX).

---

## 🎯 Features

* UART transmission using DMA + Interrupt (TX)
* UART reception using DMA in Circular Mode (RX)
* IDLE line detection for frame processing
* Continuous data streaming without CPU blocking
* Modular driver design (`uart.c / uart.h`)

---

## 🧱 Project Structure

```text id="4k7l8k"
.
├── main.c        → Application entry point
├── uart.c        → UART driver implementation
└── uart.h        → UART API
```

---

## ⚙️ System Architecture

```text id="x7u9mn"
TX:
Memory → DMA → UART → TX line
                ↓
            Interrupt (TC)

RX:
UART → DMA (circular) → buffer[]
                    ↓
              IDLE interrupt
                    ↓
         Data processing (NDTR)
```

---

## 📥 Reception Strategy

* DMA runs in **circular mode**
* Data is continuously written into a buffer
* IDLE interrupt indicates end of frame
* NDTR register is used to track new data
* Only unprocessed data is handled

---

## 📤 Transmission Strategy

* Data is sent using DMA
* CPU is not involved during transfer
* Transfer completion is handled via interrupt

---

## 🧠 Key Concepts

* DMA-based data transfer
* Interrupt-driven firmware design
* Circular buffer management
* IDLE line detection (STM32F1 manual handling)
* Separation between hardware flow and software logic

---

## ⚠️ Important Notes

* DMA must be initialized before UART
* Peripheral increment must be disabled (PINC)
* Memory increment must be enabled (MINC)
* IDLE interrupt must be enabled manually on STM32F1
* Buffer size must be chosen carefully to avoid overflow

---

## 🐞 Debug Notes

* No data received → check DMA linkage with UART
* Callback not triggered → verify interrupt configuration
* Corrupted data → check DMA settings (alignment, increment)
* Missing frames → verify IDLE detection logic

---

## 🚀 Possible Improvements

* Ring buffer abstraction
* Protocol parser (commands / frames)
* Multi-UART support
* Double buffering (advanced DMA usage)

---

## 🧠 Conclusion

This project demonstrates a complete UART communication system using DMA and interrupts, suitable for real-time embedded applications.
