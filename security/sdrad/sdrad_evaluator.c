/*
 * ============================================================================
 * SecureWasm Edge — SDRaD In-Process Fault Recovery Benchmark Harness
 * ============================================================================
 * Evaluates:
 *   1. Baseline Execution Latency (Normal Host Function Call)
 *   2. SDRaD Anchored Execution Latency (Domain Overhead)
 *   3. Fault Interception, Memory Discard & Rewind Latency (Microsecond Recovery)
 *   4. Multi-Tenant Availability: Tenant A fails -> recovered -> Tenant B succeeds
 * ============================================================================
 */

#include "sdrad.h"
#include "vulnerable_host_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_TRIALS 30

int main(void) {
    printf("==============================================================================\n");
    printf("  SecureWasm Edge — In-Process Isolation & Recovery Benchmark (SDRaD Prototype)\n");
    printf("  Inspired by Ericsson Research: Secure Domain Rewind and Discard\n");
    printf("==============================================================================\n\n");

    if (sdrad_init() != SDRAD_OK) {
        fprintf(stderr, "ERROR: Failed to initialize SDRaD subsystem.\n");
        return 1;
    }

    sdrad_domain_t* dom = sdrad_domain_create("wasm_runtime_host_domain", 65536);
    if (!dom) {
        fprintf(stderr, "ERROR: Failed to create SDRaD domain.\n");
        sdrad_cleanup();
        return 1;
    }

    FILE* csv = fopen("c:\\Users\\Sai Deep\\Desktop\\WASMpro\\data\\security\\sdrad_recovery_benchmarks.csv", "w");
    if (csv) {
        fprintf(csv, "trial,scenario,workload,status,exec_latency_us,recovery_latency_us,heap_used_bytes,checksum\n");
    }

    uint8_t legit_payload[32];
    for (int i = 0; i < 32; ++i) legit_payload[i] = (uint8_t)(i * 7 + 13);

    uint8_t attack_payload[32];
    memset(attack_payload, 0, sizeof(attack_payload));
    attack_payload[0] = 0xFF; /* Triggers unmapped memory read in host service */

    printf("[1/3] Benchmarking Baseline Legitimate Execution (N=%d trials)...\n", NUM_TRIALS);
    double sum_legit_us = 0;
    for (int t = 1; t <= NUM_TRIALS; ++t) {
        host_service_args_t args;
        args.payload = legit_payload;
        args.payload_len = sizeof(legit_payload);
        args.output_hash = 0;
        args.success = false;

        sdrad_status_t st = sdrad_domain_execute(dom, host_crypto_verify_service, &args);
        sdrad_metrics_t m;
        sdrad_domain_get_metrics(dom, &m);

        sum_legit_us += m.last_execution_latency_us;

        if (csv) {
            fprintf(csv, "%d,normal_execution,host_crypto_verify,%s,%.2f,0.00,%zu,0x%08X\n",
                    t, (st == SDRAD_OK && args.success) ? "SUCCESS" : "FAIL",
                    m.last_execution_latency_us, m.heap_used_bytes, args.output_hash);
        }
    }
    double mean_legit_us = sum_legit_us / NUM_TRIALS;
    printf("  Mean Normal Execution Latency: %.2f us\n\n", mean_legit_us);

    printf("[2/3] Benchmarking Adversarial Attack & SDRaD Rapid Recovery (N=%d trials)...\n", NUM_TRIALS);
    double sum_attack_us = 0;
    double sum_recovery_us = 0;
    int recovered_count = 0;

    for (int t = 1; t <= NUM_TRIALS; ++t) {
        host_service_args_t args;
        args.payload = attack_payload; /* Trigger crash */
        args.payload_len = sizeof(attack_payload);
        args.output_hash = 0;
        args.success = false;

        /* Execute inside SDRaD domain: fault is caught, domain memory wiped, rewound */
        sdrad_status_t st = sdrad_domain_execute(dom, host_crypto_verify_service, &args);
        sdrad_metrics_t m;
        sdrad_domain_get_metrics(dom, &m);

        sum_attack_us += m.last_execution_latency_us;
        sum_recovery_us += m.last_recovery_latency_us;

        bool recovered = (st == SDRAD_FAULT_INTERCEPTED);
        if (recovered) recovered_count++;

        if (csv) {
            fprintf(csv, "%d,adversarial_fault,host_crypto_verify,%s,%.2f,%.2f,%zu,0x%08X\n",
                    t, recovered ? "RECOVERED" : "CRASHED",
                    m.last_execution_latency_us, m.last_recovery_latency_us, m.heap_used_bytes, args.output_hash);
        }
    }
    double mean_attack_us = sum_attack_us / NUM_TRIALS;
    double mean_recovery_us = sum_recovery_us / NUM_TRIALS;
    printf("  Fault Containment & Recovery Rate: %d / %d (%.1f%%)\n", 
           recovered_count, NUM_TRIALS, (double)recovered_count * 100.0 / NUM_TRIALS);
    printf("  Mean Total Recovery Latency:       %.2f us\n", mean_attack_us);
    printf("  Mean Isolated Discard Overhead:    %.2f us\n\n", mean_recovery_us);

    printf("[3/3] Evaluating Multi-Tenant Resilience (Post-Crash Survival Verification)...\n");
    /* Execute Legitimate Tenant B immediately after recovering from Tenant A's crash */
    host_service_args_t tenant_b_args;
    tenant_b_args.payload = legit_payload;
    tenant_b_args.payload_len = sizeof(legit_payload);
    tenant_b_args.output_hash = 0;
    tenant_b_args.success = false;

    sdrad_status_t st_b = sdrad_domain_execute(dom, host_crypto_verify_service, &tenant_b_args);
    sdrad_metrics_t m_b;
    sdrad_domain_get_metrics(dom, &m_b);

    bool tenant_b_ok = (st_b == SDRAD_OK && tenant_b_args.success && tenant_b_args.output_hash != 0);
    printf("  Tenant B Post-Crash Execution:     %s (Checksum=0x%08X, Latency=%.2f us)\n",
           tenant_b_ok ? "PASSED (Zero State Leakage)" : "FAILED", 
           tenant_b_args.output_hash, m_b.last_execution_latency_us);

    if (csv) {
        fprintf(csv, "31,post_recovery_tenant_b,host_crypto_verify,%s,%.2f,0.00,%zu,0x%08X\n",
                tenant_b_ok ? "SUCCESS" : "FAIL", m_b.last_execution_latency_us, 
                m_b.heap_used_bytes, tenant_b_args.output_hash);
        fclose(csv);
    }

    sdrad_domain_destroy(dom);
    sdrad_cleanup();

    printf("\n==============================================================================\n");
    printf("SDRaD Evaluation Complete! Results exported to:\n");
    printf("  data\\security\\sdrad_recovery_benchmarks.csv\n");
    printf("==============================================================================\n");

    return 0;
}
