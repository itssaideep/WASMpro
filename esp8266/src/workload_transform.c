/*
 * ============================================================
 * SecureWasm Edge — Workload 4: Data Transformation
 * ============================================================
 * S-box non-linear substitution and circular bitwise rotation
 * over a 128-byte buffer.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_transform(uint32_t iterations) {
    uint8_t buf[128];
    for (uint32_t i = 0; i < 128; ++i) {
        buf[i] = (uint8_t)((i * 17U + 23U) & 0xFFU);
    }

    uint32_t chk = 0;

    for (uint32_t it = 0; it < iterations; ++it) {
        for (uint32_t i = 0; i < 128; ++i) {
            uint8_t v = buf[i];
            /* 8-bit circular left rotate by 3 */
            v = (uint8_t)(((v << 3) | (v >> 5)) & 0xFFU);
            /* Non-linear XOR perturbation */
            v = (uint8_t)((v ^ 0x5AU ^ (it & 0xFFU)) & 0xFFU);
            buf[i] = v;
        }
        chk = (chk * 33U + (uint32_t)buf[0] + (uint32_t)buf[127]) & 0xFFFFFFFFU;
    }

    return chk;
}
