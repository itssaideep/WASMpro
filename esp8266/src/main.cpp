/*
 * ============================================================
 * SecureWasm Edge — ESP8266 Native & on-device wasm3 Benchmark Suite
 * ============================================================
 * Executes:
 *   1. All 6 Deterministic Native C Workloads (30 trials each)
 *   2. On-Device wasm3 WebAssembly Execution for Workload 1 (30 trials)
 *
 * Provides:
 *   - Direct on-device comparison between Native C and WASM Interpreter
 *   - On-device initialization overhead measurement (wasm3 module parse & load)
 *   - Heap tracking via ESP.getFreeHeap()
 *   - Mathematical checksum verification (0x0F6B7313)
 *   - Structured CSV export over Serial (115200 baud)
 *
 * Target: NodeMCU v2 (ESP8266EX @ 80 MHz, Xtensa LX106)
 * ============================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "workloads.h"
#include "wasm3_workload.h"

#define NUM_TRIALS 30

typedef uint32_t (*WorkloadFn)(uint32_t);

struct WorkloadDef {
    const char* name;
    WorkloadFn fn;
    uint32_t iters;
    uint32_t expected_checksum;
};

static const WorkloadDef WORKLOADS[] = {
    {"integer",     run_workload_integer,     WORKLOAD_INTEGER_ITERS,     EXPECTED_CHECKSUM_INTEGER},
    {"checksum",    run_workload_checksum,    WORKLOAD_CHECKSUM_ITERS,    EXPECTED_CHECKSUM_CHECKSUM},
    {"array",       run_workload_array,       WORKLOAD_ARRAY_ITERS,       EXPECTED_CHECKSUM_ARRAY},
    {"transform",   run_workload_transform,   WORKLOAD_TRANSFORM_ITERS,   EXPECTED_CHECKSUM_TRANSFORM},
    {"sensor",      run_workload_sensor,      WORKLOAD_SENSOR_ITERS,      EXPECTED_CHECKSUM_SENSOR},
    {"controlflow", run_workload_controlflow, WORKLOAD_CONTROLFLOW_ITERS, EXPECTED_CHECKSUM_CONTROLFLOW}
};

static const size_t NUM_WORKLOADS = sizeof(WORKLOADS) / sizeof(WORKLOADS[0]);

void run_all_benchmarks() {
    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("SecureWasm Edge — ESP8266 Native & wasm3 Suite"));
    Serial.println(F("=================================================="));

    Serial.print(F("CONFIG:platform=esp8266,arch=xtensa-lx106,cpu_mhz="));
    Serial.print(ESP.getCpuFreqMHz());
    Serial.print(F(",sdk="));
    Serial.print(ESP.getSdkVersion());
    Serial.print(F(",free_heap_boot="));
    Serial.println(ESP.getFreeHeap());

    Serial.println(F("--- CSV DATA START ---"));
    Serial.println(F("record_type,experiment_id,workload,iterations,trial,duration_us,heap_before,heap_after,checksum,status"));

    /* --------------------------------------------------------
     * Part 1: Native C Workloads (6 workloads * 30 trials)
     * -------------------------------------------------------- */
    float native_integer_mean_us = 0.0f;

    for (size_t w = 0; w < NUM_WORKLOADS; ++w) {
        const WorkloadDef& def = WORKLOADS[w];

        yield();
        def.fn(def.iters); /* Warmup */

        unsigned long total_us = 0;

        for (uint32_t t = 0; t < NUM_TRIALS; ++t) {
            yield();

            uint32_t h_before = ESP.getFreeHeap();
            unsigned long t_start = micros();
            volatile uint32_t chk = def.fn(def.iters);
            unsigned long t_end = micros();
            uint32_t h_after = ESP.getFreeHeap();

            unsigned long duration = (t_end >= t_start) ? (t_end - t_start) : (0xFFFFFFFF - t_start + t_end);
            bool pass = (chk == def.expected_checksum);
            total_us += duration;

            Serial.print(F("BENCHMARK,esp8266-native,"));
            Serial.print(def.name);
            Serial.print(F(","));
            Serial.print(def.iters);
            Serial.print(F(","));
            Serial.print(t + 1);
            Serial.print(F(","));
            Serial.print(duration);
            Serial.print(F(","));
            Serial.print(h_before);
            Serial.print(F(","));
            Serial.print(h_after);
            Serial.print(F(",0x"));
            if (chk < 0x10000000) Serial.print(F("0"));
            Serial.print(chk, HEX);
            Serial.print(F(","));
            Serial.println(pass ? F("PASS") : F("FAIL"));

            delay(2);
        }

        if (w == 0) {
            native_integer_mean_us = (float)total_us / NUM_TRIALS;
        }
    }

    /* --------------------------------------------------------
     * Part 2: On-Device wasm3 Micro-Interpreter Execution
     * -------------------------------------------------------- */
    Serial.println(F("STATUS: Executing on-device wasm3 WebAssembly interpreter..."));

    unsigned long wasm3_total_exec_us = 0;
    unsigned long wasm3_total_init_us = 0;
    size_t wasm3_pass_count = 0;

    for (uint32_t t = 0; t < NUM_TRIALS; ++t) {
        yield();

        Wasm3Result wr = run_wasm3_integer(WORKLOAD_INTEGER_ITERS);
        bool pass = wr.success && (wr.checksum == EXPECTED_CHECKSUM_INTEGER);
        if (pass) wasm3_pass_count++;

        if (wr.error_msg != NULL && t == 0) {
            Serial.print(F("DEBUG_WASM3_ERROR: "));
            Serial.println(wr.error_msg);
        }

        wasm3_total_exec_us += wr.exec_duration_us;
        wasm3_total_init_us += wr.init_duration_us;

        Serial.print(F("BENCHMARK,esp8266-wasm3,integer,"));
        Serial.print(WORKLOAD_INTEGER_ITERS);
        Serial.print(F(","));
        Serial.print(t + 1);
        Serial.print(F(","));
        Serial.print(wr.exec_duration_us);
        Serial.print(F(","));
        Serial.print(wr.heap_before);
        Serial.print(F(","));
        Serial.print(wr.heap_after);
        Serial.print(F(",0x"));
        if (wr.checksum < 0x10000000) Serial.print(F("0"));
        Serial.print(wr.checksum, HEX);
        Serial.print(F(","));
        Serial.println(pass ? F("PASS") : F("FAIL"));

        delay(5);
    }

    Serial.println(F("--- CSV DATA END ---"));

    float wasm3_mean_exec_us = (float)wasm3_total_exec_us / NUM_TRIALS;
    float wasm3_mean_init_us = (float)wasm3_total_init_us / NUM_TRIALS;
    float overhead_ratio = (native_integer_mean_us > 0) ? (wasm3_mean_exec_us / native_integer_mean_us) : 0;

    Serial.println(F("=================================================="));
    Serial.println(F("ON-DEVICE COMPARISON (ESP8266 NATIVE vs wasm3):"));
    Serial.print(F("  Workload:               integer (10,000 iters)\n"));
    Serial.print(F("  Native C Mean Latency:  ")); Serial.print(native_integer_mean_us, 2); Serial.println(F(" us"));
    Serial.print(F("  wasm3 WASM Mean Exec:   ")); Serial.print(wasm3_mean_exec_us, 2); Serial.println(F(" us"));
    Serial.print(F("  wasm3 Module Init Mean: ")); Serial.print(wasm3_mean_init_us, 2); Serial.println(F(" us"));
    Serial.print(F("  wasm3 Overhead Ratio:   ")); Serial.print(overhead_ratio, 2); Serial.println(F("x"));
    Serial.print(F("  wasm3 Checksum Checks:  ")); Serial.print(wasm3_pass_count); Serial.print(F("/")); Serial.println(NUM_TRIALS);
    Serial.println(F("=================================================="));
    Serial.println(F("STATUS: ALL_BENCHMARKS_COMPLETE"));
    Serial.println(F("=================================================="));
}

void setup() {
    Serial.begin(115200);
    delay(500);

    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(20);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    run_all_benchmarks();

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    static unsigned long last_hb = 0;
    unsigned long now = millis();
    if (now - last_hb >= 5000) {
        last_hb = now;
        Serial.print(F("[idle_heartbeat] uptime_ms="));
        Serial.print(now);
        Serial.print(F(" heap="));
        Serial.println(ESP.getFreeHeap());
    }
    yield();
}
