"""
SecureWasm Edge — Complete ESP8266 Benchmark Collector (Native + wasm3)
Captures both native C workloads and on-device wasm3 WebAssembly execution,
validates mathematical checksums, records heap consumption,
and prints comparative statistics.
"""

import sys
import time
import os
import serial
import pandas as pd

PORT = "COM6"
BAUD = 115200
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "esp8266")
CSV_OUTPUT = os.path.join(OUTPUT_DIR, "esp8266_benchmarks_with_wasm3.csv")

def collect():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"Connecting to ESP8266 on {PORT} at {BAUD} baud...")

    try:
        s = serial.Serial(PORT, BAUD, timeout=2)
    except Exception as e:
        print(f"Error opening port {PORT}: {e}")
        return False

    print("Resetting ESP8266 to execute complete suite...")
    s.setDTR(False)
    s.setRTS(True)
    s.setDTR(False)
    time.sleep(0.15)
    s.setRTS(False)
    s.setDTR(False)
    time.sleep(0.25)

    csv_lines = []
    collecting_csv = False
    done = False
    start_time = time.time()

    print("Streaming output from ESP8266...")
    while time.time() - start_time < 45 and not done:
        line_bytes = s.readline()
        if not line_bytes:
            continue
        try:
            line = line_bytes.decode("ascii", errors="ignore").rstrip()
        except Exception:
            continue

        if not line:
            continue

        if "--- CSV DATA START ---" in line:
            collecting_csv = True
            print("  [STREAM] Captured CSV header...")
            continue
        elif "--- CSV DATA END ---" in line:
            collecting_csv = False
            print("  [STREAM] Captured CSV footer.")
            continue

        if collecting_csv:
            if line.startswith("record_type") or line.startswith("BENCHMARK"):
                csv_lines.append(line)
        else:
            if "STATUS:" in line or "CONFIG:" in line or "ON-DEVICE" in line or "  " in line:
                print(f"  [ESP8266] {line}")

        if "STATUS: ALL_BENCHMARKS_COMPLETE" in line:
            done = True

    s.close()

    if len(csv_lines) <= 1:
        print("ERROR: No CSV data captured from Serial!")
        return False

    with open(CSV_OUTPUT, "w", newline="", encoding="utf-8") as f:
        f.write("\n".join(csv_lines) + "\n")

    print(f"\n[OK] Raw CSV saved to: {CSV_OUTPUT} ({len(csv_lines)-1} benchmark rows)")

    try:
        df = pd.read_csv(CSV_OUTPUT)
        df = df[df["record_type"] == "BENCHMARK"].copy()
        df["duration_us"] = pd.to_numeric(df["duration_us"])
        df["iterations"] = pd.to_numeric(df["iterations"])
        df["trial"] = pd.to_numeric(df["trial"])
        df["heap_before"] = pd.to_numeric(df["heap_before"])
        df["heap_after"] = pd.to_numeric(df["heap_after"])

        print("\n==========================================================================================================")
        print("                   SECUREWASM EDGE — ON-DEVICE ESP8266 BENCHMARK RESULTS (NATIVE vs wasm3)                ")
        print("==========================================================================================================")
        print("Platform:      NodeMCU v2 (ESP8266EX @ 80 MHz, Xtensa LX106, 80 KB SRAM, 4 MB Flash)")
        print(f"Total Trials:  {len(df)} total runs")
        print(f"Pass Rate:     {(df['status'] == 'PASS').sum()} / {len(df)} ({(df['status'] == 'PASS').mean()*100:.1f}%)")
        print("----------------------------------------------------------------------------------------------------------")

        summary_rows = []
        for (exp_id, wl), group in df.groupby(["experiment_id", "workload"], sort=False):
            mean_us = group["duration_us"].mean()
            median_us = group["duration_us"].median()
            std_us = group["duration_us"].std()
            cv_pct = (std_us / mean_us) * 100 if mean_us > 0 else 0
            min_us = group["duration_us"].min()
            max_us = group["duration_us"].max()
            chk = group["checksum"].iloc[0]
            passed = (group["status"] == "PASS").all()
            heap_before = group["heap_before"].iloc[0]
            heap_after = group["heap_after"].iloc[-1]
            heap_delta = heap_after - heap_before

            summary_rows.append({
                "Engine": exp_id,
                "Workload": wl,
                "Iterations": group["iterations"].iloc[0],
                "N": len(group),
                "Mean (us)": f"{mean_us:.2f}",
                "Median (us)": f"{median_us:.1f}",
                "StdDev (us)": f"{std_us:.2f}",
                "CV (%)": f"{cv_pct:.2f}%",
                "Min (us)": min_us,
                "Max (us)": max_us,
                "Checksum": chk,
                "Verify": "PASS" if passed else "FAIL",
                "Heap Delta": f"{heap_delta} B"
            })

        sum_df = pd.DataFrame(summary_rows)
        print(sum_df.to_string(index=False))

        # Direct on-device comparison for Workload 1 (integer)
        native_int = df[(df["experiment_id"] == "esp8266-native") & (df["workload"] == "integer")]["duration_us"].mean()
        wasm3_int = df[(df["experiment_id"] == "esp8266-wasm3") & (df["workload"] == "integer")]["duration_us"].mean()

        if native_int > 0 and wasm3_int > 0:
            ratio = wasm3_int / native_int
            overhead_pct = ((wasm3_int - native_int) / native_int) * 100.0
            print("\n----------------------------------------------------------------------------------------------------------")
            print(f"DIRECT ON-DEVICE WORKLOAD 1 (INTEGER) COMPARISON ON ESP8266:")
            print(f"  Native C (Xtensa GCC -O2):           {native_int:.2f} us (~{native_int/1000:.2f} ms)")
            print(f"  WASM Interpreter (wasm3 in Flash):  {wasm3_int:.2f} us (~{wasm3_int/1000:.2f} ms)")
            print(f"  On-Device Interpreter Slowdown:      {ratio:.2f}x ({overhead_pct:+.1f}% overhead)")
            print("----------------------------------------------------------------------------------------------------------")

        print("==========================================================================================================")
        return True
    except Exception as e:
        print(f"Analysis error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    success = collect()
    sys.exit(0 if success else 1)
