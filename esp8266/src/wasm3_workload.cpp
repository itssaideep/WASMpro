/*
 * ============================================================
 * SecureWasm Edge — On-Device wasm3 Runner (ESP8266)
 * ============================================================
 * Executes Workload 1 (Integer Arithmetic) directly inside
 * the wasm3 micro-interpreter on the ESP8266 microcontroller.
 * ============================================================
 */

#include <Arduino.h>
#include <wasm3.h>
#include <m3_env.h>
#include "wasm3_workload.h"
#include "workload_integer_wasm.h"

Wasm3Result run_wasm3_integer(uint32_t iterations) {
    Wasm3Result res;
    res.init_duration_us = 0;
    res.exec_duration_us = 0;
    res.checksum = 0;
    res.heap_before = ESP.getFreeHeap();
    res.heap_after = res.heap_before;
    res.success = false;
    res.error_msg = NULL;

    unsigned long t_init0 = micros();

    IM3Environment env = m3_NewEnvironment();
    if (!env) {
        res.error_msg = "m3_NewEnvironment failed";
        return res;
    }

    /* 2048 bytes execution stack allocated inside heap */
    IM3Runtime runtime = m3_NewRuntime(env, 2048, NULL);
    if (!runtime) {
        m3_FreeEnvironment(env);
        res.error_msg = "m3_NewRuntime failed (OOM)";
        return res;
    }
    runtime->memoryLimit = 2048;

    IM3Module module = NULL;
    M3Result err = m3_ParseModule(env, &module, workload_integer_wasm, workload_integer_wasm_len);
    if (err) {
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        res.error_msg = err;
        return res;
    }

    err = m3_LoadModule(runtime, module);
    if (err) {
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        res.error_msg = err;
        return res;
    }

    IM3Function func = NULL;
    err = m3_FindFunction(&func, runtime, "run_workload_integer");
    if (err) {
        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        res.error_msg = err;
        return res;
    }

    unsigned long t_init1 = micros();
    res.init_duration_us = (t_init1 >= t_init0) ? (t_init1 - t_init0) : 0;

    /* Execute the interpreted WASM workload */
    unsigned long t_exec0 = micros();
    err = m3_CallV(func, iterations);
    unsigned long t_exec1 = micros();
    res.exec_duration_us = (t_exec1 >= t_exec0) ? (t_exec1 - t_exec0) : 0;

    if (err == m3Err_none) {
        uint32_t ret_val = 0;
        err = m3_GetResultsV(func, &ret_val);
        if (err == m3Err_none) {
            res.checksum = ret_val;
            res.success = true;
        } else {
            res.error_msg = err;
        }
    } else {
        res.error_msg = err;
    }

    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);

    res.heap_after = ESP.getFreeHeap();
    return res;
}
