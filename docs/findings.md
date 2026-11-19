# System Engineering Findings & Technical Architecture

> **Project:** SecureWasm Edge — Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems  
> **Author:** Sai Deep  

---

## 1. Technical Challenge: The 64 KiB Memory Wall on Microcontrollers

### The 64 KiB Page Specification Barrier
* **W3C Specification Requirement:** The WebAssembly core specification mandates that linear memory is addressed and allocated in discrete memory pages of exactly **64 KiB ($65,536$ bytes)** (`(memory 1)`).
* **Physical Hardware Constraint:** The NodeMCU ESP8266EX microcontroller features 80 KB of total SRAM, divided between Instruction RAM (IRAM) and Data RAM (DRAM). After initializing the WiFi stack, NonOS SDK, and runtime heap, only **$\approx 43$ KB to $51.5$ KB** of contiguous heap memory remains.
* **Failure Mode of Standard Engines:** Standard engines (Wasmtime, Wasmer, WAMR) attempt to allocate at least one 64 KiB page upon module instantiation. On the ESP8266, this causes an immediate **Out-Of-Memory (OOM)** allocation abort:
  $$\text{Free Heap Available } (43\text{ KB}) < \text{Single WebAssembly Page } (64.0\text{ KB})$$

### Microarchitectural Constraints (Xtensa LX106)
1. **Lack of Hardware FPU:** The Cadence Tensilica Xtensa LX106 core does not include a hardware floating-point unit; all 32-bit and 64-bit IEEE 754 operations require software emulation via `libgcc`, multiplying execution cycle counts.
2. **Absence of Hardware MMU:** Standard desktop/server runtimes rely on 4 GiB unmapped virtual memory guard pages to trap out-of-bounds accesses with zero runtime check overhead. Without an MMU, embedded runtimes must rely on explicit software bounds checks on every memory instruction.
3. **Execution Stack Limits:** Typical interpreter runtimes demand 16 KB – 32 KB execution stacks for opcode dispatch loops. The ESP8266 Arduino continuation stack is capped at 4 KB, causing immediate stack overflow and Watchdog Timer (`WDT`) resets.

---

## 2. Engineering Solutions Implemented

### Solution 1: Flash-Backed Direct Execution via `wasm3` Micro-Interpreter
To execute WebAssembly on sub-64KB silicon, we deployed `wasm3` with three key optimizations:
1. **Flash ROM Execution (`.rodata`):** Rather than copying bytecode into the scarce 43 KB RAM, bytecode is compiled into a C header (`const uint8_t[]`) and placed in SPI Flash ROM mapped at `0x40200000`. The interpreter parses and decodes instructions directly from Flash with **$0\text{ Bytes}$ RAM storage overhead**.
2. **Sub-Page Memory Allocation:** Pure computational workloads (Workload 1) compile without a memory section (register-only, 219 bytes bytecode). For memory-dependent modules, `wasm3` is configured to allocate sub-page memory buffers (e.g., 2 KB) instead of the 64 KiB specification minimum.
3. **Direct-Threaded M3 Metacode:** `wasm3` converts bytecode on-the-fly into direct-threaded M3 metacode, executing operations in-place within the ESP8266's 4 KB stack frame.

### Solution 2: In-Process Fault Recovery on the Host Boundary (SDRaD)
Wasm protects the host from the guest, but does not protect the host process from crashes inside native host functions (WASI, crypto drivers, hardware interfaces).
1. **Exception Trap Interception:** Windows Structured Exception Handling (SEH) vectors intercept hardware exceptions (`EXCEPTION_ACCESS_VIOLATION`, `EXCEPTION_INT_DIVIDE_BY_ZERO`).
2. **Deterministic State Rollback:** Fast stack context snapshots (`setjmp`/`longjmp`) restore the runtime execution frame without killing the process.
3. **Domain Scratch Heap Sanitization:** Corrupted domain scratch buffers are wiped with `memset` in nanoseconds, allowing subsequent tenant requests to proceed with zero corruption.

---

## 3. System Design Decisions

1. **Dual-Tier Hardware Architecture:**
   - **Microcontroller Tier (ESP8266 @ 80 MHz):** Measures bare-metal execution latency (`micros()`), dynamic heap consumption (`ESP.getFreeHeap()`), and flash footprint.
   - **Host Runtime Tier (x86_64 @ 3.6 GHz):** Measures high-throughput Cranelift JIT compilation, virtual memory guard protection, and instruction fuel metering.
2. **Deterministic Workload Verification:**
   - All workloads execute Donald Knuth's 64-bit Linear Congruential Generator coupled with non-linear bit mixing, yielding an identical 32-bit golden checksum (`0x0F6B7313`) across both architectures, preventing compiler dead-code elimination.
3. **Isolation of Confounding Variables:**
   - Sandboxing overhead is measured strictly on identical hardware platforms to avoid conflating clock frequency differences with runtime virtualization overhead.

---

## 4. Empirical Evaluation Results

### Microcontroller Execution (NodeMCU ESP8266EX @ 80 MHz, $N = 30$ Trials)
*Data: [`data/esp8266/esp8266_benchmarks_with_wasm3.csv`](../data/esp8266/esp8266_benchmarks_with_wasm3.csv)*

| Metric | Native C (Xtensa GCC) | wasm3 (Micro-Interpreter) | Impact / Overhead |
|:---|:---|:---|:---|
| **Execution Latency (Mean $\pm$ SD)** | **$4,877.67\ \mu\text{s}$ ($\pm 0.48$)** | **$53,658.90\ \mu\text{s}$ ($\pm 0.99$)** | **$+1,000.1\%$ ($11.00\times$ Slowdown)** |
| **Bytecode / Code in RAM** | Mapped in Flash | **$0\text{ Bytes}$ (Flash ROM)** | **Zero dynamic RAM used for bytecode** |
| **Heap Memory Delta ($\Delta\text{Heap}$)** | $0\text{ Bytes}$ | **$0\text{ Bytes}$** | **Zero heap fragmentation or leaks** |
| **Golden Checksum** | `0x0F6B7313` | `0x0F6B7313` | **100% Bit-Perfect Match** |

### Host Execution: Cranelift JIT vs. Native C (x86_64, $N = 30$ Trials)
*Data: [`data/host/raw_benchmarks.csv`](../data/host/raw_benchmarks.csv)*

| Metric | Native C Baseline (Clang) | Wasmtime JIT (Cranelift) | Impact / Overhead |
|:---|:---|:---|:---|
| **Execution Latency (Mean $\pm$ SD)** | **$60.4\ \mu\text{s}$ ($\pm 4.6$)** | **$69.5\ \mu\text{s}$ ($\pm 2.6$)** | **$+15.1\%$ ($1.15\times$ Sandboxing Tax)** |
| **Golden Checksum** | `0x0F6B7313` | `0x0F6B7313` | **100% Bit-Perfect Match** |

### Adversarial Security Evaluation ($N = 35$ Test Vectors)
*Data: [`data/security/security_evaluation.csv`](../data/security/security_evaluation.csv)*

| Threat Category | Native C Exploit Rate | Wasm Sandbox Containment | Trapping Mechanism |
|:---|:---|:---|:---|
| **Spatial Out-of-Bounds Memory** | **100% Breached** | **100% Contained (14/14)** | Hardware Guard Page / Software Check |
| **Control-Flow Integrity (CFI)** | **100% Hijacked** | **100% Contained (6/6)** | Type Signature & Indirect Call Table |
| **CPU Denial of Service** | **100% Hangs** | **100% Contained (5/5)** | Deterministic Fuel Metering |
| **Unbounded Memory Flooding** | **100% Exhausts** | **100% Contained (4/4)** | Dynamic Allocation Cap (`-1`) |

### SDRaD Fault Recovery Performance ($N = 60$ Trials)
*Data: [`data/security/sdrad_recovery_benchmarks.csv`](../data/security/sdrad_recovery_benchmarks.csv)*

| Metric | Measured Value | Operational Guarantee |
|:---|:---|:---|
| **Fault Interception & Rollback Latency** | **$4.12\ \mu\text{s}$** | Microsecond recovery from fatal C crashes |
| **Domain Scratch Heap Discard Time** | **$0.04\ \mu\text{s}$ ($40\text{ ns}$)** | Sub-microsecond memory sanitization |
| **Host Process Crash Rate** | **$0.0\%$ (Survives 100%)** | Zero process termination on host function crash |
| **Multi-Tenant Availability** | **$100.0\%$** | Subsequent tenants execute cleanly post-crash |
