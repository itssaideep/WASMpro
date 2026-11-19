# SecureWasm Edge

### Performance Benchmarking, Security Evaluation, and In-Process Fault Recovery of WebAssembly in Resource-Constrained Embedded Systems

---

## Project Overview

As distributed intelligence shifts toward the extreme edge of IoT and telecommunications infrastructure, executing third-party or untrusted software modules directly on resource-constrained embedded microcontrollers has become a critical challenge. Traditional native firmware (compiled C/C++) provides optimal performance and minimal memory usage but lacks memory isolation, hardware sandboxing, and runtime security boundaries, meaning a single pointer arithmetic bug or malicious payload can compromise the entire node.

**SecureWasm Edge** investigates **WebAssembly (Wasm)** as a secure, portable, and memory-safe execution environment for resource-constrained edge computing. This research evaluates whether WebAssembly's software-fault isolation (SFI) guarantees, deterministic capabilities, and compact bytecode can be viably deployed on microcontrollers with severe silicon and memory limitations, quantifies the exact performance and resource overhead incurred compared to bare-metal native code, and demonstrates in-process fault recovery (SDRaD) to protect host runtimes against native host-function crashes.

---

## Research Scope & Objectives

The project addresses five foundational research questions:

* **RQ1 (Execution Performance):** What is the exact execution latency and throughput penalty of running WebAssembly compared to native compiled C/C++ across compute-heavy, memory-intensive, and bitwise edge workloads?
* **RQ2 (Resource Consumption):** How does WebAssembly impact dynamic RAM consumption, heap allocation stability, and flash/storage footprint on microcontrollers with sub-100 KB SRAM?
* **RQ3 (Security & Sandboxing):** How effectively does the WebAssembly sandbox trap out-of-bounds memory accesses, illegal pointer dereferences, and prevent unauthorized host resource exploitation without hardware MMU support?
* **RQ4 (Execution Engine Trade-Offs):** What are the comparative performance and architectural trade-offs between host-level Just-In-Time (JIT) compilation (e.g., Cranelift/Wasmtime) and microcontroller-level micro-interpretation (e.g., `wasm3`)?
* **RQ5 (Host Resilience & Fault Recovery):** How can in-process compartmentalization and rollback (SDRaD) protect the host runtime and co-located workloads when native host functions or WASI dispatches trigger fatal memory faults?

---

## Experimental Architecture

The evaluation framework employs a dual-tier empirical design that guarantees fair, controlled comparisons while isolating confounding microarchitectural variables:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              SecureWasm Edge                                │
├──────────────────────────────────────┬──────────────────────────────────────┤
│      EMBEDDED TIER (Physical MCU)    │          HOST CONTROL TIER           │
│   NodeMCU ESP8266EX (Xtensa LX106)   │        Workstation (x86_64)          │
│   80 MHz, 80 KB SRAM, 4 MB Flash     │        Windows 11, Multi-core        │
├──────────────────┬───────────────────┼──────────────────┬───────────────────┤
│  Native C Engine │ wasm3 Interpreter │  Native C Engine │   Wasmtime JIT    │
│  Xtensa GCC -O2  │ Flash Bytecode    │  LLVM Clang -O2  │  Cranelift JIT    │
│  Cycle Accuracy  │ M3 Metacode Stack │  High-Res QPC    │  Fuel Metering    │
│  Zero Heap Delta │ Sub-Page Sandbox  │  Working Set     │  Virtual Guards   │
└──────────────────┴───────────────────┴──────────────────┴───────────────────┘
```

1. **Embedded Microcontroller Tier:**
   * **Target:** NodeMCU ESP8266EX featuring a 32-bit Tensilica Xtensa LX106 core running at 80 MHz with only ~43 KB of usable DRAM at runtime.
   * **Evaluations:** Bare-metal native C workloads compiled via `xtensa-lx106-elf-gcc` benchmarked directly against on-device interpreted WebAssembly via the universal micro-interpreter **`wasm3`**.
   * **Telemetry:** Microsecond-precision hardware timing (`micros()`), dynamic heap monitoring (`ESP.getFreeHeap()`), and serial telemetry capture over UART at 115,200 baud.

2. **Host Control Tier:**
   * **Target:** Modern x86_64 workstation providing cycle-accurate performance counters and baseline comparison.
   * **Evaluations:** Native C compiled with LLVM Clang benchmarked against Wasmtime (v49.0.2) running the Cranelift JIT compiler across `-O0` and `-O2` optimization levels.

---

## Workload Suite

The framework implements six deterministic, architecture-neutral benchmark workloads representative of real-world edge sensing and edge analytics tasks:

1. **Workload 1 (Integer Arithmetic):** Triple Linear Congruential Generator (LCG) coupled with non-linear bitwise mixing and accumulator perturbation:
   * **Generator A:** Knuth / *Numerical Recipes* constants ($a = 1664525, c = 1013904223$)
   * **Generator B:** POSIX / glibc / ANSI C `rand()` constants ($a = 1103515245, c = 12345$)
   * **Generator C:** Microsoft Visual C++ `rand()` constants ($a = 214013, c = 2531011$)
   * **Non-linear mixing:** Avalanche bit rotation and XOR folding prevents compiler optimization/dead-code elimination.
   * **Deterministic Checksum:** `0x0F6B7313`
2. **Workload 2 (Checksum / Cryptographic Primitive):** IEEE 802.3 CRC32 bitwise polynomial computation over a deterministic 256-byte buffer (Checksum: `0x067088A3`).
3. **Workload 3 (Array & Memory Access):** Permutation and in-place bubble sort of a 64-element array measuring sequential and random access overhead (Checksum: `0x36ABF7B8`).
4. **Workload 4 (Matrix / Data Transform):** S-box non-linear substitution and circular bitwise rotation over a 128-byte buffer (Checksum: `0xF170C069`).
5. **Workload 5 (Sensor Emulation):** Synthetic analog-to-digital converter (ADC) exponential moving average (EMA) filter and peak detection (Checksum: `0x0249054D`).
6. **Workload 6 (Control Flow & State Machine):** 6-state finite state machine with branching and cyclic state transitions (Checksum: `0x0000222F`).

*All workloads maintain strict mathematical determinism using unsigned 32-bit wrap-around arithmetic ($2^{32}$ modulo) with bit-for-bit golden checksum verification across both architectures and all execution engines.*

---

## Core Findings & Architectural Discoveries

### 1. The 64 KiB Memory Wall on Embedded Silicon
The W3C WebAssembly core specification mandates that linear memory is allocated in discrete pages of **64 KiB ($65,536\text{ bytes}$)**. Microcontrollers in the sub-100 KB SRAM class (such as the ESP8266 with $\approx 43\text{ KB}$ available heap at boot) experience immediate Out-Of-Memory (`OOM`) failure when attempting to instantiate standard WebAssembly modules:
$$\text{Available Free Heap } (42{,}912\text{ B}) < \text{Single WebAssembly Page } (65{,}536\text{ B})$$
Standard runtimes (Wasmtime, Wasmer, standard WAMR) physically cannot execute in this hardware class because they require allocating full 64 KiB pages upon instantiation.

### 2. The Micro-Interpreter Strategy (`wasm3`) & Flash-Backed Storage
To overcome the memory wall on sub-64 KB silicon, this research implements a specialized micro-interpreter architecture using `wasm3`:
* **Flash-Backed Direct Bytecode Execution (0 Bytes RAM Allocation):** Standard runtimes load the `.wasm` file into a dynamic RAM buffer via `malloc()`, consuming scarce heap before execution starts. On the ESP8266, the compiled bytecode array is declared as `const uint8_t[]` in [`workload_integer_wasm.h`](esp8266/include/workload_integer_wasm.h). The compiler places it in `.rodata` burned into the 4 MB SPI Flash ROM (`0x40200000`). `wasm3` decodes opcodes in place directly from Flash, consuming **zero bytes of dynamic RAM for module storage**.
* **Sub-Page Memory Sandboxing:** Bypassing standard 64 KiB page allocations via `runtime->memoryLimit = 2048` enables execution within a 2 KB execution stack boundary with **zero heap leakage ($\Delta\text{Heap} = 0\text{ B}$)** across 30 consecutive trials.

### 3. JIT Compilation vs. Micro-Interpretation Trade-off
Empirical measurements quantify the exact cost of sandboxing across hardware tiers:
* **Host JIT Execution (x86_64 + Cranelift):** WebAssembly incurs only **$+15.1\%$ execution overhead ($1.15\times$)** relative to native compiled C ($60.4\ \mu\text{s}$ vs $69.5\ \mu\text{s}$) due to hardware MMU guard pages and AOT machine code generation.
* **Embedded Interpreted Execution (Xtensa + wasm3):** WebAssembly incurs an **$11.00\times$ execution slowdown (+1,000.1% overhead)** relative to native compiled C ($4,877.7\ \mu\text{s}$ vs $53,658.9\ \mu\text{s}$) due to direct-threaded opcode dispatch loops on an 80 MHz core without dynamic JIT capabilities.
* **Industrial Takeaway:** The 11× slowdown is the exact mathematical price of enforcing software-based memory safety on silicon that lacks an MMU. It remains significantly faster than MicroPython ($30\times - 100\times$) or embedded Lua ($20\times - 30\times$).

### 4. Empirical Security & Sandboxing Evaluation (RQ3)
Adversarial benchmarking across $N=35$ test vectors proved WebAssembly's software fault isolation capabilities:
* **100% Sandboxed Containment Rate (29/29):** Both Wasmtime and wasm3 deterministically trapped all out-of-bounds reads/writes, null pointer dereferences, indirect call table violations, and type signature mismatches in under $50\ \mu\text{s}$ without leaking a single byte of host memory.
* **100% Native C Exploit Rate (6/6):** Unprotected native C suffered silent stack canary corruption, privilege escalation, unauthorized function pointer redirection, and uncontrolled heap exhaustion.
* **Resource Governance:** Wasmtime's instruction fuel metering successfully bounded execution and contained infinite loops in $119.7\ \mu\text{s}$, preventing host CPU starvation.
* **See full report:** [`docs/security_evaluation.md`](docs/security_evaluation.md) and visualization in [`plots/security_evaluation.png`](plots/security_evaluation.png).

### 5. In-Process Isolation & Fault Recovery Prototype (SDRaD Integration)
Addressing the vulnerability of native host components outside the Wasm sandbox, we implemented an in-process fault recovery prototype inspired by Ericsson Research's **Secure Domain Rewind and Discard (SDRaD)** framework:
* **The Vulnerability:** While Wasm protects the host from the guest, native host functions and WASI handlers execute with raw native process privileges. A bug in a host library crashes the entire runtime process, killing all co-located workloads.
* **Microsecond Fault Recovery ($4.12\ \mu\text{s}$):** Deliberate native memory faults in host functions were intercepted via low-level exception filters, isolated domain scratch memory was discarded ($40\ \text{ns}$), and execution was rewound to a saved anchor point in **$4.12\ \mu\text{s}$** without crashing the host process.
* **Multi-Tenant Availability (100%):** Verified in Wasmtime that when an adversarial tenant triggers a native host crash, the fault is converted to a clean `-403` trap while co-located legitimate tenants continue executing in the same process with 100% data integrity (`0xC70BFB85`).
* **See full report:** [`docs/sdrad_integration.md`](docs/sdrad_integration.md) and visualization in [`plots/sdrad_recovery_performance.png`](plots/sdrad_recovery_performance.png).

---

## Executive Evaluation Summary

| Evaluation Dimension | Native C Baseline | Wasmtime JIT (x86_64) | wasm3 Micro-Interp (ESP8266) | SDRaD-Hardened Wasmtime |
|:---|:---|:---|:---|:---|
| **Workload 1 Execution Time** | $60.4\ \mu\text{s}$ (Host) / $4.88\text{ ms}$ (MCU) | $69.5\ \mu\text{s}$ (Host) | $53.66\text{ ms}$ (MCU) | $69.6\ \mu\text{s}$ |
| **Sandboxing Overhead** | $0\%$ (Baseline) | **$+15.1\%$ ($1.15\times$)** | **$+1,000.1\%$ ($11.00\times$)** | $+15.2\%$ |
| **RAM Footprint for Bytecode** | Full binary in flash | Mapped in virtual RAM | **$0\text{ Bytes}$ (Flash-backed)** | Mapped in virtual RAM |
| **Dynamic Heap Leakage** | N/A | $\Delta\text{RAM} = 0$ | **$\Delta\text{Heap} = 0\text{ B}$** | $\Delta\text{RAM} = 0$ |
| **In-Guest Spatial Safety** | ❌ Vulnerable (Canary corrupt) | ✅ Contained (Trapped) | ✅ Contained (Trapped) | ✅ Contained (Trapped) |
| **Control-Flow Integrity (CFI)** | ❌ Vulnerable (Func ptr hijacked)| ✅ Contained (Table checked) | ✅ Contained (Table checked) | ✅ Contained (Table checked) |
| **Host Function Fault Impact** | 💥 Process Crash (Kills all) | 💥 Process Crash (Kills all) | 💥 Board Reboot (WDT / Fatal) | ✅ **Recovered in $4.12\ \mu\text{s}$** |
| **Multi-Tenant Availability** | 0% Survival | 0% Survival | 0% Survival | **100% Survival (Carrier-Grade)** |

---

## How to Run & Reproduce

### 1. Flash and Run Live ESP8266 Benchmarks
Connect the NodeMCU ESP8266 via USB (CP210x on COM port) and run:
```bash
# Flash the firmware and execute on silicon
pio run -d esp8266 -t upload

# Or stream live telemetry and record CSV
python scripts/collect_esp8266.py
```

### 2. Run Host Native & Wasmtime Benchmarks
```bash
# Native host C benchmarks
python scripts/run_native_benchmarks.py

# Wasmtime Cranelift JIT benchmarks
python scripts/run_wasm_benchmarks.py
```

### 3. Run Adversarial Sandboxing Evaluation (RQ3)
```bash
# Run the 35-vector adversarial test suite
python security/tests/run_security_suite.py

# Generate security evaluation plots
python scripts/plot_security_results.py
```

### 4. Run SDRaD Fault Recovery & Multi-Tenant Resilience Demo
```bash
# Execute standalone C SDRaD benchmark
security/sdrad/sdrad_evaluator.exe

# Execute live Wasmtime multi-tenant integration
python security/sdrad/wasmtime_sdrad_integration.py

# Generate SDRaD recovery plots
python scripts/plot_sdrad_results.py
```

---

## Repository Structure

* [`esp8266/`](esp8266/): PlatformIO firmware implementing the native C benchmark suite and the on-device `wasm3` micro-interpreter runner.
  * [`src/main.cpp`](esp8266/src/main.cpp): Firmware entry point, UART telemetry exporter, and benchmark orchestrator.
  * [`src/wasm3_workload.cpp`](esp8266/src/wasm3_workload.cpp): Micro-interpreter lifecycle and 2 KB sub-page sandbox configuration.
  * [`include/workload_integer_wasm.h`](esp8266/include/workload_integer_wasm.h): Flash-backed 219-byte Wasm bytecode array.
* [`native/`](native/): Host native C benchmark harness instrumented with high-resolution Windows performance timers (`QueryPerformanceCounter`).
* [`wasm/`](wasm/): WebAssembly source workloads and compiled standalone `.wasm` binaries across `-O0` and `-O2`.
* [`security/`](security/):
  * [`tests/run_security_suite.py`](security/tests/run_security_suite.py): 35-vector adversarial test harness for spatial safety, CFI, DoS, and capability leakage.
  * [`tests/native_vulnerabilities.c`](security/tests/native_vulnerabilities.c): Bare-metal C exploit demonstrator (buffer overflow, canary overwrite, function pointer hijack).
  * [`sdrad/`](security/sdrad/): Secure Domain Rewind and Discard C library, vulnerable host service mock, and Wasmtime multi-tenant integration.
* [`data/`](data/): Verified, raw CSV datasets preserving full empirical integrity:
  * [`esp8266/esp8266_benchmarks_with_wasm3.csv`](data/esp8266/esp8266_benchmarks_with_wasm3.csv): 210 trials on physical ESP8266 silicon.
  * [`wasm/wasm_benchmarks.csv`](data/wasm/wasm_benchmarks.csv): 360 trials on Wasmtime.
  * [`security/security_evaluation.csv`](data/security/security_evaluation.csv): 35 adversarial containment trials.
  * [`security/sdrad_recovery_benchmarks.csv`](data/security/sdrad_recovery_benchmarks.csv): 60 fault recovery trials.
* [`plots/`](plots/): High-resolution empirical performance, security, and SDRaD recovery visualization figures.
* [`docs/`](docs/): Detailed research documentation:
  * [`findings.md`](docs/findings.md): Hardware constraints and the 64 KiB memory wall discovery.
  * [`runtime_investigation.md`](docs/runtime_investigation.md): Runtime evaluation matrix (Wasmtime vs Wasmer vs WAMR).
  * [`security_evaluation.md`](docs/security_evaluation.md): Full empirical report on RQ3 adversarial sandboxing.
  * [`sdrad_integration.md`](docs/sdrad_integration.md): Technical report on SDRaD in-process fault recovery.
* [`scripts/`](scripts/): Data collection, serial logging, and benchmark automation scripts.

---

## License

This project is licensed under the [MIT License](LICENSE).
