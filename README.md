# SecureWasm Edge

> In-Process Isolation, Fault Recovery, and Sandboxed WebAssembly Execution for Embedded Systems and Multi-Tenant Runtimes

[![Platform](https://img.shields.io/badge/Platform-ESP8266%20%7C%20x86__64-blue.svg)](#system-architecture)
[![Runtime](https://img.shields.io/badge/Runtimes-wasm3%20%7C%20Wasmtime-green.svg)](#system-architecture)
[![Security](https://img.shields.io/badge/Security-SDRaD%20%7C%20SFI-red.svg)](#in-process-fault-recovery-sdrad)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

---

## Overview

Executing third-party, untrusted code on edge devices and microcontrollers is traditionally dangerous. Bare-metal C/C++ firmware delivers high speed and low memory usage, but has **zero memory safety**: a single buffer overflow, null pointer dereference, or malicious payload corrupts hardware registers and compromises the entire system.

**SecureWasm Edge** is an end-to-end systems project providing memory-safe, sandboxed execution for embedded silicon and cloud/edge runtimes using **WebAssembly (Wasm)**. 

The project solves two core engineering problems:
1. **At the Microcontroller Edge:** It runs WebAssembly on ultra-constrained chips (NodeMCU ESP8266EX, 80 KB SRAM) by bypassing the standard 64 KiB memory page requirement, executing bytecode directly from Flash memory (ROM) with **zero dynamic RAM overhead for module storage** and **zero heap leakage**.
2. **At the Runtime Host Boundary:** It hardens the Wasm runtime itself against native crashes. Standard Wasm engines crash completely if a native host function (WASI, crypto drivers, or C libraries) fails. SecureWasm Edge implements **Secure Domain Rewind and Discard (SDRaD)**—an in-process fault recovery subsystem that catches native faults, erases corrupted domain memory, and rolls execution back in **$4.12\ \mu\text{s}$** without taking down the host process.

---

## System Architecture

The project implements a dual-tier execution architecture comparing bare-metal native code against sandboxed WebAssembly:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              SecureWasm Edge                                │
├──────────────────────────────────────┬──────────────────────────────────────┤
│      EMBEDDED TIER (Physical MCU)    │          HOST RUNTIME TIER           │
│   NodeMCU ESP8266EX (Xtensa LX106)   │        Workstation (x86_64)          │
│   80 MHz, 80 KB SRAM, 4 MB Flash     │        Windows 11, Multi-core        │
├──────────────────┬───────────────────┼──────────────────┬───────────────────┤
│  Native C Engine │ wasm3 Interpreter │  Native C Engine │   Wasmtime JIT    │
│  Xtensa GCC -O2  │ Flash ROM Storage │  LLVM Clang -O2  │  Cranelift JIT    │
│  Hardware Timers │ Sub-Page Sandbox  │  High-Res QPC    │  Fuel Metering    │
│  Zero Heap Delta │ 2 KB Memory Cap   │  Working Set     │  Virtual Guards   │
└──────────────────┴───────────────────┴──────────────────┴───────────────────┘
                                       │
                                       ▼
       ┌──────────────────────────────────────────────────────────────┐
       │             SDRaD IN-PROCESS FAULT RECOVERY LAYER            │
       │  - Checkpoints CPU register state via setjmp                 │
       │  - Intercepts native host crashes (SEGV / Access Violation)  │
       │  - Discards domain scratch memory in 40 nanoseconds          │
       │  - Rewinds execution in 4.12 µs without process termination  │
       └──────────────────────────────────────────────────────────────┘
```

### Components

1. **Embedded Microcontroller Tier (`esp8266/`):**
   * **Hardware:** NodeMCU v2 (ESP8266EX, 32-bit Xtensa LX106 @ 80 MHz, 80 KB SRAM, 4 MB SPI Flash).
   * **Engines:** Bare-metal native C compiled via `xtensa-lx106-elf-gcc` vs. on-device interpreted Wasm via `wasm3`.
   * **Telemetry:** Hardware microsecond timers (`micros()`), heap tracking (`ESP.getFreeHeap()`), and UART serial streaming at 115,200 baud.
2. **Host Runtime Tier (`native/`, `wasm/`):**
   * **Hardware:** Modern x86_64 workstation.
   * **Engines:** Native C compiled via LLVM Clang vs. Wasmtime (v49.0) with Cranelift JIT compilation (`-O0` and `-O2`).
3. **Automated Adversarial Security Suite (`security/tests/`):**
   * Automated test battery evaluating spatial memory bounds, control-flow integrity, CPU starvation, and capability boundaries.
4. **SDRaD Fault Recovery Subsystem (`security/sdrad/`):**
   * C library and Wasmtime integration providing isolated memory domains, hardware exception filters, and microsecond transactional rollback.

---

## Workload Suite

The system implements six deterministic, architecture-neutral workloads designed to test real-world edge operations:

* **Workload 1 (Integer ALU):** Triple Linear Congruential Generator (LCG) coupled with non-linear bitwise avalanche mixing. Combines three historical PRNG constants (Knuth's *Numerical Recipes*, POSIX/glibc `rand()`, and MSVC `rand()`).
  * *Golden Checksum:* `0x0F6B7313`
* **Workload 2 (Checksum / Hashing):** IEEE 802.3 CRC32 bitwise polynomial reduction over a deterministic 256-byte buffer.
  * *Golden Checksum:* `0x067088A3`
* **Workload 3 (Array & Memory Access):** In-place bubble sort and permutations over a 64-element array testing sequential and random access overhead.
  * *Golden Checksum:* `0x36ABF7B8`
* **Workload 4 (DSP Data Transform):** S-box non-linear substitution and 8-bit circular bitwise rotation over a 128-byte buffer.
  * *Golden Checksum:* `0xF170C069`
* **Workload 5 (Sensor Emulation):** Synthetic analog-to-digital converter (ADC) exponential moving average (EMA) filter and peak detection.
  * *Golden Checksum:* `0x0249054D`
* **Workload 6 (Control Flow & State Machine):** 6-state finite state machine with branching transitions and switch dispatch.
  * *Golden Checksum:* `0x0000222F`

*All workloads use strict 32-bit unsigned wrapping arithmetic ($2^{32}$ modulo) with zero floating-point operations and zero undefined behavior, guaranteeing bit-for-bit checksum equivalence across all targets.*

---

## Core Engineering Implementations

### 1. Overcoming the 64 KiB Memory Wall on Microcontrollers

The W3C WebAssembly specification mandates that linear memory is allocated in discrete pages of **64 KiB ($65,536\text{ bytes}$)**. The ESP8266 has only **$\approx 43\text{ KB}$** of available heap after booting the WiFi stack and SDK. Standard runtimes (Wasmtime, Wasmer, WAMR) crash immediately with an Out-of-Memory (`OOM`) error.

**Our Implementation:**
1. **Flash-Backed ROM Storage (0 Bytes RAM for Bytecode):**  
   Standard runtimes allocate dynamic RAM to store and parse `.wasm` files. In [`esp8266/include/workload_integer_wasm.h`](esp8266/include/workload_integer_wasm.h), the bytecode is declared as `const uint8_t[]` in Flash memory (`.rodata` at `0x40200000`). `wasm3` decodes opcodes in place directly from Flash ROM, consuming **0 bytes of dynamic RAM**.
2. **Sub-Page Memory Allocation:**  
   Bypasses standard 64 KiB page allocations via `runtime->memoryLimit = 2048`, constraining execution to a **2 KB boundary**.
3. **Register-Only Bytecode:**  
   The core compute module is compiled with zero linear memory sections (all state mapped to local registers), producing a standalone **219-byte** binary.

### 2. In-Process Fault Recovery (SDRaD Architecture)

Standard WebAssembly sandboxing is strictly **guest-to-host**: it prevents guest bytecode from corrupting host memory. However, the host runtime itself (WASI dispatches, native host functions, linked C libraries) runs with raw process privileges. If a native host function crashes (null pointer dereference, buffer overflow), **the entire OS process terminates**, killing all co-located workloads.

Inspired by Ericsson Research's **Secure Domain Rewind and Discard (SDRaD)** framework, we built an in-process fault recovery engine in [`security/sdrad/`](security/sdrad/):

```
      Wasm Guest Calls Native Host Function
                        │
                        ▼
            1. sdrad_set_anchor()
            Captures CPU registers via setjmp in 40 ns
                        │
                        ▼
            2. sdrad_domain_enter()
            Switches to isolated scratch heap with guard canaries
                        │
                        ▼
            3. Native Host Function Executes
            💥 Triggers native memory fault (*(int*)0x0)
                        │
                        ▼
            4. Vectored Exception Interception
            Catches fault at OS kernel boundary before process aborts
                        │
                        ▼
            5. sdrad_discard()
            memset zeroes domain heap in 40 ns, erasing corruption
                        │
                        ▼
            6. sdrad_rewind()
            longjmp restores CPU registers back to Step 1 in 4.12 µs
                        │
                        ▼
     Host Function returns clean error code (-403) to guest
     Runtime process remains 100% ALIVE!
                        │
                        ▼
     Co-located Tenant B executes immediately in same process
     Result: 0xC70BFB85 (100% data integrity, zero state leakage!)
```

---

## Project Structure

```
WASMpro/
├── esp8266/                 # Embedded Microcontroller Firmware (PlatformIO)
│   ├── include/             # Workload definitions & 219-byte Wasm bytecode
│   ├── src/main.cpp         # Firmware entry point, UART streamer & benchmark runner
│   ├── src/wasm3_workload.cpp # wasm3 micro-interpreter runner & 2 KB sandbox
│   └── platformio.ini       # Build configuration for NodeMCU v2 (Xtensa LX106)
├── native/                  # Host Native C Benchmark Engine
│   └── src/main.c           # Windows high-resolution performance counter runner
├── wasm/                    # Standalone WebAssembly Modules
│   └── modules/             # Precompiled .wasm binaries (-O0 and -O2)
├── security/                # Security & Fault Recovery Subsystems
│   ├── tests/               # Adversarial test suite & native exploit PoC
│   │   ├── native_vulnerabilities.c # Bare-metal C exploit demonstrator
│   │   └── run_security_suite.py    # 35-vector automated attack harness
│   └── sdrad/               # SDRaD In-Process Fault Recovery Engine
│       ├── sdrad.h / sdrad.c         # Core domain, anchor, discard & rewind API
│       ├── vulnerable_host_service.c # Simulated host function with memory flaw
│       ├── sdrad_evaluator.c         # C benchmark harness for recovery latency
│       └── wasmtime_sdrad_integration.py # Live Wasmtime multi-tenant bridge
├── data/                    # Raw Empirical CSV Datasets
│   ├── esp8266/             # 210 trials on physical ESP8266 silicon
│   ├── wasm/                # 360 trials on Wasmtime (JIT)
│   └── security/            # 35 security trials & 60 SDRaD recovery trials
├── plots/                   # High-Resolution Visualization Charts
├── docs/                    # Technical Documentation & Analysis Reports
└── scripts/                 # Automation, collection, and plotting scripts
```

---

## Quick Start & Reproduction

### Prerequisites
* **Python 3.10+** (`pip install wasmtime pywasm3 pandas matplotlib pyserial`)
* **PlatformIO Core** (for ESP8266 builds)
* **LLVM Clang / MinGW-w64** (for native host builds)

### 1. Flash and Run on Physical ESP8266 Hardware
Connect the NodeMCU board via USB (CP210x on COM port) and run:
```bash
# Flash the firmware via PlatformIO
pio run -d esp8266 -t upload

# Stream live serial telemetry and export CSV
python scripts/collect_esp8266.py
```

### 2. Run Host Benchmarks (Native vs. Wasmtime)
```bash
# Execute native C benchmark suite
python scripts/run_native_benchmarks.py

# Execute Wasmtime Cranelift JIT suite
python scripts/run_wasm_benchmarks.py
```

### 3. Run Automated Adversarial Security Suite
```bash
# Execute the 35-vector security evaluation
python security/tests/run_security_suite.py

# Generate security charts
python scripts/plot_security_results.py
```

### 4. Run SDRaD Fault Recovery & Multi-Tenant Demo
```bash
# Run standalone C SDRaD benchmark
security/sdrad/sdrad_evaluator.exe

# Run live Wasmtime multi-tenant integration
python security/sdrad/wasmtime_sdrad_integration.py

# Generate recovery performance plot
python scripts/plot_sdrad_results.py
```

---

## Performance & Security Evaluation Results

All results below were gathered from verified empirical trials across physical ESP8266 silicon ($N=210$), host Wasmtime ($N=360$), adversarial security attacks ($N=35$), and SDRaD recovery trials ($N=60$).

### 1. On-Device Microcontroller Execution (ESP8266 @ 80 MHz)

Telemetry recorded directly over UART from NodeMCU v2 ($N=30$ recorded trials per workload):

| Engine | Workload | Iterations | Mean Latency | StdDev | Checksum | Status | Heap Delta |
|:---|:---|:---|:---|:---|:---|:---|:---|
| **esp8266-native** | integer | 10,000 | **$4,877.67\ \mu\text{s}$** ($4.88\text{ ms}$) | $\pm 1.92\ \mu\text{s}$ | `0x0F6B7313` | PASS | **$0\text{ B}$** |
| **esp8266-native** | checksum | 100 | **$26,271.53\ \mu\text{s}$** | $\pm 2.01\ \mu\text{s}$ | `0x067088A3` | PASS | **$0\text{ B}$** |
| **esp8266-native** | array | 100 | **$26,593.63\ \mu\text{s}$** | $\pm 0.49\ \mu\text{s}$ | `0x36ABF7B8` | PASS | **$0\text{ B}$** |
| **esp8266-native** | transform | 1,000 | **$19,339.97\ \mu\text{s}$** | $\pm 1.56\ \mu\text{s}$ | `0xF170C069` | PASS | **$0\text{ B}$** |
| **esp8266-native** | sensor | 10,000 | **$1,484.10\ \mu\text{s}$** | $\pm 0.84\ \mu\text{s}$ | `0x0249054D` | PASS | **$0\text{ B}$** |
| **esp8266-native** | controlflow | 10,000 | **$2,910.40\ \mu\text{s}$** | $\pm 1.33\ \mu\text{s}$ | `0x0000222F` | PASS | **$0\text{ B}$** |
| **esp8266-wasm3** | integer | 10,000 | **$53,658.90\ \mu\text{s}$** ($53.66\text{ ms}$) | $\pm 165.3\ \mu\text{s}$ | `0x0F6B7313` | PASS | **$0\text{ B}$** |

* **Execution Overhead:** wasm3 incurred an **$11.00\times$ slowdown** ($+1{,}000.1\%$) relative to native compiled C. This represents the exact cost of direct-threaded opcode dispatch on an 80 MHz core without dynamic JIT capabilities.
* **Heap Stability:** Dynamic heap consumption was **$\Delta\text{Heap} = 0\text{ B}$** across all trials (heap locked at 42,912 bytes before and after execution).
* **Module Initialization:** One-time module parsing from Flash ROM averaged **$1,167.77\ \mu\text{s}$** ($1.17\text{ ms}$).

---

### 2. Host Execution: JIT Compilation vs. Native (x86_64)

Benchmarked on host workstation ($N=30$ trials per configuration):

| Workload | Native C (-O2) | Wasmtime JIT (-O2) | Overhead Ratio | Overhead (%) | Checksum Verified |
|:---|:---|:---|:---|:---|:---|
| **integer** | $60.4\ \mu\text{s}$ | $69.5\ \mu\text{s}$ | **$1.15\times$** | **$+15.1\%$** | `0x0F6B7313` (PASS) |
| **checksum**| $312.8\ \mu\text{s}$| $381.6\ \mu\text{s}$| **$1.22\times$** | **$+22.0\%$** | `0x067088A3` (PASS) |
| **array** | $145.2\ \mu\text{s}$| $184.4\ \mu\text{s}$| **$1.27\times$** | **$+27.0\%$** | `0x36ABF7B8` (PASS) |
| **transform**| $88.5\ \mu\text{s}$ | $110.6\ \mu\text{s}$| **$1.25\times$** | **$+25.0\%$** | `0xF170C069` (PASS) |
| **sensor** | $18.2\ \mu\text{s}$ | $22.4\ \mu\text{s}$ | **$1.23\times$** | **$+23.1\%$** | `0x0249054D` (PASS) |
| **controlflow**| $42.1\ \mu\text{s}$| $48.8\ \mu\text{s}$ | **$1.16\times$** | **$+15.9\%$** | `0x0000222F` (PASS) |

* On server-class hardware with hardware MMU support, Wasmtime's Cranelift JIT incurs only **$+15.1\% - +27.0\%$ overhead** relative to native compiled C.

---

### 3. Adversarial Security Benchmark Results ($N = 35$ Vectors)

Evaluated across Native C, Wasmtime, and wasm3:

```
==========================================================================================================
                               ADVERSARIAL SECURITY EVALUATION SUMMARY                                   
==========================================================================================================
Vector ID    Threat Category       Runtime                   Payload / Offset            Status     Latency
──────────────────────────────────────────────────────────────────────────────────────────────────────────
V1-NAT       OOB Memory Write      Native C                  Buffer overflow (+8B)     VULNERABLE   0.00 µs
V1-WT        OOB Memory Write      Wasmtime (Cranelift JIT)  Write @ 0x00010000 (+0B)  CONTAINED  300.60 µs
V1-WT        OOB Memory Write      Wasmtime (Cranelift JIT)  Write @ 0x00010004 (+4B)  CONTAINED   68.10 µs
V1-WT        OOB Memory Write      Wasmtime (Cranelift JIT)  Write @ 0x00100000 (+1MB) CONTAINED   76.80 µs
V1-WT        OOB Memory Write      Wasmtime (Cranelift JIT)  Write @ 0xFFFFFFFC (Wrap) CONTAINED   72.10 µs
V1-W3        OOB Memory Write      wasm3 (Micro-Interp)      Write @ 0x00010000 (+0B)  CONTAINED   48.30 µs
V1-W3        OOB Memory Write      wasm3 (Micro-Interp)      Write @ 0x00010004 (+4B)  CONTAINED    6.40 µs
V1-W3        OOB Memory Write      wasm3 (Micro-Interp)      Write @ 0x00100000 (+1MB) CONTAINED    6.20 µs
V1-W3        OOB Memory Write      wasm3 (Micro-Interp)      Write @ 0xFFFFFFFC (Wrap) CONTAINED    6.60 µs
V2-NAT       OOB Memory Read       Native C                  Read past buffer bounds   VULNERABLE   0.00 µs
V2-WT        OOB Memory Read       Wasmtime (Cranelift JIT)  Read @ 0x00010004 (+4B)   CONTAINED   55.80 µs
V2-W3        OOB Memory Read       wasm3 (Micro-Interp)      Read @ 0x00010004 (+4B)   CONTAINED    6.30 µs
V3-NAT       CFI Function Hijack   Native C                  Corrupt function pointer  VULNERABLE   0.00 µs
V3-WT        CFI Table OOB         Wasmtime (Cranelift JIT)  call_indirect idx=2       CONTAINED  324.60 µs
V3-W3        CFI Table OOB         wasm3 (Micro-Interp)      call_indirect idx=2       CONTAINED   24.50 µs
V4-WT        CFI Signature Mismatch Wasmtime (Cranelift JIT) Type signature clash      CONTAINED  130.10 µs
V4-W3        CFI Signature Mismatch wasm3 (Micro-Interp)     Type signature clash      CONTAINED   26.60 µs
V5-NAT       CPU DoS (Infinite)    Native C                  while(1) infinite loop    VULNERABLE   0.00 µs
V5-WT        CPU DoS (Fuel Meter)  Wasmtime (Fuel Metering)  25,000 instruction budget CONTAINED  119.70 µs
V6-NAT       RAM Flood (memory)    Native C                  malloc(SIZE_MAX)          VULNERABLE   0.00 µs
V6-WT        RAM Flood (grow)      Wasmtime (Cranelift JIT)  memory.grow 65536 (4 GiB) CONTAINED   88.90 µs
V6-W3        RAM Flood (grow)      wasm3 (Micro-Interp)      memory.grow 65536 (4 GiB) CONTAINED    4.70 µs
V7-NAT       Ambient Syscall       Native C                  Read host /etc/hosts      VULNERABLE   0.00 µs
V7-WT        Ambient Syscall       Wasmtime (Linker)         Ungranted host import     CONTAINED  111.50 µs
V7-W3        Ambient Syscall       wasm3 (Linker)            Ungranted host import     CONTAINED   60.10 µs
==========================================================================================================
```

* **Wasm Sandboxed Vectors Contained:** **$29 / 29\ (100.0\%)$**. Zero bytes leaked, zero unauthorized jumps.
* **Native C Vectors Vulnerable:** **$6 / 6\ (100.0\%)$**. Stack canary corruption, privilege escalation, and backdoor jumps verified.

---

### 4. SDRaD In-Process Fault Recovery Results ($N = 60$ Trials)

Benchmarked on host runtime using [`security/sdrad/sdrad_evaluator.c`](security/sdrad/sdrad_evaluator.c):

| Metric | Measured Value | Operational Significance |
|:---|:---|:---|
| **Normal Host Service Overhead** | **$0.10\ \mu\text{s}$** | Negligible overhead during non-faulting execution |
| **Fault Interception & Rollback Latency** | **$4.12\ \mu\text{s}$** | Catches hardware exception and rolls back in single-digit microseconds |
| **Domain Scratch Heap Discard Time** | **$0.04\ \mu\text{s}$ ($40\text{ ns}$)** | `memset` sanitizes corrupted domain memory in nanoseconds |
| **Host Process Crash Rate** | **$0.0\%$ (Survives 100%)** | Prevents OS process termination on native faults |
| **Multi-Tenant Availability** | **$100.0\%$** | Legitimate Tenant B executed immediately post-crash with golden checksum `0xC70BFB85` |

---

## Master Comparison Matrix

| Evaluation Criterion | Native C Baseline | Standard Wasmtime | wasm3 on ESP8266 | SDRaD-Hardened Wasmtime |
|:---|:---|:---|:---|:---|
| **Execution Latency (Workload 1)** | $60.4\ \mu\text{s}$ (Host) / $4.88\text{ ms}$ (MCU) | $69.5\ \mu\text{s}$ (Host) | $53.66\text{ ms}$ (MCU) | $69.6\ \mu\text{s}$ (Host) |
| **Execution Overhead** | $0.0\%$ (Baseline) | **$+15.1\%$ ($1.15\times$)** | **$+1,000.1\%$ ($11.00\times$)** | **$+15.2\%$ ($1.15\times$)** |
| **RAM Used for Storing Bytecode** | Full binary in Flash | Mapped in virtual RAM | **$0\text{ Bytes}$ (Flash ROM)** | Mapped in virtual RAM |
| **Dynamic Heap Leakage** | N/A | $\Delta\text{RAM} = 0$ | **$\Delta\text{Heap} = 0\text{ B}$** | $\Delta\text{RAM} = 0$ |
| **Guest Spatial Memory Safety** | ❌ Vulnerable (Canary overwrite) | ✅ Contained (Hardware trap) | ✅ Contained (Software check) | ✅ Contained (Hardware trap) |
| **Control-Flow Integrity (CFI)** | ❌ Vulnerable (Func ptr hijacked)| ✅ Contained (Table checked) | ✅ Contained (Table checked) | ✅ Contained (Table checked) |
| **Host-Function Native Crash** | 💥 Process dies (Kills all) | 💥 Process dies (Kills all) | 💥 Board Reboots (WDT) | ✅ **Recovered in $4.12\ \mu\text{s}$** |
| **Multi-Tenant Availability** | 0% Survival | 0% Survival | 0% Survival | **100% Survival (Carrier-Grade)** |

---

## License

This project is licensed under the [MIT License](LICENSE).
