/*
 * ============================================================
 * SecureWasm Edge — ESP8266 Environment Validation (Phase 1)
 * ============================================================
 *
 * Purpose:
 *   Minimal firmware to verify that the ESP8266 toolchain works:
 *   - Serial output at 115200 baud
 *   - LED blink on the built-in LED (GPIO2)
 *   - Reports chip info and free heap
 *   - Confirms the board is ready for benchmark workloads
 *
 * Expected Serial output:
 *   ========================================
 *   SecureWasm Edge — ESP8266 Phase 1
 *   Environment Validation
 *   ========================================
 *   Chip ID:       <hex>
 *   CPU Freq:      80 MHz
 *   Flash Size:    4194304 bytes
 *   Free Heap:     <bytes>
 *   SDK Version:   <string>
 *   ========================================
 *   STATUS: ENVIRONMENT_OK
 *   ========================================
 *   [heartbeat] uptime=<ms> heap=<bytes>
 *   ...
 *
 * Board:  NodeMCU v2 (ESP-12E)
 * Framework: Arduino
 * ============================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

/* Built-in LED on NodeMCU v2 is typically GPIO2 (active LOW) */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

/* Heartbeat interval in milliseconds */
static const unsigned long HEARTBEAT_INTERVAL_MS = 2000;

/* Track last heartbeat time */
static unsigned long lastHeartbeatMs = 0;
static unsigned long heartbeatCount = 0;

void setup() {
    /* ---- Serial ---- */
    Serial.begin(115200);
    delay(500);  /* Allow Serial to stabilize after reset */

    /* ---- Disable WiFi to reduce background noise ---- */
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(10);

    /* ---- LED ---- */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  /* OFF (active LOW) */

    /* ---- Print environment info ---- */
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("SecureWasm Edge - ESP8266 Phase 1"));
    Serial.println(F("Environment Validation"));
    Serial.println(F("========================================"));

    Serial.print(F("Chip ID:       0x"));
    Serial.println(ESP.getChipId(), HEX);

    Serial.print(F("CPU Freq:      "));
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(F(" MHz"));

    Serial.print(F("Flash Size:    "));
    Serial.print(ESP.getFlashChipRealSize());
    Serial.println(F(" bytes"));

    Serial.print(F("Free Heap:     "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));

    Serial.print(F("SDK Version:   "));
    Serial.println(ESP.getSdkVersion());

    Serial.print(F("Reset Reason:  "));
    Serial.println(ESP.getResetReason());

    Serial.println(F("========================================"));
    Serial.println(F("STATUS: ENVIRONMENT_OK"));
    Serial.println(F("========================================"));
    Serial.println();

    lastHeartbeatMs = millis();
}

void loop() {
    unsigned long now = millis();

    /* ---- Heartbeat: blink LED + print status ---- */
    if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatMs = now;
        heartbeatCount++;

        /* Toggle LED */
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

        /* Print heartbeat line */
        Serial.print(F("[heartbeat] count="));
        Serial.print(heartbeatCount);
        Serial.print(F(" uptime_ms="));
        Serial.print(now);
        Serial.print(F(" heap="));
        Serial.println(ESP.getFreeHeap());
    }

    /* Yield to the ESP8266 background tasks */
    yield();
}
