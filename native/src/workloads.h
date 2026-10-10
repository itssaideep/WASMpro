/*
 * ============================================================
 * SecureWasm Edge — Complete Deterministic Workload Interface
 * ============================================================
 * Component A & B: 6 Deterministic C/C++ Workloads
 *
 * Designed to execute identically across:
 *   1. ESP8266 Native (Xtensa LX106 @ 80 MHz)
 *   2. Host Native (x86_64 GCC -O0 and -O2)
 *   3. WebAssembly (wasm32-wasi via Clang -O0 and -O2)
 *
 * Determinism guarantees:
 *   - Strictly unsigned integer wrapping math (no undefined behavior)
 *   - Fixed iteration counts and input datasets
 *   - Output verification checksums that match 100% across targets
 * ============================================================
 */

#ifndef WORKLOADS_H
#define WORKLOADS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------
 * Workload Iteration Counts
 * ------------------------------------------------------------ */
#define WORKLOAD_INTEGER_ITERS     10000U
#define WORKLOAD_CHECKSUM_ITERS      100U
#define WORKLOAD_ARRAY_ITERS         100U
#define WORKLOAD_TRANSFORM_ITERS    1000U
#define WORKLOAD_SENSOR_ITERS      10000U
#define WORKLOAD_CONTROLFLOW_ITERS 10000U

/* ------------------------------------------------------------
 * Known-Good Verification Checksums (Strictly Derived)
 * ------------------------------------------------------------ */
#define EXPECTED_CHECKSUM_INTEGER     0x0F6B7313U
#define EXPECTED_CHECKSUM_CHECKSUM    0x067088A3U
#define EXPECTED_CHECKSUM_ARRAY       0x36ABF7B8U
#define EXPECTED_CHECKSUM_TRANSFORM   0xF170C069U
#define EXPECTED_CHECKSUM_SENSOR      0x0249054DU
#define EXPECTED_CHECKSUM_CONTROLFLOW 0x0000222FU

/* ------------------------------------------------------------
 * Workload Function Declarations
 * ------------------------------------------------------------ */

/* 1. Integer arithmetic (LCG mixing + non-linear arithmetic) */
uint32_t run_workload_integer(uint32_t iterations);

/* 2. Checksum / Hashing (IEEE 802.3 CRC32 over 256-byte block) */
uint32_t run_workload_checksum(uint32_t iterations);

/* 3. Array processing (Permutation + in-place bubble sort of 64 elements) */
uint32_t run_workload_array(uint32_t iterations);

/* 4. Data transformation (Bitwise byte rotation + S-box substitution) */
uint32_t run_workload_transform(uint32_t iterations);

/* 5. Simulated sensor processing (Exponential moving average + peak detection) */
uint32_t run_workload_sensor(uint32_t iterations);

/* 6. Control flow & dispatch (6-state machine transition graph) */
uint32_t run_workload_controlflow(uint32_t iterations);

#ifdef __cplusplus
}
#endif

#endif /* WORKLOADS_H */
