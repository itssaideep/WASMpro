/*
 * ============================================================
 * SecureWasm Edge — Workload 1: Integer Arithmetic
 * ============================================================
 * Architecture-neutral C implementation.
 *
 * Algorithm:
 *   Triple Linear Congruential Generator (LCG) coupled with
 *   mixed bitwise non-linear mixing and accumulator perturbation.
 *
 * Determinism:
 *   Uses strictly uint32_t unsigned wrap-around math. No signed
 *   overflow (UB), no floating point rounding discrepancies.
 *
 * Input:
 *   iterations: count of outer compute cycles.
 * Output:
 *   uint32_t verification checksum.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_integer(uint32_t iterations) {
    uint32_t a = 123456789U;
    uint32_t b = 987654321U;
    uint32_t c = 555555555U;

    for (uint32_t i = 0; i < iterations; ++i) {
        /* Pseudo-random LCG mixing step */
        a = (a * 1664525U + 1013904223U) ^ (b >> 3);
        b = (b * 1103515245U + 12345U) ^ (c << 5);
        c = (c * 214013U + 2531011U) ^ (a >> 7);

        /* Non-linear arithmetic perturbation */
        uint32_t temp = (a ^ b) + c;
        temp = (temp * 31U) ^ (temp >> 13);
        a ^= (temp + (i * 17U));
    }

    return a ^ b ^ c;
}
