"""
SecureWasm Edge — Host Native Benchmark Runner
Executes native benchmarks for both -O0 and -O2 optimization levels,
validates mathematical checksums against expected values,
measures WorkingSet and PeakWorkingSet memory,
and exports clean datasets for cross-platform comparison.
"""

import subprocess
import os
import sys
import pandas as pd

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(ROOT_DIR, "native", "build")
OUTPUT_DIR = os.path.join(ROOT_DIR, "data", "native")

os.makedirs(OUTPUT_DIR, exist_ok=True)

def run_suite(exe_name, opt_label):
    exe_path = os.path.join(BUILD_DIR, exe_name)
    csv_file = os.path.join(OUTPUT_DIR, f"native_benchmarks_{opt_label}.csv")

    print(f"\n==================================================")
    print(f"Running Host Native Benchmarks: -{opt_label}")
    print(f"Executable: {exe_path}")
    print(f"==================================================")

    res = subprocess.run([exe_path], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"ERROR: Execution failed with code {res.returncode}")
        print(res.stderr)
        return None

    # Print stderr summary from the C harness
    print(res.stderr.strip())

    # Save CSV
    with open(csv_file, "w", newline="", encoding="utf-8") as f:
        f.write(res.stdout)

    print(f"[OK] Raw CSV saved to: {csv_file}")
    df = pd.read_csv(csv_file)
    return df

def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    df_o0 = run_suite("benchmark_O0.exe", "O0")
    df_o2 = run_suite("benchmark_O2.exe", "O2")

    if df_o0 is None or df_o2 is None:
        sys.exit(1)

    combined_df = pd.concat([df_o0, df_o2], ignore_index=True)
    combined_path = os.path.join(OUTPUT_DIR, "native_benchmarks_combined.csv")
    combined_df.to_csv(combined_path, index=False)
    print(f"\n[OK] Combined native dataset saved to: {combined_path} ({len(combined_df)} rows)")

    # Print comparative analysis
    print("\n==========================================================================================================")
    print("                     SECUREWASM EDGE — HOST NATIVE BENCHMARK RESULTS (WINDOWS x64)                         ")
    print("==========================================================================================================")
    print(f"Compiler: Clang 23.1.3 (Target: x86_64-pc-windows-msvc)")
    print(f"Total Trials: {len(combined_df)} runs (30 trials * 6 workloads * 2 optimization levels)")
    print(f"Pass Rate:    {(combined_df['status'] == 'PASS').sum()} / {len(combined_df)} ({(combined_df['status'] == 'PASS').mean()*100:.1f}%)")
    print("----------------------------------------------------------------------------------------------------------")

    rows = []
    for wl in df_o0["workload"].unique():
        sub_o0 = df_o0[df_o0["workload"] == wl]
        sub_o2 = df_o2[df_o2["workload"] == wl]

        mean_o0 = sub_o0["duration_us"].mean()
        std_o0 = sub_o0["duration_us"].std()
        mean_o2 = sub_o2["duration_us"].mean()
        std_o2 = sub_o2["duration_us"].std()

        speedup = (mean_o0 / mean_o2) if mean_o2 > 0 else 0
        chk = sub_o2["checksum"].iloc[0]
        pass_o0 = (sub_o0["status"] == "PASS").all()
        pass_o2 = (sub_o2["status"] == "PASS").all()
        peak_ram_mb = sub_o2["peak_memory_bytes"].iloc[0] / (1024 * 1024)

        rows.append({
            "Workload": wl,
            "Iterations": sub_o0["iterations"].iloc[0],
            "Mean -O0 (us)": f"{mean_o0:.2f}",
            "StdDev -O0": f"{std_o0:.2f}",
            "Mean -O2 (us)": f"{mean_o2:.2f}",
            "StdDev -O2": f"{std_o2:.2f}",
            "Speedup (-O2/-O0)": f"{speedup:.2f}x",
            "Peak RSS": f"{peak_ram_mb:.2f} MB",
            "Checksum": chk,
            "Verify": "PASS" if (pass_o0 and pass_o2) else "FAIL"
        })

    summary_df = pd.DataFrame(rows)
    print(summary_df.to_string(index=False))
    print("==========================================================================================================")

if __name__ == "__main__":
    main()
