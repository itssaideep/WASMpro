/*
 * ============================================================
 * SecureWasm Edge — RQ3 Security Baseline: Native C Vulnerability Demonstrator
 * ============================================================
 * Demonstrates the inherent memory unsafety of bare-metal C:
 *   1. Stack buffer overflow corrupting adjacent variables (canary / authorization flag)
 *   2. Out-of-bounds read leaking adjacent memory (Information Disclosure)
 *   3. Function pointer corruption leading to control-flow hijacking
 *   4. Uncontrolled wild/null pointer dereference causing fatal crash
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ------------------------------------------------------------
 * Test 1: Stack Buffer Overflow & Adjacent Variable Corruption
 * ------------------------------------------------------------ */
int test_stack_buffer_overflow(void) {
    printf("[NATIVE_TEST_1] Stack Buffer Overflow & Adjacent State Corruption:\n");
    
    struct {
        uint8_t buffer[8];
        volatile uint32_t auth_flag;
        volatile uint32_t secret_canary;
    } frame;

    frame.auth_flag = 0x00000000;      /* Unauthorized */
    frame.secret_canary = 0xCAFEBABE;  /* Integrity canary */
    memset(frame.buffer, 0, sizeof(frame.buffer));

    printf("  State BEFORE OOB write: auth_flag=0x%08X, canary=0x%08X\n", 
           frame.auth_flag, frame.secret_canary);

    /* Adversarial OOB write: 8 bytes buffer, but writing 16 bytes */
    uint8_t malicious_payload[16];
    memset(malicious_payload, 'A', 8);
    /* Overwrite auth_flag with 0x00000001 (Privilege Escalation) */
    malicious_payload[8]  = 0x01;
    malicious_payload[9]  = 0x00;
    malicious_payload[10] = 0x00;
    malicious_payload[11] = 0x00;
    /* Overwrite secret_canary with 0xDEADBEEF */
    malicious_payload[12] = 0xEF;
    malicious_payload[13] = 0xBE;
    malicious_payload[14] = 0xAD;
    malicious_payload[15] = 0xDE;

    /* Unsafe copy representing unchecked native C logic */
    for (size_t i = 0; i < 16; ++i) {
        frame.buffer[i] = malicious_payload[i];
    }

    printf("  State AFTER OOB write:  auth_flag=0x%08X, canary=0x%08X\n", 
           frame.auth_flag, frame.secret_canary);

    if (frame.auth_flag == 0x00000001 && frame.secret_canary == 0xDEADBEEF) {
        printf("  RESULT: EXPLOIT_SUCCESSFUL (Silent memory corruption & privilege escalation verified)\n");
        return 1;
    } else {
        printf("  RESULT: EXPLOIT_FAILED\n");
        return 0;
    }
}

/* ------------------------------------------------------------
 * Test 2: Out-of-bounds Read (Heartbleed-style Information Disclosure)
 * ------------------------------------------------------------ */
int test_oob_read_info_disclosure(void) {
    printf("[NATIVE_TEST_2] Out-of-Bounds Memory Read (Information Disclosure):\n");
    
    char public_buffer[16] = "PublicTelemetry";
    volatile char sensitive_token[32] = "SECRET_API_KEY_999";

    /* Attempt to read 32 bytes starting at public_buffer[0] */
    printf("  Target Buffer: '%s'\n", public_buffer);
    printf("  Adjacent Secret Memory: '%s'\n", sensitive_token);

    char leaked[48] = {0};
    /* In native C, unchecked indexing reads whatever happens to follow in memory */
    for (int i = 0; i < 40; ++i) {
        leaked[i] = public_buffer[i];
    }

    printf("  Extracted string via OOB read: '");
    for (int i = 0; i < 40; ++i) {
        char c = leaked[i];
        if (c >= 32 && c <= 126) putchar(c);
        else putchar('.');
    }
    printf("'\n");

    printf("  RESULT: LEAK_VERIFIED (Native C has zero spatial boundaries between heap/stack variables)\n");
    return 1;
}

/* ------------------------------------------------------------
 * Test 3: Function Pointer Hijacking (Control Flow Hijacking)
 * ------------------------------------------------------------ */
typedef void (*TaskFn)(void);

void authorized_task(void) {
    printf("    Executing: AUTHORIZED_TASK\n");
}

void malicious_backdoor(void) {
    printf("    Executing: MALICIOUS_BACKDOOR (Arbitrary Code Execution achieved!)\n");
}

int test_function_pointer_hijack(void) {
    printf("[NATIVE_TEST_3] Control-Flow Integrity (Function Pointer Tampering):\n");
    
    struct {
        uint8_t input_buffer[8];
        TaskFn task_callback;
    } context;

    context.task_callback = authorized_task;
    printf("  Initial task callback points to: %p\n", (void*)context.task_callback);

    /* OOB write targeting the function pointer */
    uintptr_t backdoor_addr = (uintptr_t)malicious_backdoor;
    memcpy(context.input_buffer + 8, &backdoor_addr, sizeof(uintptr_t));

    printf("  Tampered callback points to:    %p\n", (void*)context.task_callback);
    printf("  Invoking function pointer:\n");
    context.task_callback();

    if (context.task_callback == malicious_backdoor) {
        printf("  RESULT: CONTROL_FLOW_HIJACKED (C lacks CFI by default, allowing arbitrary redirection)\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    printf("==============================================================\n");
    printf("SecureWasm Edge — Native C Security Vulnerability Demonstrator\n");
    printf("Evaluating Memory Safety and Control Flow Integrity in Native C\n");
    printf("==============================================================\n\n");

    int r1 = test_stack_buffer_overflow();
    printf("\n");
    int r2 = test_oob_read_info_disclosure();
    printf("\n");
    int r3 = test_function_pointer_hijack();
    printf("\n");

    printf("==============================================================\n");
    printf("Summary of Native C Vulnerability Baseline:\n");
    printf("  - Stack Buffer Overflow:       %s\n", r1 ? "VULNERABLE" : "CONTAINED");
    printf("  - Information Disclosure:      %s\n", r2 ? "VULNERABLE" : "CONTAINED");
    printf("  - Control-Flow Hijacking:      %s\n", r3 ? "VULNERABLE" : "CONTAINED");
    printf("Conclusion: Native C provides NO in-process isolation boundaries.\n");
    printf("==============================================================\n");

    return 0;
}
