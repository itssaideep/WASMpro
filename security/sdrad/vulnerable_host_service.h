/*
 * ============================================================================
 * SecureWasm Edge — Vulnerable Host Service Mock
 * ============================================================================
 * Represents third-party C libraries, WASI host functions, or native drivers
 * called by WebAssembly modules across the FFI boundary.
 * ============================================================================
 */

#ifndef VULNERABLE_HOST_SERVICE_H
#define VULNERABLE_HOST_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t* payload;
    size_t payload_len;
    uint32_t output_hash;
    bool success;
} host_service_args_t;

/* Simulates a native cryptographic token parser with a memory corruption flaw */
void host_crypto_verify_service(void* args);

/* Simulates a sensor decompression routine with a wild/null dereference flaw */
void host_decompression_service(void* args);

/* Direct SDRaD-protected entry points */
int sdrad_invoke_crypto_protected(void* dom, host_service_args_t* args);
int sdrad_invoke_decompression_protected(void* dom, host_service_args_t* args);

#ifdef __cplusplus
}
#endif

#endif /* VULNERABLE_HOST_SERVICE_H */
