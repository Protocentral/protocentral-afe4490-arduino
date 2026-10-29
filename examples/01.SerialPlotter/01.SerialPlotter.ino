// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 01.SerialPlotter
//
// Streams the IR and red PPG to the Arduino Serial Plotter (Tools > Serial Plotter,
// 115200 baud). The AFE runs at 500 Hz; every 5 samples are averaged, so the plotter
// gets 100 points per second. Put a finger in the probe to see the pulse.
//
// Wiring: see the README (Hardware Setup section). DRDY on D2 gives one read per sample.
// For research and education only - not a medical device.

#include <SPI.h>
#include "protocentral_afe44xx.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_CS = 13, PIN_DRDY = 26, PIN_PWDN = 27, PIN_RESET = 14;
#else
const int PIN_CS = 7, PIN_DRDY = 2, PIN_PWDN = 4, PIN_RESET = 5;  // AFE4490 Shield / Breakout on an Uno
#endif

const uint8_t AVERAGE = 5;  // 500 Hz -> 100 Hz

Protocentral_AFE44xx afe(PIN_CS, PIN_DRDY, PIN_PWDN, PIN_RESET);

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!afe.begin()) {
        Serial.print(F("AFE4490 not found, error "));
        Serial.println((int)afe.lastError());
        while (1) delay(1000);
    }
    afe.enableDataReadyInterrupt();  // without an interrupt pin the driver reads on a timer
}

// The PPG is a small ripple on a large DC level; subtracting a slow running
// average keeps both traces centred in the plotter
int32_t irSum = 0, redSum = 0, irBaseline = 0, redBaseline = 0;
uint8_t count = 0;

void loop() {
    AFE44xx_Sample s;
    if (!afe.available() || !afe.read(s)) return;

    irSum += s.ir();
    redSum += s.red();
    if (++count < AVERAGE) return;

    int32_t ir  = irSum / AVERAGE;
    int32_t red = redSum / AVERAGE;
    irSum = redSum = 0;
    count = 0;

    if (irBaseline == 0) {
        irBaseline  = ir;
        redBaseline = red;
    }
    irBaseline += (ir - irBaseline) / 64;
    redBaseline += (red - redBaseline) / 64;

    // Inverted so each heartbeat points up
    Serial.print(F("ir:"));
    Serial.print(irBaseline - ir);
    Serial.print(F(" red:"));
    Serial.println(redBaseline - red);
}
