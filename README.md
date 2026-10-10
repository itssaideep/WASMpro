# SecureWasm Edge

**Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems**

> **Academic Context:** MSc Computer Science portfolio project — Blekinge Institute of Technology (BTH), Sweden  
> **Industrial Alignment:** Ericsson Master's Thesis Opportunity (Req 792028, Linköping, Sweden)  
> **Author:** Sai Deep  
> **Date:** October 2026  

---

## 1. Overview

**SecureWasm Edge** is an empirical, reproducible benchmarking framework designed to evaluate the performance, resource footprint, portability, and security isolation of **WebAssembly (WASM)** when deployed in resource-constrained IoT and embedded systems.

The framework benchmarks identical computational workloads across two execution paradigms:
1. **Bare-Metal Microcontroller Execution:** NodeMCU ESP8266EX (Xtensa LX106 @ 80 MHz, 80 KB SRAM, 4 MB Flash) comparing **Native C** against on-device interpreted WebAssembly (**`wasm3`**).
2. **Controlled Host Baseline:** Host x86_64 system comparing **Native C** against JIT-compiled WebAssembly (**Wasmtime / Cranelift**).

---

## 2. Research Questions

| ID | Research Question | Empirical Metric |
|:---|:---|:---|
| **RQ1** | How does WebAssembly compare with native C/C++ in raw execution latency and throughput? | Execution duration ($\mu\text{s}$), Relative Overhead ($\%$, Slowdown Factor) |
| **RQ2** | What additional memory and storage resources does WebAssembly require on constrained hardware? | Dynamic Heap Consumption ($\text{bytes}$), Binary Size ($\text{Flash/Disk}$) |
| **RQ3** | How effectively does the WebAssembly sandbox restrict unsafe behavior in embedded domains? | OOB memory isolation, null pointer traps, host isolation |
| **RQ4** | What are the architectural trade-offs between JIT compilation (Host) and micro-interpretation (MCU)? | JIT vs Interpreter overhead comparison, cold-start latency |

---

## 3. System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              SecureWasm Edge                                │
├──────────────────────────────────────┬──────────────────────────────────────┤
│      EMBEDDED TIER (NodeMCU ESP8266) │          HOST TIER (Windows x64)     │
│       Xtensa LX106 @ 80 MHz, 80 KB   │      Modern x86_64, Multi-core       │
├──────────────────┬───────────────────┼──────────────────┬───────────────────┤
│  Native C Engine │ wasm3 Interpreter │  Native C Engine │   Wasmtime JIT    │
│  Xtensa GCC -O2  │ Flash Bytecode    │  LLVM Clang -O2  │  Cranelift JIT    │
│  Hardware Timers │ M3 Metacode Stack │  QPC Sub-μs      │  Fuel Metering    │
│  Heap Tracking   │ Sub-page Memory   │  Working Set     │  Linear Sandbox   │
└──────────────────┴───────────────────┴──────────────────┴───────────────────┘
```

---

## 4. Key Experimental Findings

### Finding 1: The 64 KiB Memory Wall & Hardware Class Boundaries
* Standard WebAssembly (W3C MVP/2.0) allocates linear memory in discrete pages of **$64\text{ KiB}$ ($65,536\text{ bytes}$)**.
* The ESP8266 has only $\approx 51.5\text{ KB}$ of free heap at boot. Standard runtimes (Wasmtime, Wasmer, standard WAMR) cannot instantiate modules with linear memory without immediate Out-Of-Memory (`OOM`) exhaustion.
* **Industrial Recommendation (Ericsson 792028):** Standard WebAssembly with WASI system calls requires devices with at least **$256\text{ KB}$ SRAM** (e.g., ARM Cortex-M4/M33, nRF52840, STM32F4, ESP32). Devices with $\le 64\text{ KB}$ SRAM require specialized sub-page execution models.

### Finding 2: Breakthrough On-Device Execution with `wasm3`
* By executing bytecode directly from **Flash memory (ROM)** and enforcing `runtime->memoryLimit = 2048`, we achieved successful, deterministic on-device WebAssembly execution on the ESP8266.
* Full details documented in [`docs/findings.md`](docs/findings.md) and [`docs/runtime_investigation.md`](docs/runtime_investigation.md).

### Finding 3: Architectural Overhead Comparison (Host JIT vs. MCU Interpreter)
Comparing identical Workload 1 (Integer Arithmetic, 10,000 iterations):

| Environment | Architecture | Native Latency | WASM Latency | Overhead / Slowdown | Execution Model |
|:---|:---|:---:|:---:|:---:|:---|
| **Host x86_64** | Multi-core 3.5+ GHz | $5.30\ \mu\text{s}$ | $6.10\ \mu\text{s}$ | **$+15.1\%$ ($1.15\times$)** | Cranelift JIT |
| **NodeMCU ESP8266** | Xtensa LX106 @ 80 MHz | $4,877.53\ \mu\text{s}$ | $53,658.87\ \mu\text{s}$ | **$+1,000.1\%$ ($11.00\times$)** | wasm3 Micro-Interpreter |

> **Key Architectural Takeaway:** JIT compilation on capable hardware incurs negligible overhead ($\sim 15\%$), while strict interpretation on microcontrollers carries an order-of-magnitude penalty ($11\times$). However, `wasm3` execution maintains **0 B heap leakage** and **100% bit-for-bit mathematical determinism** (`0x0F6B7313`).

---

## 5. Summary of Experimental Datasets

All raw CSV experimental data is preserved under `data/`:

| Dataset Path | Platform | Trials | Pass Rate | Key Metrics |
|:---|:---|:---:|:---:|:---|
| [`data/esp8266/esp8266_benchmarks_with_wasm3.csv`](data/esp8266/esp8266_benchmarks_with_wasm3.csv) | ESP8266 | 210 | **100.0%** | Native vs wasm3, Hardware micros, Heap deltas |
| [`data/native/native_benchmarks_combined.csv`](data/native/native_benchmarks_combined.csv) | Host x64 | 360 | **100.0%** | Clang -O0 / -O2, QPC timing, Memory working set |
| [`data/wasm/wasm_benchmarks.csv`](data/wasm/wasm_benchmarks.csv) | Host x64 | 360 | **100.0%** | Wasmtime compilation, instantiation, exec time |

---

## 6. Repository Layout

```
WASMpro/
├── esp8266/                # ESP8266 PlatformIO project (Native + wasm3)
│   ├── src/                # main.cpp, wasm3_workload.cpp, workloads
│   ├── include/            # workload_integer_wasm.h (Flash bytecode)
│   └── platformio.ini      # NodeMCU v2 environment configuration
├── native/                 # Host native C benchmark suite (Component A)
│   └── src/                # Deterministic workloads & QueryPerformanceCounter runner
├── wasm/                   # WebAssembly modules & compilation toolchain (Component D)
│   └── modules/            # Standalone .wasm binaries (-O0 and -O2)
├── security/               # Controlled security experiments (Component E)
├── scripts/                # Automation, Serial collectors, and analysis
│   ├── collect_esp8266.py  # Live COM6 serial benchmark collector
│   ├── run_native_benchmarks.py # Host native benchmark harness
│   └── run_wasm_benchmarks.py   # Wasmtime benchmark runner
├── data/                   # Verified experimental datasets (CSV format)
├── docs/                   # Academic reports, findings, and runtime analysis
│   ├── findings.md         # Key findings & 64 KiB memory wall analysis
│   └── runtime_investigation.md # Feasibility study (Wasmtime, Wasmer, WAMR, wasm3)
└── results/                # Synthesis, plots, and industrial recommendations
```

---

## 7. Quick Start & Reproduction

### Prerequisites
* **Python:** 3.10+ (`pip install -r scripts/requirements.txt`)
* **LLVM/Clang:** 16+ with `wasm32` target support
* **PlatformIO Core:** 6.1+ (`pio --version`)
* **Hardware:** NodeMCU ESP8266 connected on `COM6` (or adjust `PORT` in scripts)

### Run ESP8266 Benchmarks (Native + wasm3)
```bash
# 1. Build and upload firmware to ESP8266
cd esp8266
pio run -t upload

# 2. Collect serial telemetry over COM6 (from repository root)
cd ..
python scripts/collect_esp8266.py
```

### Run Host Native Benchmarks
```bash
python scripts/run_native_benchmarks.py
```

### Run Host WebAssembly Benchmarks (Wasmtime)
```bash
python scripts/run_wasm_benchmarks.py
```

---

## 8. Current Implementation Status

- [x] **Phase 1: Environment Validation & Hardware Setup** (NodeMCU ESP8266, PlatformIO, Clang, Wasmtime)
- [x] **Phase 2: ESP8266 Native Benchmarks** (6 workloads $\times$ 30 trials, $N=180$, CV $< 0.1\%$)
- [x] **Phase 3: Host Native Benchmarks** (6 workloads $\times$ 2 opt levels $\times$ 30 trials, $N=360$)
- [x] **Phase 4: WASM Runtime Investigation & Feasibility** (64 KiB memory wall documented in `docs/`)
- [x] **Phase 5: Host Native vs. WASM Benchmarks** (Wasmtime JIT suite, 360 trials, overhead quantified)
- [x] **On-Device WASM Execution:** (`wasm3` micro-interpreter integrated on ESP8266, 210 trials, 100% pass)
- [ ] **Phase 7: Security Isolation Experiments** (Component E — Out-of-bounds traps, fuel limits, WASI isolation)
- [ ] **Phase 8: Analysis & Visualization Pipeline** (Component G — Charts, overhead curves, distribution plots)
- [ ] **Phase 9: Comprehensive Thesis Portfolio Report** (Component H — Synthesis for Ericsson Req 792028)

---

## 9. License

This project is licensed under the [MIT License](LICENSE).
