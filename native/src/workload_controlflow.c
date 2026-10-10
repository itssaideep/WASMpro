/*
 * ============================================================
 * SecureWasm Edge — Workload 6: Control Flow & Dispatch
 * ============================================================
 * Multi-state finite state machine with branching, bitwise
 * accumulation, and cyclic state transitions.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_controlflow(uint32_t iterations) {
    uint32_t state = 0;
    uint32_t acc = 12345U;

    for (uint32_t it = 0; it < iterations; ++it) {
        switch (state) {
            case 0:
                acc = (acc * 3U + 1U) & 0xFFFFU;
                state = (acc & 1U) ? 1U : 2U;
                break;
            case 1:
                acc = (acc ^ 0xACE1U) & 0xFFFFU;
                state = 3U;
                break;
            case 2:
                acc = (acc + (it & 0xFFU)) & 0xFFFFU;
                state = 4U;
                break;
            case 3:
                acc = ((acc >> 1) | (acc << 15)) & 0xFFFFU;
                state = 0U;
                break;
            case 4:
                acc = (acc * 5U + 7U) & 0xFFFFU;
                state = 0U;
                break;
            default:
                state = 0U;
                break;
        }
    }

    return acc & 0xFFFFFFFFU;
}
