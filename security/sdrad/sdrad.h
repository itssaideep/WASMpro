/*
 * ============================================================================
 * SecureWasm Edge — Secure Domain Rewind and Discard (SDRaD) Runtime Layer
 * ============================================================================
 * Implements in-process domain isolation and rapid microsecond fault recovery
 * for WebAssembly runtimes (Wasmtime / wasm3), inspired by Ericsson Research.
 *
 * Core Capabilities:
 *   1. Isolated Domain Allocation: Dedicated domain scratch heap with guard boundaries.
 *   2. Checkpointing (Anchor): Low-overhead CPU context and stack snapshot.
 *   3. Hardware/OS Fault Trapping: Intercepts memory faults before OS termination.
 *   4. Discard & Rewind: Wipes corrupted domain state and rolls back execution.
 *   5. Seamless Host Resilience: Returns a controlled error trap to Wasm guest.
 * ============================================================================
 */

#ifndef SDRAD_H
#define SDRAD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SDRAD_OK = 0,
    SDRAD_ERR_INIT = -1,
    SDRAD_ERR_OOM = -2,
    SDRAD_ERR_ACTIVE_DOMAIN = -3,
    SDRAD_FAULT_INTERCEPTED = 1
} sdrad_status_t;

typedef struct sdrad_domain sdrad_domain_t;

/* Initialize the SDRaD subsystem and register low-level exception vectors */
sdrad_status_t sdrad_init(void);

/* Clean up SDRaD subsystem */
void sdrad_cleanup(void);

/* Create an isolated memory domain with a specified heap budget */
sdrad_domain_t* sdrad_domain_create(const char* name, size_t heap_capacity_bytes);

/* Destroy an isolated domain and free its memory */
void sdrad_domain_destroy(sdrad_domain_t* dom);

/* Allocate memory inside the isolated domain's scratch heap */
void* sdrad_domain_alloc(sdrad_domain_t* dom, size_t size);

/* 
 * Execute a target function inside an isolated domain with automatic
 * checkpointing, fault interception, discard, and rewind.
 *
 * Parameters:
 *   dom: The target isolated domain.
 *   fn: Function to execute inside the domain.
 *   user_data: Opaque argument passed to fn.
 *
 * Returns:
 *   SDRAD_OK if execution completed normally.
 *   SDRAD_FAULT_INTERCEPTED if a fault occurred and was successfully recovered.
 */
typedef void (*sdrad_target_fn)(void* user_data);
sdrad_status_t sdrad_domain_execute(sdrad_domain_t* dom, sdrad_target_fn fn, void* user_data);

/* Telemetry & Metrics */
typedef struct {
    uint64_t total_executions;
    uint64_t faults_intercepted;
    uint64_t discards_performed;
    double last_execution_latency_us;
    double last_recovery_latency_us;
    size_t heap_used_bytes;
    size_t heap_capacity_bytes;
} sdrad_metrics_t;

void sdrad_domain_get_metrics(sdrad_domain_t* dom, sdrad_metrics_t* out_metrics);

#ifdef __cplusplus
}
#endif

#endif /* SDRAD_H */
