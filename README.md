# SecureWasm Edge

**Performance Benchmarking and Security Evaluation of WebAssembly in Resource-Constrained Embedded Systems**

> MSc Computer Science portfolio project — Blekinge Institute of Technology (BTH), Sweden  
> Aligned with Ericsson Master's Thesis Opportunity (Req 792028, Linköping)

---

## Overview

This project provides a reproducible experimental framework comparing native C/C++ execution with WebAssembly (WASM) execution, investigating performance, resource consumption, portability, and security isolation trade-offs relevant to embedded systems.

### Research Questions

| ID | Question |
|----|----------|
| RQ1 | How does WebAssembly compare with native C/C++ in execution performance? |
| RQ2 | What additional resources does WebAssembly require? |
| RQ3 | How effectively does WASM sandboxing restrict unsafe behavior? |
| RQ4 | How do security restrictions affect performance? |

## Architecture

```
┌─────────────────────────────────────────────────┐
│              SecureWasm Edge                     │
├──────────────┬──────────────┬───────────────────┤
│  ESP8266     │  Host Native │  Host WASM        │
│  (Xtensa)   │  (x86_64)    │  (Wasmtime/Wasmer)│
│  Native C   │  Native C    │  C → .wasm        │
│  Benchmarks  │  Benchmarks  │  Benchmarks       │
├──────────────┴──────────────┴───────────────────┤
│  Security Experiments (WASM sandbox tests)      │
├─────────────────────────────────────────────────┤
│  Python Experiment Runner & Analysis Pipeline   │
└─────────────────────────────────────────────────┘
```

## Repository Structure

```
WASMpro/
├── esp8266/          # ESP8266 native benchmarks (PlatformIO)
├── native/           # Host native C benchmarks
├── wasm/             # WASM workloads and build scripts
├── security/         # Controlled security experiments
├── scripts/          # Python runner, analysis, visualization
├── data/             # Raw experimental results
├── plots/            # Generated charts and figures
├── results/          # Processed results and recommendations
└── docs/             # Methodology, report, limitations
```

## Hardware & Software

| Component | Details |
|-----------|---------|
| Embedded target | NodeMCU ESP8266 (Xtensa LX106, 80 KB SRAM, 4 MB flash) |
| Host system | Windows 11 x64, 32 GB RAM |
| ESP8266 toolchain | PlatformIO + Arduino framework |
| Host compiler | GCC (MinGW-w64) |
| WASM compiler | Clang (`--target=wasm32-wasi`) |
| WASM runtimes | Wasmtime, Wasmer |
| Analysis | Python 3.10, pandas, matplotlib |

> **Note:** The ESP8266 cannot run WebAssembly runtimes due to architecture and memory limitations. It provides native-only benchmarks. All WASM experiments run on the Windows x64 host.

## Quick Start

### ESP8266 Benchmarks

```bash
cd esp8266
pio run                    # Build firmware
pio run -t upload          # Flash to ESP8266
pio device monitor         # View Serial output (CSV)
```

### Host Native Benchmarks

```bash
cd native
mkdir build && cd build
gcc -O2 -o benchmark ../src/*.c
./benchmark
```

### WASM Benchmarks

```bash
cd wasm
./build.ps1                # Compile C → .wasm
wasmtime modules/workload_integer.wasm
```

### Analysis

```bash
pip install -r scripts/requirements.txt
python scripts/analyze.py
```

## Current Status

- [x] Phase 1: Environment validation & repo skeleton
- [ ] Phase 2: ESP8266 native benchmarks
- [ ] Phase 3: Host native benchmarks
- [ ] Phase 4: WASM feasibility & toolchain
- [ ] Phase 5: Native-vs-WASM benchmarking
- [ ] Phase 6: Resource measurements
- [ ] Phase 7: Security experiments
- [ ] Phase 8: Analysis & visualization
- [ ] Phase 9: Documentation & report

## License

[MIT](LICENSE)
