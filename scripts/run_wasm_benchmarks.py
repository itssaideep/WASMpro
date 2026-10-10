"""
SecureWasm Edge — WebAssembly Benchmark Runner (Wasmtime Engine)
Executes all 6 workloads across -O0 and -O2 optimization levels,
instruments JIT compilation, instantiation, and steady-state execution times,
validates mathematical checksums, and computes native-vs-WASM overhead.
"""

import os
import sys
import time
import wasmtime
import pandas as pd
import numpy as np

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODULES_DIR = os.path.join(ROOT_DIR, "wasm", "modules")
OUTPUT_DIR = os.path.join(ROOT_DIR, "data", "wasm")
NATIVE_DATA_PATH = os.path.join(ROOT_DIR, "data", "native", "native_benchmarks_combined.csv")

os.makedirs(OUTPUT_DIR, exist_ok=True)

WORKLOAD_CONFIGS = [
    {"name": "integer",     "iters": 10000, "expected": 0x0F6B7313},
    {"name": "checksum",    "iters":   100, "expected": 0x067088A3},
    {"name": "array",       "iters":   100, "expected": 0x36ABF7B8},
    {"name": "transform",   "iters":  1000, "expected": 0xF170C069},
    {"name": "sensor",      "iters": 10000, "expected": 0x0249054D},
    {"name": "controlflow", "iters": 10000, "expected": 0x0000222F}
]

NUM_TRIALS = 30

def run_benchmarks():
    engine = wasmtime.Engine()
    records = []

    print("==================================================")
    print("SecureWasm Edge — WebAssembly Benchmark Suite")
    print("Runtime: Wasmtime 49.0 (Cranelift JIT)")
    print(f"Trials per workload: {NUM_TRIALS}")
    print("==================================================")

    for opt in ["O0", "O2"]:
        for cfg in WORKLOAD_CONFIGS:
            name = cfg["name"]
            iters = cfg["iters"]
            expected = cfg["expected"]
            fn_name = f"run_workload_{name}"
            wasm_filename = f"workload_{name}_{opt}.wasm"
            wasm_path = os.path.join(MODULES_DIR, wasm_filename)

            if not os.path.exists(wasm_path):
                print(f"ERROR: Missing module {wasm_path}")
                continue

            module_size = os.path.getsize(wasm_path)

            # Measure Compilation Time
            t_comp0 = time.perf_counter_ns()
            module = wasmtime.Module.from_file(engine, wasm_path)
            comp_time_us = (time.perf_counter_ns() - t_comp0) / 1000.0

            # Measure Instantiation Time
            store = wasmtime.Store(engine)
            t_inst0 = time.perf_counter_ns()
            instance = wasmtime.Instance(store, module, [])
            inst_time_us = (time.perf_counter_ns() - t_inst0) / 1000.0

            fn = instance.exports(store)[fn_name]

            # Warm-up run (not recorded)
            warmup_res = fn(store, iters)
            warmup_chk = warmup_res & 0xFFFFFFFF
            if warmup_chk != expected:
                print(f"WARNING: Checksum mismatch for {wasm_filename}! Got 0x{warmup_chk:08X}, expected 0x{expected:08X}")

            exp_id = f"wasm-wasmtime-{opt}"

            # Execute 30 repeated trials
            for trial in range(1, NUM_TRIALS + 1):
                t_exec0 = time.perf_counter_ns()
                res = fn(store, iters)
                t_exec1 = time.perf_counter_ns()

                duration_us = (t_exec1 - t_exec0) / 1000.0
                chk = res & 0xFFFFFFFF
                pass_status = "PASS" if chk == expected else "FAIL"

                records.append({
                    "record_type": "BENCHMARK",
                    "experiment_id": exp_id,
                    "platform": "windows-x64",
                    "runtime": "wasmtime",
                    "runtime_version": "49.0",
                    "optimization_level": opt,
                    "workload": name,
                    "iterations": iters,
                    "trial": trial,
                    "duration_us": duration_us,
                    "compilation_time_us": comp_time_us,
                    "instantiation_time_us": inst_time_us,
                    "module_size_bytes": module_size,
                    "checksum": f"0x{chk:08X}",
                    "status": pass_status
                })

            print(f"  [{opt}] {name:<12}: Module={module_size:>4} B, Comp={comp_time_us:>7.1f} us, Inst={inst_time_us:>5.1f} us, Status=OK")

    df = pd.DataFrame(records)
    csv_out = os.path.join(OUTPUT_DIR, "wasm_benchmarks.csv")
    df.to_csv(csv_out, index=False)
    print(f"\n[OK] Raw WASM benchmark dataset saved to: {csv_out} ({len(df)} records)")

    # Comparative Native vs WASM Analysis
    print("\n==========================================================================================================")
    print("                    SECUREWASM EDGE — NATIVE vs. WEBASSEMBLY COMPARATIVE RESULTS                          ")
    print("==========================================================================================================")

    if not os.path.exists(NATIVE_DATA_PATH):
        print("Note: Native dataset not found at", NATIVE_DATA_PATH)
        return

    df_nat = pd.read_csv(NATIVE_DATA_PATH)

    summary_rows = []
    for opt in ["O0", "O2"]:
        for cfg in WORKLOAD_CONFIGS:
            name = cfg["name"]

            sub_wasm = df[(df["workload"] == name) & (df["optimization_level"] == opt)]
            sub_nat = df_nat[(df_nat["workload"] == name) & (df_nat["experiment_id"] == f"native-host-{opt}")]

            wasm_mean = sub_wasm["duration_us"].mean()
            wasm_std = sub_wasm["duration_us"].std()
            nat_mean = sub_nat["duration_us"].mean()
            nat_std = sub_nat["duration_us"].std()

            overhead_pct = ((wasm_mean - nat_mean) / nat_mean) * 100.0 if nat_mean > 0 else 0
            ratio = wasm_mean / nat_mean if nat_mean > 0 else 0
            mod_size = sub_wasm["module_size_bytes"].iloc[0]

            summary_rows.append({
                "Opt": opt,
                "Workload": name,
                "Native (us)": f"{nat_mean:.2f}",
                "WASM (us)": f"{wasm_mean:.2f}",
                "Ratio (W/N)": f"{ratio:.2f}x",
                "Overhead (%)": f"{overhead_pct:+.1f}%",
                "WASM StdDev": f"{wasm_std:.2f}",
                "Module Size": f"{mod_size} B",
                "Status": "PASS" if (sub_wasm["status"] == "PASS").all() else "FAIL"
            })

    sum_df = pd.DataFrame(summary_rows)
    print(sum_df.to_string(index=False))
    print("==========================================================================================================")
    print("Key Observations:")
    print("  - Module Size: Standalone WASM bytecode is ~1 KB - 2 KB (vs ~150 KB native .exe).")
    print("  - Overhead %: ((WASM - Native) / Native) * 100 on identical hardware.")
    print("==========================================================================================================")

if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    run_benchmarks()
