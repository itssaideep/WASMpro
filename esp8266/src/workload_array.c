/*
 * ============================================================
 * SecureWasm Edge — Workload 3: Array Processing
 * ============================================================
 * Permutation and in-place sorting of a 64-element array.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_array(uint32_t iterations) {
    uint16_t arr[64];
    for (uint32_t i = 0; i < 64; ++i) {
        arr[i] = (uint16_t)((i * 109U + 89U) & 0xFFFFU);
    }

    uint32_t chk = 0;

    for (uint32_t it = 0; it < iterations; ++it) {
        /* Permute array */
        for (uint32_t i = 0; i < 64; ++i) {
            arr[i] = (uint16_t)((arr[i] + it * 7U) & 0xFFFFU);
        }

        /* Bubble sort (worst-case array sorting) */
        for (uint32_t i = 0; i < 64; ++i) {
            for (uint32_t j = 0; j < 63U - i; ++j) {
                if (arr[j] > arr[j + 1]) {
                    uint16_t tmp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = tmp;
                }
            }
        }

        chk = (chk * 31U + (uint32_t)arr[0] + (uint32_t)arr[63]) & 0xFFFFFFFFU;
    }

    return chk;
}
