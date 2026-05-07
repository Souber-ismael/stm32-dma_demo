# 🚀 STM32 UART RX using DMA (HAL)

## 📌 Overview

This project demonstrates how to receive data over UART using DMA on STM32 microcontrollers with the HAL library.

It focuses on understanding how the **Direct Memory Access** works in **normal mode**, and how data is transferred from a peripheral to memory without CPU intervention.

---

## 🎯 Objectives

* Configure UART RX with DMA
* Receive data without blocking the CPU
* Understand DMA normal mode behavior
* Use interrupt callback to detect transfer completion

---

## 🛠️ Features

* UART RX using DMA
* DMA Normal Mode (one-shot transfer)
* Interrupt-driven completion
* LED toggle on transfer complete
* Basic UART debug output

---

## 📂 Project Structure

```text id="9p2c7a"
.
├── main.c
├── uart.c
└── uart.h
```

---

## ⚙️ How It Works

1. UART receives incoming data from external source (PC / terminal)
2. DMA transfers data from UART data register to memory buffer
3. When the buffer is full:

   * DMA triggers an interrupt
   * Callback function is executed

---

## 🔁 Data Flow

```text id="m3k8rt"
UART → DMA → RX Buffer → Callback → LED / Processing
```

---

## ⚠️ Important Notes

* DMA is configured in **Normal Mode**

  * Transfer stops when buffer is full
  * Must be restarted manually

* Callback is triggered **only when full buffer is received**

* Correct configuration is critical:

  * Peripheral Increment = OFF
  * Memory Increment = ON
  * DMA linked to UART RX

---

## 🧪 How to Test

1. Flash the program to your STM32 board
2. Open a serial terminal (e.g., PuTTY, Tera Term)
3. Send exactly **N bytes** (buffer size)
4. Observe:

   * LED toggles
   * Data received correctly

---

## 🐞 Debug Notes

Common issues encountered:

* Callback not triggered → buffer not full
* Wrong DMA linkage (TX instead of RX)
* Incorrect Peripheral Increment (PINC)
* Missing DMA IRQ handler

---

## 🚀 Future Improvements

* DMA Circular Mode (continuous reception)
* UART IDLE line detection
* Ring buffer implementation
* Protocol parsing (commands / frames)

---

## 🧠 Learning Outcome

This project helped understand:

* Difference between UART interrupts and DMA interrupts
* Asynchronous data transfer behavior
* Importance of buffer size and NDTR
* Real-world debugging of embedded systems

---

## 📢 Notes

This is a learning-focused project aimed at building a strong foundation in embedded firmware development using STM32.
