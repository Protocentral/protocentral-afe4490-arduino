// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Optional SpO2 and heart-rate processing for the AFE4490 / AFE4400.
//
// Input at any sample rate of at least 50 Hz is low-pass filtered and resampled to a
// fixed 50 Hz design rate, so results do not depend on the PRF or on how often the
// sketch calls in. Streaming beat detector on the IR channel; per beat, the red/IR AC
// ratio is a least-squares fit over the whole beat; median over 5 beats. Integer filters, ~150 bytes of RAM.
//
// The SpO2 calibration curve must be validated against a reference oximeter for the
// probe in use. For research and education only - not a medical device.
//
// https://github.com/Protocentral/protocentral-afe4490-arduino

#ifndef PROTOCENTRAL_AFE44XX_ALGORITHMS_H
#define PROTOCENTRAL_AFE44XX_ALGORITHMS_H

#include <Arduino.h>

#define AFE44XX_ALGO_RATE 50  ///< Design rate in Hz. Every input is resampled to it; slower input is rejected

// SpO2 = A x R^2 + B x R + C, with R = (AC_red / DC_red) / (AC_ir / DC_ir).
// Defaults are the curve used by 1.x; calibrate for your probe.
#ifndef AFE44XX_SPO2_A
#define AFE44XX_SPO2_A -45.060f
#endif
#ifndef AFE44XX_SPO2_B
#define AFE44XX_SPO2_B 30.354f
#endif
#ifndef AFE44XX_SPO2_C
#define AFE44XX_SPO2_C 94.845f
#endif

#ifndef AFE44XX_ALGO_MIN_DC
#define AFE44XX_ALGO_MIN_DC 1000L  ///< Ambient-subtracted IR counts below this mean no finger / no light
#endif
#ifndef AFE44XX_ALGO_MIN_PI
#define AFE44XX_ALGO_MIN_PI 5      ///< Minimum IR perfusion index, in 0.01 % (5 = 0.05 %)
#endif

// signalQuality() flags. 0 = good signal
#define AFE44XX_QUALITY_NO_SIGNAL     0x01  ///< IR level below AFE44XX_ALGO_MIN_DC (no finger, LED off or probe unplugged)
#define AFE44XX_QUALITY_SATURATED     0x02  ///< ADC near full scale: lower the LED current or TIA gain
#define AFE44XX_QUALITY_LOW_PERFUSION 0x04  ///< Pulse too small relative to DC
#define AFE44XX_QUALITY_NO_PULSE      0x08  ///< Fewer than 4 regular beats in the last few seconds
#define AFE44XX_QUALITY_RATE_TOO_LOW  0x10  ///< Input slower than 50 Hz

class AFE44xx_Algorithms {
public:
    AFE44xx_Algorithms();

    /** @brief Reset and set the input rate. Returns false (and reports invalid) below 50 Hz. */
    bool begin(uint16_t sampleRate);

    /** @brief Clear all filters and beat history (e.g. after the finger is removed). */
    void reset();

    /**
     * @brief Feed one ambient-subtracted sample per period, at the rate passed to begin().
     * Use AFE44xx_Sample::ir() and red(). Returns true when a beat updated the results.
     */
    bool processSample(int32_t ir, int32_t red);

    /**
     * @brief Feed a sample taken @p intervalUs after the previous one, for callers that
     * cannot guarantee a fixed rate. Returns true when a beat updated the results.
     */
    bool processSampleTimed(int32_t ir, int32_t red, uint32_t intervalUs);

    uint8_t  heartRate() const { return heartRateValid() ? _heartRate : 0; }  ///< bpm, 0 when invalid
    uint8_t  spo2() const { return spo2Valid() ? _spo2 : 0; }                  ///< %, 0 when invalid
    bool     heartRateValid() const;
    bool     spo2Valid() const;
    uint8_t  signalQuality() const;                            ///< AFE44XX_QUALITY_* flags, 0 = good
    uint16_t perfusionIndex() const { return _perfusion; }     ///< IR AC/DC of the last beat, in 0.01 %
    uint16_t ratio() const { return _ratioMedian; }            ///< Median R of the last beats, x 1000
    int32_t  filteredIr() const { return _y[2]; }              ///< Band-passed IR at 50 Hz, beats pointing up

private:
    // Resampler to 50 Hz
    uint16_t _rate;           // input rate for processSample()
    uint32_t _phase;
    int32_t  _sumIr, _sumRed;
    uint16_t _count;
    uint32_t _avgIntervalUs;  // processSampleTimed(): smoothed call interval
    bool     _rateTooLow;

    // Conditioning at 50 Hz
    bool    _primed;
    uint8_t _warmup;               // samples left before beats are accepted
    int32_t _dcIrQ5, _dcRedQ5;     // DC estimate x 32
    int32_t _maIr[4], _maRed[4];   // 4-point moving average of AC
    uint8_t _maIdx;
    int32_t _y[3];                 // inverted, smoothed IR AC: n-2, n-1, n

    // Beat detector
    int32_t  _envelope;
    uint16_t _sinceBeat;           // samples since the last accepted peak
    int16_t  _lastFracQ8;
    bool     _haveBeat;
    int32_t  _irMax, _irMin;       // smoothed IR AC extremes since the last peak (perfusion index)
    uint16_t _beatN;               // samples since the last peak, and their sums for the red/IR fit
    float    _sx, _sy, _sxx, _sxy;

    // Results
    uint16_t _intervals[5];        // beat intervals, samples x 256
    uint16_t _ratios[5];           // R x 1000 of the same beats
    uint8_t  _nBeats, _ringIdx;
    uint8_t  _heartRate, _spo2;
    uint16_t _perfusion;
    uint16_t _ratioMedian;
    uint8_t  _quality;             // flags from the signal, without RATE_TOO_LOW
    bool     _irregular;
    bool     _lowPerfusion;

    bool push();
    bool process50(int32_t ir, int32_t red);
    bool onPeak(int32_t dcIr, int32_t dcRed);
    void clearBeats();
    void clearExtremes();
    static uint16_t median(const uint16_t* v, uint8_t n);
};

#endif  // PROTOCENTRAL_AFE44XX_ALGORITHMS_H
