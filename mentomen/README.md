# 🚀 STM32 DMA Memory-to-Memory Transfer (HAL) — Debug Case Study

## 📌 Overview

This project demonstrates a **Memory-to-Memory DMA transfer** on STM32 using the HAL library, along with a real debugging case where the transfer completed but the application got stuck in an infinite loop.

It highlights an important detail about how the **Direct Memory Access** callback mechanism works internally in STM32 HAL.

---

## 🎯 Objectives

* Implement a basic DMA memory-to-memory transfer
* Understand how DMA interrupts work in HAL
* Debug a real-world issue related to DMA callbacks
* Learn the difference between **expected vs actual HAL behavior**

---

## 🛠️ Features

* Memory-to-Memory DMA transfer
* Interrupt-based completion handling
* Manual callback registration
* Debug scenario with root cause analysis

---

## ⚙️ Hardware & Tools

* STM32 microcontroller (tested on STM32F1)
* STM32 HAL library
* STM32CubeIDE (or equivalent toolchain)

---

## 📂 Project Structure

```text
Core/
 ├── Src/
 │    ├── main.c
```

---

## 🔁 How It Works

1. DMA is configured for **Memory → Memory transfer**
2. Transfer is started using interrupt mode:

```c
HAL_DMA_Start_IT(...)
```

3. When transfer completes:

```text
DMA IRQ → HAL_DMA_IRQHandler() → XferCpltCallback
```

---

## ❗ The Bug

The application was stuck here:

```c
while (!dma_done);
```

Even though:

* DMA transfer completed ✅
* Interrupt fired ✅

👉 The flag `dma_done` was never updated.

---

## 🔍 Root Cause

The callback pointer inside the DMA handle was **NULL**:

```c
hdma->XferCpltCallback == NULL
```

STM32 HAL does **not always automatically call a global callback** depending on configuration.

Instead, it relies on a **function pointer inside the DMA handle**:

```text
HAL_DMA_IRQHandler() → hdma->XferCpltCallback()
```

Since it was not set, nothing happened.

---

## ✅ The Fix

Manually register the callback:

```c
hdma_memtomem.XferCpltCallback = DMA_XferCplt;
```

```c
static void DMA_XferCplt(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    dma_done = 1;
}
```

---

## 🧠 Key Takeaways

* DMA completion relies on **interrupt + handler + callback chain**
* HAL uses **function pointers in handles**, not always global callbacks
* Always verify:

  * IRQ is enabled
  * IRQ handler is implemented
  * Callback is correctly assigned

---

## 🧪 How to Test

1. Build and flash the project
2. Run the code
3. Verify that:

   * DMA transfer completes
   * `dma_done` becomes `1`
   * Program exits the loop correctly

---

## 🚀 Future Improvements

* Add UART + DMA circular mode
* Implement double buffering
* Add performance comparison (CPU vs DMA)

---

## 📚 References

* STM32 Reference Manual (DMA section)
* STM32 HAL documentation

---

## 🧑‍💻 Author

Embedded firmware learner exploring low-level concepts and real-world debugging.

---

## 📢 Note

This project focuses on **understanding behavior**, not just making it work — especially useful for learning how embedded systems behave under the hood.
