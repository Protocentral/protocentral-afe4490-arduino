// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 03.SpO2HeartRate
//
// Prints SpO2, heart rate and the signal quality once per second (115200 baud).
// Keep the finger still: the first values appear after about 5 seconds.
//
// The algorithms resample every input to 50 Hz, so the results do not depend on the
// PRF. They report invalid (0) until they have 4 regular beats of good signal.
// The SpO2 curve must be calibrated against a reference oximeter for your probe.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "protocentral_afe44xx.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_CS = 13, PIN_DRDY = 26, PIN_PWDN = 27, PIN_RESET = 14;
#else
const int PIN_CS = 7, PIN_DRDY = 2, PIN_PWDN = 4, PIN_RESET = 5;  // AFE4490 Shield / Breakout on an Uno
#endif

Protocentral_AFE44xx afe(PIN_CS, PIN_DRDY, PIN_PWDN, PIN_RESET);
AFE44xx_Algorithms algo;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!afe.begin()) {
        Serial.print(F("AFE4490 not found, error "));
        Serial.println((int)afe.lastError());
        while (1) delay(1000);
    }
    afe.enableDataReadyInterrupt();
    algo.begin(afe.getSampleRate());
    Serial.println(F("Place your finger in the probe"));
}

void printQuality(uint8_t q) {
    if (q & AFE44XX_QUALITY_NO_SIGNAL)     Serial.print(F(" no finger"));
    if (q & AFE44XX_QUALITY_SATURATED)     Serial.print(F(" saturated"));
    if (q & AFE44XX_QUALITY_LOW_PERFUSION) Serial.print(F(" weak pulse"));
    if (q & AFE44XX_QUALITY_NO_PULSE)      Serial.print(F(" searching"));
    if (q & AFE44XX_QUALITY_RATE_TOO_LOW)  Serial.print(F(" sample rate too low"));
}

unsigned long lastPrint = 0;

void loop() {
    AFE44xx_Sample s;
    if (afe.available() && afe.read(s)) {
        algo.processSample(s.ir(), s.red());
    }

    if (millis() - lastPrint < 1000) return;
    lastPrint = millis();

    Serial.print(F("SpO2: "));
    if (algo.spo2Valid()) Serial.print(algo.spo2()); else Serial.print(F("--"));
    Serial.print(F(" %  HR: "));
    if (algo.heartRateValid()) Serial.print(algo.heartRate()); else Serial.print(F("--"));
    Serial.print(F(" bpm  PI: "));
    Serial.print(algo.perfusionIndex() / 100.0, 2);
    Serial.print(F(" %"));
    printQuality(algo.signalQuality());
    Serial.println();
}
