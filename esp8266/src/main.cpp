/*
 * ============================================================
 * SecureWasm Edge — Complete ESP8266 Native Benchmark Suite
 * ============================================================
 * Component B: 6 Deterministic C/C++ Workloads
 *
 * Workloads:
 *   1. integer     — LCG arithmetic & non-linear perturbation (10,000 iters)
 *   2. checksum    — IEEE 802.3 CRC32 over 256 bytes (100 iters)
 *   3. array       — Array permutation & bubble sort (100 iters)
 *   4. transform   — S-box substitution & rotation (1,000 iters)
 *   5. sensor      — Simulated sensor EMA filter & peak detector (10,000 iters)
 *   6. controlflow — State machine branching dispatch (10,000 iters)
 *
 * Measurement Protocol:
 *   - WiFi OFF to eliminate background radio interrupts
 *   - Timer overhead calibration using micros()
 *   - 1 unrecorded warm-up trial per workload
 *   - 30 recorded trials per workload (180 total runs)
 *   - Free heap before & after via ESP.getFreeHeap()
 *   - Strict checksum verification against expected values
 *   - Structured CSV export over Serial (115200 baud)
 *
 * Target: NodeMCU v2 (ESP8266EX @ 80 MHz, Xtensa LX106)
 * ============================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "workloads.h"

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
    Serial.println(F("SecureWasm Edge — ESP8266 Native Benchmark Suite"));
    Serial.println(F("Full 6-Workload Execution"));
    Serial.println(F("=================================================="));

    Serial.print(F("CONFIG:platform=esp8266,arch=xtensa-lx106,cpu_mhz="));
    Serial.print(ESP.getCpuFreqMHz());
    Serial.print(F(",sdk="));
    Serial.print(ESP.getSdkVersion());
    Serial.print(F(",workloads="));
    Serial.print(NUM_WORKLOADS);
    Serial.print(F(",trials_per_workload="));
    Serial.println(NUM_TRIALS);

    /* Calibrate timer overhead */
    unsigned long cal0 = micros();
    unsigned long cal1 = micros();
    unsigned long timer_overhead_us = (cal1 >= cal0) ? (cal1 - cal0) : 0;
    Serial.print(F("CONFIG:timer_resolution_us=1,timer_overhead_us="));
    Serial.println(timer_overhead_us);

    Serial.println(F("--- CSV DATA START ---"));
    Serial.println(F("record_type,experiment_id,workload,iterations,trial,duration_us,heap_before,heap_after,checksum,status"));

    for (size_t w = 0; w < NUM_WORKLOADS; ++w) {
        const WorkloadDef& def = WORKLOADS[w];

        /* Warm-up run */
        yield();
        uint32_t warmup_chk = def.fn(def.iters);
        (void)warmup_chk;

        unsigned long total_us = 0;
        unsigned long min_us = 0xFFFFFFFF;
        unsigned long max_us = 0;
        size_t pass_count = 0;

        for (uint32_t t = 0; t < NUM_TRIALS; ++t) {
            yield();

            uint32_t h_before = ESP.getFreeHeap();
            unsigned long t_start = micros();
            volatile uint32_t chk = def.fn(def.iters);
            unsigned long t_end = micros();
            uint32_t h_after = ESP.getFreeHeap();

            unsigned long duration = (t_end >= t_start) ? (t_end - t_start) : (0xFFFFFFFF - t_start + t_end);

            bool pass = (chk == def.expected_checksum);
            if (pass) pass_count++;

            total_us += duration;
            if (duration < min_us) min_us = duration;
            if (duration > max_us) max_us = duration;

            /* CSV row */
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

            delay(5);
        }

        /* Workload Summary */
        float mean_us = (float)total_us / NUM_TRIALS;
        Serial.print(F("WORKLOAD_SUMMARY,"));
        Serial.print(def.name);
        Serial.print(F(",trials="));
        Serial.print(NUM_TRIALS);
        Serial.print(F(",mean_us="));
        Serial.print(mean_us, 2);
        Serial.print(F(",min_us="));
        Serial.print(min_us);
        Serial.print(F(",max_us="));
        Serial.print(max_us);
        Serial.print(F(",passed="));
        Serial.print(pass_count);
        Serial.print(F("/"));
        Serial.println(NUM_TRIALS);
    }

    Serial.println(F("--- CSV DATA END ---"));
    Serial.println(F("=================================================="));
    Serial.println(F("STATUS: ALL_WORKLOADS_COMPLETE"));
    Serial.println(F("=================================================="));
}

void setup() {
    Serial.begin(115200);
    delay(500);

    /* Turn off WiFi completely */
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(20);

    /* Turn on LED while running benchmarks */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    run_all_benchmarks();

    digitalWrite(LED_BUILTIN, HIGH); /* OFF when done */
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
