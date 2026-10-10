# Component C: WebAssembly Runtime Feasibility & Compatibility Investigation

> **Project:** SecureWasm Edge  
> **Context:** MSc Computer Science, BTH, Sweden · Ericsson Thesis Prep (Req 792028)  
> **Author:** Sai Deep  
> **Date:** 2026-10-10  

---

## 1. Executive Summary

This investigation evaluates candidate WebAssembly (WASM) runtimes for resource-constrained embedded systems and establishes the technical justification for our experimental architecture. We evaluate **Wasmtime**, **Wasmer**, and **WAMR (WebAssembly Micro Runtime)** against the physical constraints of our hardware: the **NodeMCU ESP8266 (Xtensa LX106, 80 KB SRAM, 4 MB Flash)** and our host development environment (**Windows 11 x86_64**).

### Primary Finding & Hardware Incompatibility Proof

> [!IMPORTANT]
> **Definitive Conclusion on ESP8266 Compatibility:**  
> The NodeMCU ESP8266 **cannot execute standard WebAssembly runtimes**. Executing WASM on this board is physically infeasible due to fundamental memory architecture and ISA limitations (detailed in Section 3). Consequently, in accordance with the project guidelines, the ESP8266 is utilized strictly for **native C/C++ embedded baseline benchmarking**, while controlled **native-versus-WASM comparisons** are conducted on the host platform.

---

## 2. Comprehensive Runtime Evaluation Matrix

| Criterion | Wasmtime (v49.0.2) | Wasmer (v7.5) | WAMR (v2.x) |
|:---|:---|:---|:---|
| **Primary Maintainer** | Bytecode Alliance | Wasmer Inc. | Bytecode Alliance / Intel |
| **Target Domains** | Server, Cloud, Edge, CLI | Universal, Cloud, Edge | Embedded, IoT, Edge, Microcontrollers |
| **Supported Architectures** | x86_64, aarch64, s390x, riscv64 | x86_64, aarch64, riscv64 | x86, x86_64, ARM, AArch64, MIPS, RISC-V, Xtensa (ESP32 only) |
| **Execution Engines** | JIT (Cranelift) | JIT (Cranelift/Singlepass/LLVM), Interpreter | Classic Interpreter, Fast JIT, LLVM AOT |
| **Runtime Binary Footprint** | ~25 MB – 45 MB | ~30 MB – 60 MB | **50 KB – 300 KB** (Interpreter) |
| **Minimum Usable RAM** | ~15 MB – 30 MB | ~20 MB – 40 MB | **~100 KB – 256 KB** (Bare minimum) |
| **WASI Support** | WASI Preview 1 & Preview 2 (Component Model) | WASI Preview 1 & 2 | WASI Preview 1 (subset tailored for embedded) |
| **Sandboxing Capabilities** | Hardware guard pages, Memory bounds, Fuel metering, Epoch interruption | Memory bounds, Metering middleware, Sandbox limits | Linear memory bounds, Heap allocator sandbox, Execution limits |
| **Target Build Requirements** | Rust toolchain, C compiler, 64-bit OS | Rust toolchain, C compiler, 64-bit OS | CMake, GCC/Clang cross-toolchain |
| **Status in this Project** | **Adopted (Primary Host Runtime)** | **Evaluated** | **Investigated (Excluded due to target RAM limits)** |

---

## 3. Why the ESP8266 Cannot Execute Standard WebAssembly Runtimes

The ESP8266 microcontroller presents three insurmountable physical and architectural barriers to hosting standard WASM runtimes (Wasmtime, Wasmer, WAMR):

### Barrier 1: The "Memory Wall" (64 KiB Page Granularity vs. SRAM)
* In the standard WebAssembly specification (Wasm Core 1.0/2.0), **linear memory is allocated in discrete pages of 64 KiB (65,536 bytes)**.
* Even a minimal module declaring `(memory 1)` demands at least **64 KiB of contiguous RAM** for the module's linear memory instance alone.
* The ESP8266 has **80 KB of total SRAM**, partitioned into:
  - Instruction RAM (IRAM): ~32 KB
  - Data RAM (DRAM): ~48 KB
* At boot, the NonOS SDK, WiFi stacks, and Arduino core consume approximately 28 KB of DRAM, leaving **at most ~51,544 bytes of free heap**.
* **Mathematical Incompatibility:** 
  $$\text{Available Free Heap } (51.5\text{ KB}) < \text{Single WASM Page } (64.0\text{ KB})$$
  Allocating a single standard WebAssembly memory page causes immediate Out-Of-Memory (`OOM`) exhaustion before the runtime interpreter can even instantiate its internal execution stack.

### Barrier 2: Architecture & ISA Limitations (Xtensa LX106)
* The ESP8266 uses the **Cadence Tensilica Xtensa LX106** core (a 32-bit reduced RISC architecture).
* It **lacks hardware floating-point support** (all IEEE 754 operations must be emulated in software via GCC libgcc routines).
* It enforces a strict **Harvard architecture memory split**: code executing from Flash must be cached into IRAM in 32-bit aligned words. Unaligned accesses or writing to executable memory triggers immediate hardware fatal exceptions (`Fatal exception 0 / 9 / 28`).
* While WAMR has experimental support for **ESP32** (which features a dual-core Xtensa LX6 with 520 KB SRAM and MMU), **no production WASM runtime officially supports Xtensa LX106**.

### Barrier 3: Runtime Stack Overhead
* WAMR's minimal classic interpreter requires an execution thread stack of at least **16 KB to 32 KB** to process the operand stack and opcode dispatch loops.
* Arduino ESP8266 tasks are allocated a fixed **4 KB continuation stack**. Running any complex interpreter loop overflows this stack, resulting in silent memory corruption or watchdog timer (`WDT`) resets.

---

## 4. Architectural Solutions for Sub-64KB Systems

To address this "Memory Wall" in resource-constrained IoT devices, two distinct architectural paths exist:

### Path A: Hardware Class Boundary Definition (Ericsson Industrial Finding)
* For standard enterprise WASM runtimes (Wasmtime, Wasmer, standard WAMR) with full WASI POSIX capabilities, the **minimum viable hardware boundary is $\ge 256\text{ KB}$ SRAM** (e.g., ARM Cortex-M4/M33, nRF52840, STM32F4, or ESP32).
* Hardware below this boundary ($\le 64\text{ KB}$ SRAM) cannot support standard WebAssembly memory models without breaking the 64 KiB page abstraction.

### Path B: The Micro-Interpreter Strategy (`wasm3`)
* **wasm3** is a specialized universal WebAssembly micro-interpreter written in C.
* It bypasses the 64 KiB memory barrier through three techniques:
  1. **Flash-Backed Bytecode Storage:** Instead of loading the `.wasm` binary into scarce RAM, the bytecode is embedded in the 4 MB Flash memory (ROM) as a `const uint8_t[]` array and decoded directly from Flash address space.
  2. **Sub-Page Linear Memory:** `wasm3` allows modules with static memory requirements to execute within a small heap or stack buffer (~8 KB – 16 KB) rather than enforcing 64 KiB page allocations.
  3. **M3 Intermediate Metacode:** It transforms WebAssembly bytecode on-the-fly into high-density direct-threaded metacode, significantly reducing operand stack overhead.
* **Experimental Integration:** In this project, we implement and evaluate `wasm3` directly on the ESP8266 for Workload 1 to determine the exact execution overhead of interpreted WebAssembly on bare-metal Xtensa silicon.

---

## 5. Runtime Selection for the Experimental Suite

Based on these verified findings:

1. **Embedded Native Baseline (ESP8266):**
   - The ESP8266 executes small, deterministic C/C++ native workloads compiled directly to Xtensa machine code via `xtensa-lx106-elf-gcc`.
   - It provides real-world microcontroller measurements: hardware timing (`micros()`), heap behavior (`ESP.getFreeHeap()`), and firmware size.
   
2. **WebAssembly Execution Platform (Host Windows x86_64):**
   - Workloads are compiled to WebAssembly using **LLVM/Clang (`--target=wasm32`)**.
   - **Wasmtime (v49.0.2)** is selected as the primary runtime because it implements:
     - Robust JIT compilation via Cranelift
     - Strict linear memory sandboxing with hardware guard pages
     - Deterministic instruction metering (**Fuel**) for resource exhaustion experiments (Component E)
     - High-precision host performance timers and process isolation
   - **Wasmer** is analyzed for execution engine trade-offs (Singlepass vs Cranelift).

---

## 6. Methodological Validity & Confounding Variable Controls

To adhere to the engineering standards of this project:
* Measurements from the ESP8266 are **never directly plotted against host WASM measurements** as a 1:1 speed comparison, which would falsely conflate clock speed (80 MHz vs 3.5+ GHz) and ISA differences with runtime overhead.
* The native-versus-WASM execution overhead is evaluated **on the same host hardware** using identical source code, deterministic inputs, and matched optimization levels (`-O0` and `-O2`).
* All comparative metrics strictly observe:
  $$\text{WASM Overhead (\%)} = \left(\frac{T_{\text{WASM}} - T_{\text{Native}}}{T_{\text{Native}}}\right) \times 100$$
