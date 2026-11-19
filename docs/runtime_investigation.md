# WebAssembly Runtime Feasibility & Architecture Investigation

> **Project:** SecureWasm Edge — Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems  
> **Author:** Sai Deep  

---

## 1. Executive Summary

This investigation evaluates WebAssembly (Wasm) runtimes across two distinct operational environments:
1. **Ultra-Constrained Embedded Silicon:** The **NodeMCU ESP8266EX** (Tensilica Xtensa LX106 @ 80 MHz, 80 KB SRAM, 4 MB Flash).
2. **Cloud/Edge Host Execution:** **x86_64** (AMD Ryzen 7 @ 3.6 GHz, Windows 11).

### Core Architectural Finding

Standard desktop and server WebAssembly engines (**Wasmtime**, **Wasmer**) are physically incompatible with microcontrollers having $\le 64\text{ KB}$ SRAM due to:
* The W3C specification requirement for 64 KiB linear memory pages.
* The lack of virtual memory management units (MMU) for zero-cost guard pages.
* Heavy runtime binary sizes (25 MB – 60 MB) and execution stack requirements ($>16\text{ KB}$).

To overcome these constraints, the project implements a **two-tier architecture**:
* On physical microcontrollers, we deploy **`wasm3`**, a universal micro-interpreter that executes bytecode directly from Flash ROM (`.rodata`) with **zero dynamic RAM overhead for bytecode storage** and sub-page memory boundaries.
* On the host tier, we deploy **`Wasmtime`** (Cranelift JIT) with hardware guard page sandboxing, fuel metering, and **SDRaD in-process fault recovery**.

---

## 2. Comprehensive Runtime Evaluation Matrix

| Criterion | Wasmtime (v49.0) | Wasmer (v7.5) | WAMR (v2.x) | wasm3 (v0.5.0) |
|:---|:---|:---|:---|:---|
| **Primary Maintainer** | Bytecode Alliance | Wasmer Inc. | Bytecode Alliance / Intel | Steven Massey / Community |
| **Target Domains** | Server, Cloud, Edge, CLI | Universal, Cloud, Edge | Embedded, IoT, Edge | Ultra-Constrained MCUs, IoT |
| **Supported Architectures** | x86_64, aarch64, riscv64 | x86_64, aarch64, riscv64 | x86, ARM, RISC-V, ESP32 | **ARM Cortex-M, ESP8266, ESP32, RISC-V, x86** |
| **Execution Engines** | JIT (Cranelift) | JIT (Cranelift/Singlepass), Interp | Fast JIT, Classic Interp, AOT | **Direct-Threaded M3 Interpreter** |
| **Binary Footprint** | ~25 MB – 45 MB | ~30 MB – 60 MB | 50 KB – 300 KB | **~64 KB Flash Footprint** |
| **Minimum Usable RAM** | ~15 MB – 30 MB | ~20 MB – 40 MB | ~100 KB – 256 KB | **~10 KB – 20 KB SRAM** |
| **Linear Memory Model**| 64 KiB Pages (MMU Guard) | 64 KiB Pages (MMU Guard) | 64 KiB Pages (Configurable Heap) | **Sub-Page (<64 KiB) & Stack-Only** |
| **Flash Direct Execution**| No (Virtual RAM Mapped) | No (Virtual RAM Mapped) | Experimental | **Yes (`const uint8_t[]` in ROM)** |
| **Status in Project** | **Adopted (Host JIT & SDRaD)** | **Evaluated** | **Investigated (Excluded: RAM)** | **Adopted (ESP8266 Live MCU Engine)** |

---

## 3. Microarchitectural & Memory Constraints on the ESP8266

The ESP8266 microcontroller presents three fundamental hardware barriers to hosting standard Wasm engines:

### 1. The "Memory Wall" (64 KiB Page Granularity vs. 43 KB Free Heap)
* The official W3C WebAssembly core specification mandates that linear memory is addressed and allocated in discrete memory pages of exactly **64 KiB ($65,536$ bytes)** (`(memory 1)`).
* The ESP8266EX has 80 KB total SRAM (partitioned into IRAM and DRAM). At runtime, the WiFi stack, NonOS SDK, and runtime heap leave **at most ~43 KB to ~51 KB of contiguous free DRAM**.
* Standard runtimes attempt to allocate 64 KiB for the linear memory block on initialization, causing an immediate **Out-Of-Memory (OOM)** allocation crash.

### 2. Architecture & ISA Limitations (Cadence Tensilica Xtensa LX106)
* The Xtensa LX106 is a reduced 32-bit RISC core without hardware floating-point support. All IEEE 754 operations require software emulation via GCC `libgcc`, multiplying execution cycle counts.
* Harvard architecture memory split: code executing from Flash must be 32-bit word aligned. Unaligned accesses or writing to executable regions triggers hardware exception traps.
* No MMU: Standard runtimes achieve out-of-bounds protection by placing unmapped 4 GiB virtual address space guard pages around linear memory. Without an MMU, embedded execution requires explicit software bounds checking.

### 3. Execution Stack Limitations
* Standard interpreter loops in engines like WAMR require 16 KB – 32 KB execution thread stacks.
* The ESP8266 execution stack is capped at 4 KB. Deep interpreter call stacks result in stack overflows and Watchdog Timer (`WDT`) resets.

---

## 4. Architectural Solutions for Sub-64KB Microcontrollers

To overcome the memory barrier, SecureWasm Edge implements the **`wasm3` micro-interpreter architecture**:

1. **Flash-Backed Direct Execution (`.rodata`):**
   The compiled `.wasm` bytecode is embedded in the 4 MB Flash memory (ROM) as a `const uint8_t[]` array mapped at `0x40200000`. `wasm3` parses and executes the bytecode directly from Flash without copying it into RAM, consuming **$0\text{ Bytes}$ of heap memory for bytecode storage**.
2. **Sub-Page Linear Memory & Register-Only Bytecode:**
   Workload 1 was compiled into register-only bytecode (219 bytes, no memory section), requiring zero linear memory allocation. For memory-dependent workloads, `wasm3` allows allocating sub-page memory buffers (e.g., 2 KB to 8 KB) within the ESP8266 heap rather than enforcing the 64 KiB minimum.
3. **M3 Direct-Threaded Metacode:**
   `wasm3` transforms bytecode on-the-fly into direct-threaded M3 metacode, executing opcodes in-place within the ESP8266's 4 KB stack frame.

---

## 5. Host Runtime Architecture & Fault Recovery

On host servers and edge gateways, **Wasmtime (v49.0)** provides high-throughput JIT execution using Cranelift. 

To address the host boundary blind spot (where native crashes in host functions kill the runtime), SecureWasm Edge integrates **Secure Domain Rewind and Discard (SDRaD)**:
* **Fault Interception:** Vector exception handlers catch hardware traps (`EXCEPTION_ACCESS_VIOLATION`, `SIGSEGV`).
* **State Rollback:** Snapshot restoration via `setjmp`/`longjmp` safely returns control to the host caller.
* **Domain Scrubbing:** Corrupted scratch memory is sanitized with `memset` in **$40\text{ ns}$**, preserving carrier-grade availability.

---

## 6. Experimental Benchmark Results

### Microcontroller Execution (ESP8266 @ 80 MHz, $N = 30$ Trials)
*Dataset: [`data/esp8266/esp8266_benchmarks_with_wasm3.csv`](../data/esp8266/esp8266_benchmarks_with_wasm3.csv)*

| Metric | Native C Baseline | wasm3 Micro-Interpreter | Variance / Slowdown |
|:---|:---|:---|:---|
| **Execution Latency** | **$4,877.67\ \mu\text{s}$ ($\pm 0.48$)** | **$53,658.90\ \mu\text{s}$ ($\pm 0.99$)** | **$+1,000.1\%$ ($11.00\times$)** |
| **RAM Footprint (Bytecode)**| Mapped in Flash | **$0\text{ Bytes}$ (Flash ROM)** | **Zero RAM Overhead** |
| **Heap Memory Delta ($\Delta\text{Heap}$)**| $0\text{ Bytes}$ | **$0\text{ Bytes}$** | **Zero Memory Leaks** |
| **32-Bit Golden Checksum** | `0x0F6B7313` | `0x0F6B7313` | **Bit-Perfect Determinism** |

### Host Execution: Cranelift JIT vs. Native C (x86_64, $N = 30$ Trials)
*Dataset: [`data/host/raw_benchmarks.csv`](../data/host/raw_benchmarks.csv)*

| Metric | Native C (Clang -O2) | Wasmtime JIT (Cranelift) | Sandboxing Overhead |
|:---|:---|:---|:---|
| **Execution Latency** | **$60.4\ \mu\text{s}$ ($\pm 4.6$)** | **$69.5\ \mu\text{s}$ ($\pm 2.6$)** | **$+15.1\%$ ($1.15\times$)** |
| **32-Bit Golden Checksum** | `0x0F6B7313` | `0x0F6B7313` | **Bit-Perfect Determinism** |

### Host Fault Recovery: SDRaD Prototype ($N = 60$ Trials)
*Dataset: [`data/security/sdrad_recovery_benchmarks.csv`](../data/security/sdrad_recovery_benchmarks.csv)*

| Metric | Measured Value | System Guarantee |
|:---|:---|:---|
| **Normal Host Service Overhead** | **$0.10\ \mu\text{s}$** | Negligible overhead during non-faulting execution |
| **Fault Interception & Rollback Latency** | **$4.12\ \mu\text{s}$** | Microsecond recovery from native C crashes |
| **Scratch Heap Discard Time** | **$0.04\ \mu\text{s}$ ($40\text{ ns}$)** | Sub-microsecond memory sanitization |
| **Host Process Crash Rate** | **$0.0\%$ (Survives 100%)** | Zero process abort on host function crash |
| **Multi-Tenant Availability** | **$100.0\%$** | Subsequent tenants execute cleanly post-crash |
