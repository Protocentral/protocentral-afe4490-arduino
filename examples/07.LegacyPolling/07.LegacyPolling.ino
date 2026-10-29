// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 07.LegacyPolling
//
// The 1.x API, unchanged: AFE44XX, afe44xx_init() and get_AFE44XX_Data(). Sketches
// written for 1.x build and run as before, with two differences in 2.0:
//  - SpO2 and heart rate are correct at any call rate of at least 50 Hz (1.x was only
//    right if get_AFE44XX_Data() was called at exactly 500 Hz)
//  - a new estimate arrives at every heartbeat instead of every 4 s
//
// New sketches should use Protocentral_AFE44xx with DRDY (see 03.SpO2HeartRate).
// 115200 baud.
//
// For research and education only - not a medical device.

#include <SPI.h>
#include "protocentral_afe44xx.h"

#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;
int32_t heart_rate_prev = 0;
int32_t spo2_prev = 0;

void setup() {
    Serial.begin(115200);
    Serial.println(F("Initialising AFE44xx..."));

    SPI.begin();
    afe44xx.afe44xx_init();
    Serial.println(F("Done"));
}

void loop() {
    delay(8);  // ~125 calls per second, as in the 1.x examples

    afe44xx.get_AFE44XX_Data(&afe44xx_raw_data);

    if (afe44xx_raw_data.buffer_count_overflow) {
        afe44xx_raw_data.buffer_count_overflow = false;

        if (afe44xx_raw_data.spo2 == -999) {
            Serial.println(F("Probe error or no finger"));
        } else if (heart_rate_prev != afe44xx_raw_data.heart_rate || spo2_prev != afe44xx_raw_data.spo2) {
            heart_rate_prev = afe44xx_raw_data.heart_rate;
            spo2_prev = afe44xx_raw_data.spo2;

            Serial.print(F("SpO2: "));
            Serial.print(afe44xx_raw_data.spo2);
            Serial.print(F(" %, pulse rate: "));
            Serial.print(afe44xx_raw_data.heart_rate);
            Serial.println(F(" bpm"));
        }
    }
}
