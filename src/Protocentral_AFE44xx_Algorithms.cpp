// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2016-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// https://github.com/Protocentral/protocentral-afe4490-arduino

#include "Protocentral_AFE44xx_Algorithms.h"

// All counts below are at the 50 Hz design rate
static const uint32_t PERIOD_US       = 1000000UL / AFE44XX_ALGO_RATE;  // 20 ms
static const uint32_t MAX_GAP_US      = 250000UL;  // a longer pause between timed samples restarts everything
static const uint16_t MIN_INTERVAL_Q8 = 13 * 256;  // 230 bpm
static const uint16_t MAX_INTERVAL_Q8 = 100 * 256; // 30 bpm
static const uint8_t  MIN_REFRACTORY  = 13;        // samples after a peak in which no other peak is accepted
static const uint16_t BEAT_TIMEOUT    = 125;       // 2.5 s without a beat clears the history
static const uint8_t  WARMUP_SAMPLES  = 50;        // 1 s for the DC estimate to settle
static const uint8_t  BEATS_NEEDED    = 4;
static const int32_t  SATURATION      = 1992294L;  // 95 % of the 22-bit full scale
static const uint32_t BPM_Q8          = 60UL * AFE44XX_ALGO_RATE * 256;  // bpm = BPM_Q8 / interval_q8
static const uint16_t RATIO_MIN       = 20;        // valid R x 1000, the range of the 1.x lookup table
static const uint16_t RATIO_MAX       = 1840;
static const int32_t  I32_MAX         = 2147483647L;  // INT32_MAX needs __STDC_LIMIT_MACROS on avr-libc
static const int32_t  I32_MIN         = -I32_MAX - 1;

AFE44xx_Algorithms::AFE44xx_Algorithms() {
    _rate = AFE44XX_ALGO_RATE;
    reset();
}

bool AFE44xx_Algorithms::begin(uint16_t sampleRate) {
    reset();
    _rate       = sampleRate;
    _rateTooLow = sampleRate < AFE44XX_ALGO_RATE;
    return !_rateTooLow;
}

void AFE44xx_Algorithms::reset() {
    _phase         = 0;
    _sumIr         = 0;
    _sumRed        = 0;
    _count         = 0;
    _avgIntervalUs = 0;
    _rateTooLow    = _rate < AFE44XX_ALGO_RATE;

    _primed  = false;
    _warmup  = WARMUP_SAMPLES;
    _dcIrQ5  = 0;
    _dcRedQ5 = 0;
    memset(_maIr, 0, sizeof(_maIr));
    memset(_maRed, 0, sizeof(_maRed));
    _maIdx = 0;
    memset(_y, 0, sizeof(_y));

    _envelope  = 0;
    _sinceBeat = 0;
    _perfusion = 0;
    _heartRate = 0;
    _spo2      = 0;
    _ratioMedian = 0;
    clearBeats();
    _quality = AFE44XX_QUALITY_NO_PULSE;
}

void AFE44xx_Algorithms::clearBeats() {
    _haveBeat     = false;
    _lastFracQ8   = 0;
    _nBeats       = 0;
    _ringIdx      = 0;
    _irregular    = false;
    _lowPerfusion = false;
    clearExtremes();
}

void AFE44xx_Algorithms::clearExtremes() {
    _irMax  = I32_MIN;
    _irMin  = I32_MAX;
    _redMax = I32_MIN;
    _redMin = I32_MAX;
}

uint8_t AFE44xx_Algorithms::signalQuality() const {
    return _quality | (_rateTooLow ? AFE44XX_QUALITY_RATE_TOO_LOW : 0);
}

bool AFE44xx_Algorithms::heartRateValid() const {
    // A small pulse still gives a usable rate; only SpO2 needs good perfusion
    return (signalQuality() & ~AFE44XX_QUALITY_LOW_PERFUSION) == 0 && _nBeats >= BEATS_NEEDED;
}

bool AFE44xx_Algorithms::spo2Valid() const {
    return signalQuality() == 0 && _nBeats >= BEATS_NEEDED && _ratioMedian > RATIO_MIN &&
           _ratioMedian < RATIO_MAX;
}

// ============================================================================
// Resampling to 50 Hz
// ============================================================================
//
// Input samples are averaged (a boxcar low-pass) until one 50 Hz period has passed,
// tracked by a phase accumulator. This works for integer and non-integer rate ratios;
// with a non-integer ratio each output is placed within one input period of its ideal
// time, which is small against the 20 ms output period.

bool AFE44xx_Algorithms::processSample(int32_t ir, int32_t red) {
    if (_rateTooLow) return false;
    _sumIr += ir;
    _sumRed += red;
    _count++;
    _phase += AFE44XX_ALGO_RATE;
    if (_phase < _rate) return false;
    _phase -= _rate;
    return push();
}

bool AFE44xx_Algorithms::processSampleTimed(int32_t ir, int32_t red, uint32_t intervalUs) {
    if (intervalUs > MAX_GAP_US) {
        uint16_t rate = _rate;
        reset();
        _rate = rate;
    }

    // Smoothed call interval decides whether the caller is fast enough
    if (_avgIntervalUs == 0) {
        _avgIntervalUs = intervalUs;
    } else {
        _avgIntervalUs = (uint32_t)((int32_t)_avgIntervalUs + ((int32_t)intervalUs - (int32_t)_avgIntervalUs) / 8);
    }
    _rateTooLow = _avgIntervalUs > PERIOD_US + PERIOD_US / 20;

    _sumIr += ir;
    _sumRed += red;
    _count++;
    _phase += intervalUs;
    if (_phase < PERIOD_US) return false;
    _phase -= PERIOD_US;
    if (_phase >= PERIOD_US) _phase = 0;  // never emit a burst after a slow call
    return push() && !_rateTooLow;
}

bool AFE44xx_Algorithms::push() {
    int32_t ir  = _sumIr / (int32_t)_count;
    int32_t red = _sumRed / (int32_t)_count;
    _sumIr  = 0;
    _sumRed = 0;
    _count  = 0;
    return process50(ir, red);
}

// ============================================================================
// Beat detection and SpO2 at 50 Hz
// ============================================================================

bool AFE44xx_Algorithms::process50(int32_t ir, int32_t red) {
    if (!_primed) {
        _dcIrQ5  = ir * 32;
        _dcRedQ5 = red * 32;
        _primed  = true;
    }

    // DC: one-pole low-pass, time constant 32 samples (0.64 s)
    _dcIrQ5 += ir - _dcIrQ5 / 32;
    _dcRedQ5 += red - _dcRedQ5 / 32;
    int32_t dcIr  = _dcIrQ5 / 32;
    int32_t dcRed = _dcRedQ5 / 32;

    // AC, smoothed by a 4-point moving average (zero at 12.5 Hz)
    _maIr[_maIdx]  = ir - dcIr;
    _maRed[_maIdx] = red - dcRed;
    _maIdx = (_maIdx + 1) & 3;
    int32_t acIr  = (_maIr[0] + _maIr[1] + _maIr[2] + _maIr[3]) / 4;
    int32_t acRed = (_maRed[0] + _maRed[1] + _maRed[2] + _maRed[3]) / 4;

    // More blood absorbs more light, so systole is a minimum: invert to detect peaks
    _y[0] = _y[1];
    _y[1] = _y[2];
    _y[2] = -acIr;

    uint8_t q = 0;
    if (dcIr < AFE44XX_ALGO_MIN_DC) q |= AFE44XX_QUALITY_NO_SIGNAL;
    if (ir > SATURATION || ir < -SATURATION || red > SATURATION || red < -SATURATION) {
        q |= AFE44XX_QUALITY_SATURATED;
    }

    if (_sinceBeat < 0xFFFF) _sinceBeat++;
    if (_haveBeat && _sinceBeat > BEAT_TIMEOUT) clearBeats();
    if (q) clearBeats();  // no usable signal: start over when it returns
    if (_warmup) _warmup--;

    bool updated = false;
    if (!q && !_warmup && _y[1] > _y[0] && _y[1] >= _y[2]) {
        int32_t threshold = _envelope / 2;
        int32_t floor     = dcIr / 4000;  // 0.025 % of DC
        if (threshold < floor) threshold = floor;
        if (_y[1] > threshold) {
            uint16_t refractory = MIN_REFRACTORY;
            if (_nBeats >= BEATS_NEEDED) {
                // Half the typical interval rejects the dicrotic notch at low heart rates
                uint16_t half = median(_intervals, _nBeats) / 512;
                if (half > refractory) refractory = half;
            }
            if (!_haveBeat || _sinceBeat - 1 >= refractory) updated = onPeak(dcIr, dcRed);
        }
    }

    // The current sample belongs to the beat that has just started
    if (acIr > _irMax) _irMax = acIr;
    if (acIr < _irMin) _irMin = acIr;
    if (acRed > _redMax) _redMax = acRed;
    if (acRed < _redMin) _redMin = acRed;

    _envelope -= _envelope / 128;  // forget old beats over ~2.5 s

    if (_nBeats < BEATS_NEEDED || _irregular) q |= AFE44XX_QUALITY_NO_PULSE;
    if (_lowPerfusion) q |= AFE44XX_QUALITY_LOW_PERFUSION;
    _quality = q;
    return updated;
}

// Called when sample n-1 is a peak. Returns true when the results were updated.
bool AFE44xx_Algorithms::onPeak(int32_t dcIr, int32_t dcRed) {
    // Sub-sample peak position by parabolic interpolation, in 1/256 sample
    int32_t den     = _y[0] - 2 * _y[1] + _y[2];  // < 0 at a maximum
    int16_t fracQ8  = 0;
    if (den != 0) {
        int32_t f = ((_y[0] - _y[2]) * 128) / den;
        fracQ8 = (int16_t)constrain(f, -128L, 128L);
    }

    bool updated = false;
    if (_haveBeat) {
        int32_t intervalQ8 = (int32_t)(_sinceBeat - 1) * 256 + fracQ8 - _lastFracQ8;
        if (intervalQ8 < MIN_INTERVAL_Q8) return false;  // too close: not a new beat

        int32_t ppIr  = _irMax - _irMin;
        int32_t ppRed = _redMax - _redMin;
        if (intervalQ8 <= MAX_INTERVAL_Q8 && ppIr > 0 && ppRed > 0 && dcIr > 0 && dcRed > 0) {
            float r    = ((float)ppRed / (float)dcRed) / ((float)ppIr / (float)dcIr);
            float pi   = (float)ppIr * 10000.0f / (float)dcIr;
            _perfusion    = (uint16_t)constrain(pi, 0.0f, 65535.0f);
            _lowPerfusion = _perfusion < AFE44XX_ALGO_MIN_PI;

            _intervals[_ringIdx] = (uint16_t)intervalQ8;
            _ratios[_ringIdx]    = (uint16_t)constrain(r * 1000.0f, 0.0f, 65535.0f);
            _ringIdx = (_ringIdx + 1) % 5;
            if (_nBeats < 5) _nBeats++;

            uint16_t med = median(_intervals, _nBeats);
            _irregular = false;
            for (uint8_t i = 0; i < _nBeats; i++) {
                // Any interval more than 25 % off the median: the rhythm or the detection is unreliable
                uint16_t d = _intervals[i] > med ? _intervals[i] - med : med - _intervals[i];
                if (d > med / 4) _irregular = true;
            }
            uint32_t bpm = (BPM_Q8 + med / 2) / med;  // med >= MIN_INTERVAL_Q8, so bpm <= 230
            _heartRate = (uint8_t)(bpm > 255 ? 255 : bpm);

            _ratioMedian = median(_ratios, _nBeats);
            float rm = _ratioMedian / 1000.0f;
            float s  = AFE44XX_SPO2_A * rm * rm + AFE44XX_SPO2_B * rm + AFE44XX_SPO2_C;
            _spo2 = (uint8_t)constrain(s + 0.5f, 0.0f, 100.0f);
            updated = true;
        }
    }

    _envelope   = _envelope ? (_envelope * 3 + _y[1]) / 4 : _y[1];
    _haveBeat   = true;
    _sinceBeat  = 1;  // the peak was one sample ago
    _lastFracQ8 = fracQ8;
    clearExtremes();
    return updated;
}

uint16_t AFE44xx_Algorithms::median(const uint16_t* v, uint8_t n) {
    if (n == 0) return 0;
    uint16_t s[5];
    for (uint8_t i = 0; i < n; i++) {
        uint16_t x = v[i];
        uint8_t  j = i;
        for (; j > 0 && s[j - 1] > x; j--) s[j] = s[j - 1];
        s[j] = x;
    }
    return (n & 1) ? s[n / 2] : (uint16_t)(((uint32_t)s[n / 2 - 1] + s[n / 2]) / 2);
}
