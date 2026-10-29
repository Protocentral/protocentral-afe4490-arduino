// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 02.OpenView
//
// Streams the red and IR PPG with heart rate and SpO2 to ProtoCentral OpenView
// (https://github.com/Protocentral/protocentral_openview).
// In OpenView, choose the "AFE4490 Breakout" board and this serial port.
//
// Wire format (must match OpenView's afe4490 descriptor - change both together):
//   57600 baud, 100 packets/s
//   0x0A 0xFA <len=10> 0x00 <type=0x02> | RED int32 | IR int32 | HR uint8 | SpO2 uint8 | 0x00 0x0B
//   (little-endian; HR and SpO2 are 0 while invalid)
//
// The AFE runs at 500 Hz. Every sample goes to the algorithms; the waveform is
// averaged over 5 samples because 57600 baud carries about 300 packets/s.
//
// This sketch prints nothing but packets - any other text would corrupt the stream.
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

const uint8_t PKT_START_1 = 0x0A;
const uint8_t PKT_START_2 = 0xFA;
const uint8_t PKT_TYPE    = 0x02;
const uint8_t PKT_STOP_1  = 0x00;
const uint8_t PKT_STOP_2  = 0x0B;
const uint8_t PAYLOAD_LEN = 10;
const uint8_t AVERAGE     = 5;  // 500 Hz -> 100 packets/s

Protocentral_AFE44xx afe(PIN_CS, PIN_DRDY, PIN_PWDN, PIN_RESET);
AFE44xx_Algorithms algo;

static void putInt32(uint8_t* p, int32_t v) {
    for (uint8_t i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void sendPacket(int32_t red, int32_t ir, uint8_t hr, uint8_t spo2) {
    uint8_t pkt[5 + PAYLOAD_LEN + 2] = {PKT_START_1, PKT_START_2, PAYLOAD_LEN, 0x00, PKT_TYPE};
    putInt32(&pkt[5], red);
    putInt32(&pkt[9], ir);
    pkt[13] = hr;
    pkt[14] = spo2;
    pkt[15] = PKT_STOP_1;
    pkt[16] = PKT_STOP_2;
    Serial.write(pkt, sizeof(pkt));
}

void setup() {
    Serial.begin(57600);

    if (!afe.begin()) {
        // No text allowed on the stream: blink the LED instead
#ifdef LED_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
        while (1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
#else
        while (1) delay(1000);
#endif
    }
    afe.enableDataReadyInterrupt();
    algo.begin(afe.getSampleRate());
}

int32_t redSum = 0, irSum = 0;
uint8_t count = 0;

void loop() {
    AFE44xx_Sample s;
    if (!afe.available() || !afe.read(s)) return;

    algo.processSample(s.ir(), s.red());

    redSum += s.red();
    irSum += s.ir();
    if (++count < AVERAGE) return;
    sendPacket(redSum / AVERAGE, irSum / AVERAGE, algo.heartRate(), algo.spo2());
    redSum = irSum = 0;
    count = 0;
}
