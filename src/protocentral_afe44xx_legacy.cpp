// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2016-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 1.x compatibility API (AFE44XX), built on Protocentral_AFE44xx and AFE44xx_Algorithms.
//
// https://github.com/Protocentral/protocentral-afe4490-arduino

#include "protocentral_afe44xx.h"

static const int32_t  SPO2_INVALID     = -999;  // 1.x value for "no SpO2"
static const uint32_t REPORT_PERIOD_MS = 4000;  // 1.x reported every 4 s; keep reporting at least that often

AFE44XX::AFE44XX(int cs_pin, int pwdn_pin)
    : _afe((uint8_t)cs_pin, -1, (int8_t)pwdn_pin, -1), _lastCallUs(0), _lastReportMs(0), _firstCall(true) {}

void AFE44XX::afe44xx_init() {
    // The default configuration is the 1.x register profile (500 Hz, 8 averages, LED code 20, RF 250 kOhm)
    _afe.begin();
    _algo.reset();
    _firstCall    = true;
    _lastReportMs = millis();
}

boolean AFE44XX::get_AFE44XX_Data(afe44xx_data* afe44xx_raw_data) {
    AFE44xx_Sample s;
    if (!_afe.readLatest(s)) return false;

    afe44xx_raw_data->IR_data  = s.led1;
    afe44xx_raw_data->RED_data = s.led2;

    // The call rate is unknown: time the calls and let the algorithms resample from it
    uint32_t now = micros();
    bool updated = false;
    if (_firstCall) {
        _firstCall = false;
    } else {
        updated = _algo.processSampleTimed(s.ir(), s.red(), now - _lastCallUs);
    }
    _lastCallUs = now;

    // A new estimate at every beat, and a report at least every 4 s so sketches still see -999
    uint32_t ms = millis();
    if (updated || ms - _lastReportMs >= REPORT_PERIOD_MS) {
        _lastReportMs = ms;
        afe44xx_raw_data->spo2 = _algo.spo2Valid() ? (int32_t)_algo.spo2() : SPO2_INVALID;
        afe44xx_raw_data->buffer_count_overflow = true;
    }
    afe44xx_raw_data->heart_rate = _algo.heartRate();
    return true;
}
