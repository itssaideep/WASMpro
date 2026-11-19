/*
 * ============================================================================
 * SecureWasm Edge — Secure Domain Rewind and Discard (SDRaD) Implementation
 * ============================================================================
 */

#include "sdrad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <time.h>
#endif

#define SDRAD_CANARY 0xC0FFEE42DEADBEEFULL

struct sdrad_domain {
    char name[64];
    uint8_t* heap_memory;
    size_t heap_capacity;
    size_t heap_used;
    uint64_t guard_canary_before;
    uint64_t guard_canary_after;

    jmp_buf anchor;
    volatile bool is_active;
    volatile bool fault_flag;
    uint32_t last_fault_code;

    sdrad_metrics_t metrics;
};

static __thread sdrad_domain_t* g_current_domain = NULL;
static void* g_vectored_handler_handle = NULL;

static double get_time_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return ((double)t.QuadPart * 1000000.0) / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
#endif
}

#ifdef _WIN32
static LONG WINAPI sdrad_vectored_exception_filter(PEXCEPTION_POINTERS ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;

    /* Intercept memory access violations, illegal instructions, divide by zero */
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        code == EXCEPTION_STACK_OVERFLOW) {

        if (g_current_domain != NULL && g_current_domain->is_active) {
            sdrad_domain_t* dom = g_current_domain;
            dom->fault_flag = true;
            dom->last_fault_code = (uint32_t)code;
            dom->metrics.faults_intercepted++;

            /* Perform immediate Discard & Rewind */
            longjmp(dom->anchor, 1);
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

sdrad_status_t sdrad_init(void) {
#ifdef _WIN32
    if (!g_vectored_handler_handle) {
        g_vectored_handler_handle = AddVectoredExceptionHandler(1, sdrad_vectored_exception_filter);
        if (!g_vectored_handler_handle) {
            return SDRAD_ERR_INIT;
        }
    }
#endif
    return SDRAD_OK;
}

void sdrad_cleanup(void) {
#ifdef _WIN32
    if (g_vectored_handler_handle) {
        RemoveVectoredExceptionHandler(g_vectored_handler_handle);
        g_vectored_handler_handle = NULL;
    }
#endif
}

sdrad_domain_t* sdrad_domain_create(const char* name, size_t heap_capacity_bytes) {
    if (heap_capacity_bytes == 0) heap_capacity_bytes = 65536; // Default 64 KB

    sdrad_domain_t* dom = (sdrad_domain_t*)calloc(1, sizeof(sdrad_domain_t));
    if (!dom) return NULL;

    strncpy(dom->name, name ? name : "unnamed_domain", sizeof(dom->name) - 1);
    dom->heap_capacity = heap_capacity_bytes;
    dom->heap_used = 0;
    dom->guard_canary_before = SDRAD_CANARY;
    dom->guard_canary_after = SDRAD_CANARY;

    dom->heap_memory = (uint8_t*)calloc(1, dom->heap_capacity);
    if (!dom->heap_memory) {
        free(dom);
        return NULL;
    }

    dom->metrics.heap_capacity_bytes = dom->heap_capacity;
    return dom;
}

void sdrad_domain_destroy(sdrad_domain_t* dom) {
    if (!dom) return;
    if (dom->heap_memory) free(dom->heap_memory);
    free(dom);
}

void* sdrad_domain_alloc(sdrad_domain_t* dom, size_t size) {
    if (!dom || !dom->heap_memory) return NULL;
    /* 8-byte alignment */
    size_t aligned_size = (size + 7) & ~((size_t)7);
    if (dom->heap_used + aligned_size > dom->heap_capacity) {
        return NULL; // Domain out-of-memory
    }
    void* ptr = dom->heap_memory + dom->heap_used;
    dom->heap_used += aligned_size;
    dom->metrics.heap_used_bytes = dom->heap_used;
    return ptr;
}

/* Internal discard implementation: wipe domain state */
static void sdrad_discard_internal(sdrad_domain_t* dom) {
    if (!dom) return;
    if (dom->heap_memory && dom->heap_used > 0) {
        memset(dom->heap_memory, 0, dom->heap_used);
    }
    dom->heap_used = 0;
    dom->metrics.heap_used_bytes = 0;
    dom->metrics.discards_performed++;
}

sdrad_status_t sdrad_domain_execute(sdrad_domain_t* dom, sdrad_target_fn fn, void* user_data) {
    if (!dom || !fn) return SDRAD_ERR_INIT;
    if (dom->is_active) return SDRAD_ERR_ACTIVE_DOMAIN;

    double t_start = get_time_us();

    dom->fault_flag = false;
    dom->is_active = true;
    g_current_domain = dom;

    /* Checkpoint: Anchor Point */
    if (setjmp(dom->anchor) == 0) {
        /* Forward execution into isolated domain */
        fn(user_data);

        double t_end = get_time_us();
        dom->is_active = false;
        g_current_domain = NULL;
        dom->metrics.total_executions++;
        dom->metrics.last_execution_latency_us = t_end - t_start;
        return SDRAD_OK;
    } else {
        /* Rewound here via fault handler */
        double t_recovery_start = get_time_us();

        /* Discard: clean corrupted domain memory */
        sdrad_discard_internal(dom);

        double t_recovery_end = get_time_us();
        dom->is_active = false;
        g_current_domain = NULL;
        dom->metrics.total_executions++;
        dom->metrics.last_recovery_latency_us = t_recovery_end - t_recovery_start;
        dom->metrics.last_execution_latency_us = t_recovery_end - t_start;

        return SDRAD_FAULT_INTERCEPTED;
    }
}

void sdrad_domain_get_metrics(sdrad_domain_t* dom, sdrad_metrics_t* out_metrics) {
    if (dom && out_metrics) {
        *out_metrics = dom->metrics;
    }
}
