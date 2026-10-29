// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 04.HighRateInterrupt
//
// Runs the AFE at 1 kHz with ADC_RDY on an interrupt and prints, once per second,
// how many samples arrived and how many were missed (115200 baud). Use it to find
// the highest PRF your board sustains: raise PRF until "missed" stops being 0.
//
// DRDY must be on an interrupt-capable pin (D2 or D3 on an Uno). If the interrupt
// stops firing, the driver reads on a timer instead and reports a DRDY fault.
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

const uint16_t PRF = 1000;  // Hz, 63-5000

Protocentral_AFE44xx afe(PIN_CS, PIN_DRDY, PIN_PWDN, PIN_RESET);

uint32_t received = 0;
uint32_t lastMissed = 0;
unsigned long lastReport = 0;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    AFE44xx_Config cfg;
    cfg.prf = PRF;  // averages are reduced to what fits at this PRF

    if (!afe.begin(cfg)) {
        Serial.print(F("AFE4490 not found, error "));
        Serial.println((int)afe.lastError());
        while (1) delay(1000);
    }
    if (!afe.enableDataReadyInterrupt()) {
        Serial.println(F("DRDY pin is not interrupt-capable on this board"));
        while (1) delay(1000);
    }
    Serial.print(F("PRF "));
    Serial.print(afe.getSampleRate());
    Serial.print(F(" Hz, "));
    Serial.print(afe.getAverages());
    Serial.println(F(" averages"));
}

void loop() {
    AFE44xx_Sample s;
    if (afe.available() && afe.read(s)) {
        received++;
    }

    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        uint32_t missed = afe.missedSamples();
        Serial.print(F("received:"));
        Serial.print(received);
        Serial.print(F(" missed:"));
        Serial.print(missed - lastMissed);
        if (afe.dataReadyPinFault()) Serial.print(F(" (DRDY fault: reading on a timer)"));
        Serial.println();
        received   = 0;
        lastMissed = missed;
    }
}
