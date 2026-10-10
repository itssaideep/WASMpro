/*
 * ============================================================
 * SecureWasm Edge — Workload 2: Checksum / Hashing
 * ============================================================
 * IEEE 802.3 CRC32 bitwise polynomial computation over
 * a deterministic 256-byte buffer.
 * ============================================================
 */

#include "workloads.h"

uint32_t run_workload_checksum(uint32_t iterations) {
    uint8_t buf[256];
    for (uint32_t i = 0; i < 256; ++i) {
        buf[i] = (uint8_t)((i * 37U + 101U) & 0xFFU);
    }

    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t it = 0; it < iterations; ++it) {
        for (uint32_t i = 0; i < 256; ++i) {
            crc ^= (uint32_t)buf[i];
            for (uint32_t b = 0; b < 8; ++b) {
                if (crc & 1U) {
                    crc = (crc >> 1) ^ 0xEDB88320U;
                } else {
                    crc >>= 1;
                }
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}
