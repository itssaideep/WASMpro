/*
 * ============================================================
 * SecureWasm Edge — Workload 5: Simulated Sensor Processing
 * ============================================================
 * Exponential moving average (EMA) filter, synthetic noise,
 * and peak detection over time-series sensor telemetry.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_sensor(uint32_t iterations) {
    uint32_t acc_filtered = 0;
    uint32_t peak_count = 0;

    for (uint32_t it = 0; it < iterations; ++it) {
        /* Synthetic periodic sensor telemetry */
        uint32_t val = ((it * 73U + 17U) % 1024U);
        /* 7/8 previous + 1/8 new sample EMA filter */
        acc_filtered = ((acc_filtered * 7U + val) >> 3) & 0xFFFFU;
        if (acc_filtered > 600U) {
            peak_count++;
        }
    }

    return ((acc_filtered << 16) | (peak_count & 0xFFFFU)) & 0xFFFFFFFFU;
}
