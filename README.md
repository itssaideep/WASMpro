# SecureWasm Edge

### Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems



---

## Project Description

As distributed intelligence shifts toward the extreme edge of IoT and telecommunications infrastructure, executing third-party or untrusted software modules directly on resource-constrained embedded microcontrollers has become a critical challenge. Traditional native firmware (compiled C/C++) provides optimal performance and minimal memory usage but lacks memory isolation, hardware sandboxing, and runtime security boundaries, meaning a single pointer arithmetic bug or malicious payload can compromise the entire node.

**SecureWasm Edge** investigates **WebAssembly (WASM)** as a secure, portable, and memory-safe execution environment for resource-constrained edge computing. This research evaluates whether WebAssembly's software-fault isolation (SFI) guarantees, deterministic capabilities, and compact bytecode can be viably deployed on microcontrollers with severe silicon and memory limitations, and quantifies the exact performance and resource overhead incurred compared to bare-metal native code.

---

## Research Scope & Objectives

The project addresses four foundational research questions:

* **RQ1 (Execution Performance):** What is the exact execution latency and throughput penalty of running WebAssembly compared to native compiled C/C++ across compute-heavy, memory-intensive, and bitwise edge workloads?
* **RQ2 (Resource Consumption):** How does WebAssembly impact dynamic RAM consumption, heap allocation stability, and flash/storage footprint on microcontrollers with sub-100 KB SRAM?
* **RQ3 (Security & Sandboxing):** How effectively does the WebAssembly sandbox trap out-of-bounds memory accesses, illegal pointer dereferences, and prevent unauthorized host resource exploitation without hardware MMU support?
* **RQ4 (Execution Engine Trade-Offs):** What are the comparative performance and architectural trade-offs between host-level Just-In-Time (JIT) compilation (e.g., Cranelift/Wasmtime) and microcontroller-level micro-interpretation (e.g., `wasm3`)?

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
   * **Telemetry:** Microsecond-precision hardware timing (`micros()`), dynamic heap monitoring (`ESP.getFreeHeap()`), and serial telemetry capture.

2. **Host Control Tier:**
   * **Target:** Modern x86_64 workstation providing cycle-accurate performance counters and baseline comparison.
   * **Evaluations:** Native C compiled with LLVM Clang benchmarked against Wasmtime (v49.0.2) running the Cranelift JIT compiler across `-O0` and `-O2` optimization levels.

---

## Workload Suite

The framework implements six deterministic, architecture-neutral benchmark workloads representative of real-world edge sensing and edge analytics tasks:

1. **Workload 1 (Integer Arithmetic):** Triple Linear Congruential Generator (LCG) and non-linear bitwise mixing testing raw integer ALU performance.
2. **Workload 2 (Checksum / Cryptographic Primitive):** Custom Adler-32 / CRC hash computing sequential memory reduction over byte streams.
3. **Workload 3 (Array & Memory Access):** In-place bubble sort and array permutations measuring sequential and random access overhead.
4. **Workload 4 (Matrix / Data Transform):** Fixed-point 2D matrix transformation simulating sensor DSP signal processing.
5. **Workload 5 (Sensor Emulation):** Synthetic analog-to-digital converter (ADC) filtering, running moving average and noise thresholds.
6. **Workload 6 (Control Flow & State Machine):** Complex branching, nested state transitions, and switch tables testing branch prediction penalties.

*All workloads maintain strict mathematical determinism with bit-for-bit golden checksum verification across both architectures and all execution engines.*

---

## Core Findings & Architectural Discoveries

### 1. The 64 KiB Memory Wall on Embedded Silicon
The W3C WebAssembly core specification mandates that linear memory is allocated in discrete pages of **64 KiB ($65,536\text{ bytes}$)**. Microcontrollers in the sub-100 KB SRAM class (such as the ESP8266 with $\approx 51.5\text{ KB}$ available heap) experience immediate Out-Of-Memory (`OOM`) failure when attempting to instantiate standard WebAssembly modules. Standard runtimes (Wasmtime, Wasmer, standard WAMR) physically cannot execute in this hardware class.

### 2. The Micro-Interpreter Strategy (`wasm3`)
To overcome the memory wall, this research implements a micro-interpreter strategy using `wasm3`:
* **Flash-Backed Direct Execution:** WebAssembly bytecode is stored directly in 4 MB Flash memory (ROM) and decoded in place, requiring 0 bytes of RAM for module storage.
* **Sub-Page Memory Sandboxing:** Bypassing standard 64 KiB allocations via `runtime->memoryLimit = 2048` enables execution within a 2 KB RAM boundary with **zero heap leakage ($\Delta\text{Heap} = 0\text{ B}$)**.

### 3. JIT Compilation vs. Micro-Interpretation Trade-off
Empirical measurements quantify the exact cost of sandboxing across hardware tiers:
* **Host JIT Execution (x86_64 + Cranelift):** WebAssembly incurs only **$+15.1\%$ execution overhead ($1.15\times$)** relative to native compiled C.
* **Embedded Interpreted Execution (Xtensa + wasm3):** WebAssembly incurs an **$11.00\times$ execution slowdown (+1,000.1% overhead)** due to direct-threaded opcode dispatch loops on an 80 MHz core without dynamic JIT capabilities.
* **Industrial Takeaway:** WebAssembly provides near-native execution on edge gateways and microprocessors ($\ge 256\text{ KB}$ RAM), while ultra-constrained microcontrollers trade an order-of-magnitude in raw compute performance for portable execution and memory safety.

---

## Repository Structure

* [`esp8266/`](esp8266/): PlatformIO firmware implementing the native C benchmark suite and the on-device `wasm3` micro-interpreter runner.
* [`native/`](native/): Host native C benchmark harness instrumented with high-resolution Windows performance timers (`QueryPerformanceCounter`).
* [`wasm/`](wasm/): WebAssembly source workloads and compiled standalone `.wasm` binaries.
* [`data/`](data/): Verified, raw CSV datasets ($N = 210$ MCU trials and $N = 360$ host trials) preserving full empirical integrity.
* [`docs/`](docs/): Detailed research documentation, including [`findings.md`](docs/findings.md) (hardware constraints) and [`runtime_investigation.md`](docs/runtime_investigation.md) (runtime feasibility study).
* [`scripts/`](scripts/): Data collection, serial logging, and benchmark automation scripts.

---

## License

This project is licensed under the [MIT License](LICENSE).
