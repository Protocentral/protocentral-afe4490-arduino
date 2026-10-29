// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 06.CustomConfig
//
// Starts from a custom AFE44xx_Config, then adjusts the LED currents at run time so
// the IR and red levels sit near the middle of the ADC range. Prints the levels and
// LED codes once per second (115200 baud).
//
// Every setter works while the AFE is running. Values are validated: an out-of-range
// setting returns false and lastError() says why.
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

const int32_t TARGET    = AFE44XX_ADC_FULL_SCALE / 2;  // aim for half scale
const int32_t TOLERANCE = AFE44XX_ADC_FULL_SCALE / 8;

Protocentral_AFE44xx afe(PIN_CS, PIN_DRDY, PIN_PWDN, PIN_RESET);

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    AFE44xx_Config cfg;
    // cfg.chip = AFE44xx_Chip::AFE4400;          // Sensything OX
    cfg.prf         = 250;                          // Hz
    cfg.dutyCycle   = 20;                           // % of each period the LED is on
    cfg.averages    = 4;
    cfg.led1Current = 30;                           // DAC code: 30 / 256 x full scale
    cfg.led2Current = 30;
    cfg.tiaRf       = AFE44xx_TiaRf::RF_500K;
    cfg.tiaCf       = AFE44xx_TiaCf::pF_5;
    cfg.stage2Gain  = AFE44xx_Stage2Gain::Off;

    if (!afe.begin(cfg)) {
        Serial.print(F("AFE4490 not found, error "));
        Serial.println((int)afe.lastError());
        while (1) delay(1000);
    }
    afe.enableDataReadyInterrupt();

    Serial.print(F("LED full scale: "));
    Serial.print(afe.ledFullScaleMilliamps());
    Serial.println(F(" mA"));
}

// One step of LED-current adjustment for one LED
void adjust(AFE44xx_Led led, int32_t level) {
    uint8_t code = afe.getLedCurrent(led);
    if (level < TARGET - TOLERANCE && code < 255) {
        afe.setLedCurrent(led, code + 1);
    } else if (level > TARGET + TOLERANCE && code > 1) {
        afe.setLedCurrent(led, code - 1);
    }
}

unsigned long lastStep = 0, lastPrint = 0;
AFE44xx_Sample s;

void loop() {
    if (afe.available()) afe.read(s);

    // Small steps, slower than the signal chain settles
    if (millis() - lastStep >= 100) {
        lastStep = millis();
        adjust(AFE44xx_Led::IR, s.ir());
        adjust(AFE44xx_Led::Red, s.red());
    }

    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        Serial.print(F("IR "));
        Serial.print(s.ir());
        Serial.print(F(" (code "));
        Serial.print(afe.getLedCurrent(AFE44xx_Led::IR));
        Serial.print(F(")  red "));
        Serial.print(s.red());
        Serial.print(F(" (code "));
        Serial.print(afe.getLedCurrent(AFE44xx_Led::Red));
        Serial.println(F(")"));
    }
}
