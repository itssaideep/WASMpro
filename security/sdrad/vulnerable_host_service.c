/*
 * ============================================================================
 * SecureWasm Edge — Vulnerable Host Service Implementation
 * ============================================================================
 */

#include "vulnerable_host_service.h"
#include "sdrad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_crypto_verify_service(void* user_data) {
    host_service_args_t* args = (host_service_args_t*)user_data;
    if (!args) return;

    /* Flaw: If payload begins with 0xFF, simulate an unchecked buffer overflow / invalid memory read */
    if (args->payload_len > 0 && args->payload[0] == 0xFF) {
        /* Deliberate memory violation: reading from an unmapped address */
        volatile uint32_t* unmapped_ptr = (volatile uint32_t*)0x00000000;
        uint32_t crash_read = *unmapped_ptr;
        args->output_hash = crash_read;
        args->success = true;
        return;
    }

    /* Legitimate processing */
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < args->payload_len; ++i) {
        h = (h ^ args->payload[i]) * 0x01000193;
    }
    args->output_hash = h;
    args->success = true;
}

void host_decompression_service(void* user_data) {
    host_service_args_t* args = (host_service_args_t*)user_data;
    if (!args) return;

    /* Flaw: If payload begins with 0xEE, simulate a wild write */
    if (args->payload_len > 0 && args->payload[0] == 0xEE) {
        volatile uint8_t* bad_dest = (volatile uint8_t*)0x00000000;
        *bad_dest = 0xAA; /* Deliberate crash write */
        args->success = true;
        return;
    }

    /* Legitimate decompression */
    uint32_t acc = 0;
    for (size_t i = 0; i < args->payload_len; ++i) {
        acc += (uint32_t)args->payload[i] * 31U;
    }
    args->output_hash = acc;
    args->success = true;
}

int sdrad_invoke_crypto_protected(void* dom, host_service_args_t* args) {
    extern sdrad_status_t sdrad_domain_execute(sdrad_domain_t* dom, void (*fn)(void*), void* user_data);
    return (int)sdrad_domain_execute((sdrad_domain_t*)dom, host_crypto_verify_service, args);
}

int sdrad_invoke_decompression_protected(void* dom, host_service_args_t* args) {
    extern sdrad_status_t sdrad_domain_execute(sdrad_domain_t* dom, void (*fn)(void*), void* user_data);
    return (int)sdrad_domain_execute((sdrad_domain_t*)dom, host_decompression_service, args);
}
