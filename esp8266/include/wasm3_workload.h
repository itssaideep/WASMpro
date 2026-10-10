/*
 * ============================================================
 * SecureWasm Edge — ESP8266 wasm3 Workload Runner Interface
 * ============================================================
 */

#ifndef WASM3_WORKLOAD_H
#define WASM3_WORKLOAD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long init_duration_us;
    unsigned long exec_duration_us;
    uint32_t checksum;
    uint32_t heap_before;
    uint32_t heap_after;
    bool success;
    const char* error_msg;
} Wasm3Result;

Wasm3Result run_wasm3_integer(uint32_t iterations);

#ifdef __cplusplus
}
#endif

#endif /* WASM3_WORKLOAD_H */
