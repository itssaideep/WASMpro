/*
 * ============================================================
 * SecureWasm Edge — Host Native C Benchmark Suite (Component A)
 * ============================================================
 * Executes all 6 deterministic workloads natively on Windows x64.
 *
 * Timing:
 *   QueryPerformanceCounter (Windows high-resolution timer, sub-microsecond precision)
 *
 * Memory:
 *   GetProcessMemoryInfo (WorkingSetSize & PeakWorkingSetSize via psapi)
 *
 * Protocol:
 *   - 1 unrecorded warm-up trial per workload
 *   - 30 recorded trials per workload (N=180 total runs)
 *   - Strict 32-bit checksum verification against expected mathematical values
 *   - Structured CSV export to stdout
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>
#include "workloads.h"

#define NUM_TRIALS 30

#ifndef OPT_LEVEL
#define OPT_LEVEL "O2"
#endif

typedef uint32_t (*WorkloadFn)(uint32_t);

typedef struct {
    const char* name;
    WorkloadFn fn;
    uint32_t iters;
    uint32_t expected_checksum;
} WorkloadDef;

static const WorkloadDef WORKLOADS[] = {
    {"integer",     run_workload_integer,     WORKLOAD_INTEGER_ITERS,     EXPECTED_CHECKSUM_INTEGER},
    {"checksum",    run_workload_checksum,    WORKLOAD_CHECKSUM_ITERS,    EXPECTED_CHECKSUM_CHECKSUM},
    {"array",       run_workload_array,       WORKLOAD_ARRAY_ITERS,       EXPECTED_CHECKSUM_ARRAY},
    {"transform",   run_workload_transform,   WORKLOAD_TRANSFORM_ITERS,   EXPECTED_CHECKSUM_TRANSFORM},
    {"sensor",      run_workload_sensor,      WORKLOAD_SENSOR_ITERS,      EXPECTED_CHECKSUM_SENSOR},
    {"controlflow", run_workload_controlflow, WORKLOAD_CONTROLFLOW_ITERS, EXPECTED_CHECKSUM_CONTROLFLOW}
};

static const size_t NUM_WORKLOADS = sizeof(WORKLOADS) / sizeof(WORKLOADS[0]);

static size_t get_working_set_bytes(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

static size_t get_peak_working_set_bytes(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    const char* opt = OPT_LEVEL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--opt") == 0 && i + 1 < argc) {
            opt = argv[++i];
        }
    }

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double freq_d = (double)freq.QuadPart;

    char exp_id[64];
    snprintf(exp_id, sizeof(exp_id), "native-host-%s", opt);

    fprintf(stderr, "==================================================\n");
    fprintf(stderr, "SecureWasm Edge — Host Native Benchmark Suite\n");
    fprintf(stderr, "Optimization Level: -%s\n", opt);
    fprintf(stderr, "Timer Frequency:    %.2f MHz\n", freq_d / 1000000.0);
    fprintf(stderr, "==================================================\n");

    printf("record_type,experiment_id,workload,iterations,trial,duration_us,memory_bytes,peak_memory_bytes,checksum,status\n");

    int all_passed = 1;

    for (size_t w = 0; w < NUM_WORKLOADS; ++w) {
        const WorkloadDef* def = &WORKLOADS[w];

        /* Warm-up run */
        volatile uint32_t warm_chk = def->fn(def->iters);
        (void)warm_chk;

        double total_us = 0.0;
        double min_us = 1e12;
        double max_us = 0.0;
        size_t pass_count = 0;

        for (uint32_t t = 1; t <= NUM_TRIALS; ++t) {
            size_t mem_before = get_working_set_bytes();

            LARGE_INTEGER t0, t1;
            QueryPerformanceCounter(&t0);
            volatile uint32_t chk = def->fn(def->iters);
            QueryPerformanceCounter(&t1);

            size_t peak_mem = get_peak_working_set_bytes();

            double duration_us = (double)(t1.QuadPart - t0.QuadPart) * 1000000.0 / freq_d;
            bool pass = (chk == def->expected_checksum);
            if (pass) pass_count++;
            else all_passed = 0;

            total_us += duration_us;
            if (duration_us < min_us) min_us = duration_us;
            if (duration_us > max_us) max_us = duration_us;

            printf("BENCHMARK,%s,%s,%u,%u,%.2f,%zu,%zu,0x%08X,%s\n",
                   exp_id, def->name, def->iters, t, duration_us,
                   mem_before, peak_mem, chk, pass ? "PASS" : "FAIL");
        }

        double mean_us = total_us / NUM_TRIALS;
        fprintf(stderr, "  %-12s: mean=%.2f us (min=%.2f, max=%.2f) [%zu/%d PASS]\n",
                def->name, mean_us, min_us, max_us, pass_count, NUM_TRIALS);
    }

    fprintf(stderr, "==================================================\n");
    fprintf(stderr, "STATUS: %s\n", all_passed ? "NATIVE_BENCHMARKS_SUCCESS" : "VERIFICATION_FAILURE");
    fprintf(stderr, "==================================================\n");

    return all_passed ? 0 : 1;
}
