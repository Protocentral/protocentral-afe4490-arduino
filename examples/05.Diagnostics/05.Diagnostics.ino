// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 05.Diagnostics
//
// First thing to run when something does not work. 115200 baud.
// Prints the library version and the begin() result, dumps the registers, runs the
// AFE's LED and photodiode open/short checks, and counts samples and DRDY interrupts.
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

const __FlashStringHelper* statusText(AFE44xx_Status st) {
    switch (st) {
        case AFE44xx_Status::Ok:              return F("OK");
        case AFE44xx_Status::NotInitialized:  return F("not initialised");
        case AFE44xx_Status::SpiError:        return F("no SPI response - check CS/SCK/MOSI/MISO, power and PWDN");
        case AFE44xx_Status::VerifyFailed:    return F("register read-back mismatch - check SPI wiring and signal integrity");
        case AFE44xx_Status::InvalidArgument: return F("invalid argument");
        case AFE44xx_Status::NotInterruptPin: return F("DRDY pin has no interrupt");
        case AFE44xx_Status::Timeout:         return F("timeout");
        case AFE44xx_Status::Unsupported:     return F("not supported on this chip");
    }
    return F("unknown");
}

void printDiagnostics(uint16_t flags) {
    if (flags & AFE44XX_DIAG_PDOC)      Serial.println(F("  photodiode open (probe unplugged?)"));
    if (flags & AFE44XX_DIAG_PDSC)      Serial.println(F("  photodiode short"));
    if (flags & AFE44XX_DIAG_INPSCGND)  Serial.println(F("  photodiode INP shorted to GND"));
    if (flags & AFE44XX_DIAG_INNSCGND)  Serial.println(F("  photodiode INN shorted to GND"));
    if (flags & AFE44XX_DIAG_INPSCLED)  Serial.println(F("  photodiode INP shorted to the LED cable"));
    if (flags & AFE44XX_DIAG_INNSCLED)  Serial.println(F("  photodiode INN shorted to the LED cable"));
    if (flags & AFE44XX_DIAG_LED1OPEN)  Serial.println(F("  LED1 (IR) open"));
    if (flags & AFE44XX_DIAG_LED2OPEN)  Serial.println(F("  LED2 (red) open"));
    if (flags & AFE44XX_DIAG_LEDSC)     Serial.println(F("  LED short"));
    if (flags & AFE44XX_DIAG_OUTPSHGND) Serial.println(F("  LED OUTP shorted to GND"));
    if (flags & AFE44XX_DIAG_OUTNSHGND) Serial.println(F("  LED OUTN shorted to GND"));
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    Serial.print(F("ProtoCentral AFE44xx library v"));
    Serial.println(Protocentral_AFE44xx::getLibraryVersion());

    bool ok = afe.begin();
    Serial.print(F("begin(): "));
    Serial.println(statusText(afe.lastError()));
    if (!ok) return;

    Serial.println(F("\nRegisters:"));
    afe.printRegisters(Serial);

    AFE44xx_Diagnostics diag;
    Serial.print(F("\nProbe diagnostics: "));
    if (!afe.runDiagnostics(diag)) {
        Serial.println(statusText(afe.lastError()));
    } else if (diag.ok()) {
        Serial.println(F("no faults"));
    } else {
        Serial.print(F("DIAG = 0x"));
        Serial.println(diag.flags, HEX);
        printDiagnostics(diag.flags);
    }

    Serial.print(F("\nPRF: "));
    Serial.print(afe.getSampleRate());
    Serial.print(F(" Hz, LED full scale "));
    Serial.print(afe.ledFullScaleMilliamps());
    Serial.println(F(" mA"));

    bool irq = afe.enableDataReadyInterrupt();
    Serial.print(F("DRDY interrupt: "));
    Serial.println(irq ? F("attached") : statusText(afe.lastError()));

    // Count samples over one second
    uint16_t count = 0;
    AFE44xx_Sample s = {};
    t0 = millis();
    while (millis() - t0 < 1000) {
        if (afe.available() && afe.read(s)) count++;
    }
    Serial.print(F("Samples in 1 s: "));
    Serial.print(count);
    Serial.print(F("  missed: "));
    Serial.println(afe.missedSamples());
    if (afe.dataReadyPinFault()) {
        Serial.println(F("DRDY never fired - check the DRDY wire (D2). The driver is reading on a timer."));
    } else if (irq) {
        Serial.println(F("Data path OK"));
    }

    Serial.print(F("Last sample: IR "));
    Serial.print(s.ir());
    Serial.print(F("  red "));
    Serial.print(s.red());
    Serial.print(F("  ambient "));
    Serial.println(s.aled1);
}

void loop() {}
