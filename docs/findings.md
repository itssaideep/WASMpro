# Key Findings & Architectural Analysis

> **Project:** SecureWasm Edge — Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems  
> **Author:** Sai Deep  
> **Date:** 2026-10-10  

---

## 1. Key Finding: ESP8266 Incompatibility with Standard WebAssembly

### The 64 KiB Memory Wall
* **Specification Requirement:** The official W3C WebAssembly core specification mandates that linear memory is addressed and allocated in discrete memory pages of exactly **64 KiB ($65,536$ bytes)**.
* **Physical Constraint:** The NodeMCU ESP8266EX microcontroller features 80 KB of total SRAM, split into IRAM and DRAM. After initializing the WiFi stack, NonOS SDK, and Arduino runtime, only **$\approx 51.5$ KB** of usable contiguous heap memory remains.
* **Failure Mode:** Attempting to allocate even a single standard WebAssembly linear memory page (`(memory 1)`) causes an immediate **Out-Of-Memory (OOM)** allocation failure before bytecode parsing or execution can commence:
  $$\text{Free Heap Available } (51.5\text{ KB}) < \text{Single WebAssembly Page } (64.0\text{ KB})$$

### Microarchitectural & Hardware Limitations (Xtensa LX106)
1. **Lack of Hardware FPU:** The Cadence Tensilica Xtensa LX106 core does not include a hardware floating-point unit; all 32-bit and 64-bit IEEE 754 float operations require software emulation via `libgcc`, adding significant execution cycles to floating-point opcodes.
2. **Absence of Memory Management Unit (MMU) & Guard Pages:** Standard WASM runtimes (e.g., Wasmtime, Wasmer) achieve zero-cost software sandboxing and out-of-bounds protection by placing unmapped 4 GiB virtual address space guard pages around linear memory. Without an MMU, the ESP8266 cannot implement virtual guard pages, requiring explicit bounds-checking on every load and store.
3. **Execution Stack Constraints:** Standard embedded runtimes like WAMR demand 16 KB – 32 KB execution thread stacks for operand stack dispatch loops. The ESP8266 Arduino continuation stack is capped at 4 KB, causing immediate stack overflow and watchdog timer (`WDT`) resets when running heavy interpreters.

---

## 2. Methodological & Architectural Decisions

To preserve the scientific integrity of the research while complying with hardware boundaries:

1. **Physical Baseline Role for ESP8266:**
   The ESP8266 is preserved as a bare-metal, native embedded C/C++ performance baseline. It captures deterministic hardware timings (`micros()`), heap footprint (`ESP.getFreeHeap()`), and binary flash utilization under real microcontroller conditions.

2. **Controlled Comparative Evaluation on Host x86_64:**
   Controlled Native-vs-WASM benchmarking is conducted on the host platform with identical compiler flags (`-O0`, `-O2`), matching workloads, identical 32-bit golden checksums, and cycle-accurate execution timers. Confounding variables (clock frequency differences, ISA mismatches) are controlled by isolating overhead calculations to identical hardware.

3. **Industrial Class Boundary Classification (Ericsson 792028 Finding):**
   Standard WebAssembly with WASI system calls requires devices with at least **$256$ KB SRAM** (e.g., ARM Cortex-M4/M33, Nordic nRF52840, STM32F4, or ESP32). Devices with $\le 64$ KB SRAM are fundamentally below the standard WASM hardware boundary.

---

## 3. The Solution: Micro-Interpreter Strategy (`wasm3`)

To evaluate whether WebAssembly can execute on sub-64KB microcontrollers, we deploy **`wasm3`**, a universal WebAssembly micro-interpreter designed specifically for extreme embedded environments:

1. **Flash-Backed Direct Execution:**
   The WebAssembly bytecode binary is stored directly in Flash memory (ROM) as a `const uint8_t[]` array, bypassing RAM consumption for module storage.
2. **Sub-Page Linear Memory Allocation & Stack-Only Modules:**
   Pure computational workloads (such as integer hashing and bitwise arithmetic) compile without linear memory imports or exports. For memory-dependent workloads, `wasm3` allows allocating sub-page memory buffers (e.g., 2 KB to 8 KB) within the ESP8266 heap rather than enforcing the 64 KiB specification minimum.
3. **M3 Metacode Transformation:**
   `wasm3` compiles Wasm bytecode on-the-fly into direct-threaded M3 metacode, executing opcodes in place with minimal call stack depth and enabling execution within the ESP8266's 4 KB stack frame.
