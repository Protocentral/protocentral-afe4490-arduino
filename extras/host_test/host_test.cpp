// Host tests for the AFE44xx driver and algorithms. No hardware: SPI talks to a fake
// register file, and the algorithms get synthetic PPG. Run with extras/host_test/run.sh.
//
// These tests check the arithmetic and the logic. They do not replace validation against
// a reference oximeter on real subjects.

#include "SPI.h"
#include "protocentral_afe44xx.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        checks++;                                         \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("FAIL %s:%d  ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

// ---------------------------------------------------------------------------
// Timing engine
// ---------------------------------------------------------------------------

// The register values 1.4.0 wrote in afe44xx_init(), LED2STC .. PRPCOUNT
static const uint16_t V1_TIMING[AFE44XX_NUM_TIMING_REGS] = {
    0x1770, 0x1F3E, 0x1770, 0x1F3F, 0x0000, 0x07CE, 0x07D0, 0x0F9E, 0x07D0, 0x0F9F,
    0x0FA0, 0x176E, 0x0002, 0x07CF, 0x07D2, 0x0F9F, 0x0FA2, 0x176F, 0x1772, 0x1F3F,
    0x0000, 0x0000, 0x07D0, 0x07D0, 0x0FA0, 0x0FA0, 0x1770, 0x1770, 0x1F3F};

static void testTiming() {
    uint16_t t[AFE44XX_NUM_TIMING_REGS];
    CHECK(Protocentral_AFE44xx::computeTiming(500, 25, t), "500 Hz rejected");
    for (int i = 0; i < AFE44XX_NUM_TIMING_REGS; i++) {
        CHECK(t[i] == V1_TIMING[i], "reg 0x%02X = 0x%04X, 1.x had 0x%04X", i + 1, t[i], V1_TIMING[i]);
    }

    CHECK(!Protocentral_AFE44xx::computeTiming(62, 25, t), "62 Hz accepted");
    CHECK(!Protocentral_AFE44xx::computeTiming(5001, 25, t), "5001 Hz accepted");
    CHECK(!Protocentral_AFE44xx::computeTiming(500, 26, t), "26 %% duty accepted");
    CHECK(!Protocentral_AFE44xx::computeTiming(500, 0, t), "0 %% duty accepted");

    // Invariants over the whole range
    for (uint16_t prf = 63; prf <= 5000; prf += (prf < 300 ? 1 : 7)) {
        for (uint8_t duty = 1; duty <= 25; duty++) {
            if (!Protocentral_AFE44xx::computeTiming(prf, duty, t)) {
                CHECK(prf * 1UL * duty > 0 && 4000000UL / prf * duty / 100 < 4, "%u Hz %u %% rejected", prf, duty);
                continue;
            }
            uint16_t prp = t[28];
            CHECK(prp == 4000000UL / prf - 1, "PRPCOUNT at %u Hz", prf);
            for (int i = 0; i < 28; i++) CHECK(t[i] <= prp, "%u Hz %u %%: reg 0x%02X beyond PRP", prf, duty, i + 1);
            // start <= end for every window
            static const int pairs[][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11},
                                           {12, 13}, {14, 15}, {16, 17}, {18, 19}};
            for (auto& p : pairs) CHECK(t[p[0]] <= t[p[1]], "%u Hz %u %%: window 0x%02X", prf, duty, p[0] + 1);
            // Each sample window lies inside its LED pulse
            CHECK(t[0] >= t[2] && t[1] <= t[3], "%u Hz: LED2 sample outside pulse", prf);
            CHECK(t[6] >= t[8] && t[7] <= t[9], "%u Hz: LED1 sample outside pulse", prf);
            // Ambient windows as long as the LED windows
            CHECK(t[1] - t[0] == t[5] - t[4] && t[7] - t[6] == t[11] - t[10], "%u Hz: ambient width", prf);
            // Conversions start after their ADC reset
            CHECK(t[12] > t[21] && t[14] > t[23] && t[16] > t[25] && t[18] > t[27], "%u Hz: conversion vs reset", prf);
        }
    }

    CHECK(Protocentral_AFE44xx::maxAverages(500) == 10, "maxAverages(500)");
    CHECK(Protocentral_AFE44xx::maxAverages(625) == 8, "maxAverages(625)");
    CHECK(Protocentral_AFE44xx::maxAverages(1250) == 4, "maxAverages(1250)");
    CHECK(Protocentral_AFE44xx::maxAverages(5000) == 1, "maxAverages(5000)");
    CHECK(Protocentral_AFE44xx::maxAverages(63) == 16, "maxAverages(63)");
}

// ---------------------------------------------------------------------------
// Driver against the fake register file
// ---------------------------------------------------------------------------

static void testDriver() {
    fake::reset();
    Protocentral_AFE44xx afe(7, 2, 4, 5);
    CHECK(afe.begin(), "begin() failed, status %d", (int)afe.lastError());

    // The 1.x profile
    CHECK(fake::regs[AFE44XX_REG_CONTROL1] == 0x000107, "CONTROL1 0x%06X", fake::regs[AFE44XX_REG_CONTROL1]);
    CHECK(fake::regs[AFE44XX_REG_TIAGAIN] == 0, "TIAGAIN 0x%06X", fake::regs[AFE44XX_REG_TIAGAIN]);
    CHECK(fake::regs[AFE44XX_REG_TIA_AMB_GAIN] == 0x000001, "TIA_AMB_GAIN 0x%06X", fake::regs[AFE44XX_REG_TIA_AMB_GAIN]);
    CHECK(fake::regs[AFE44XX_REG_LEDCNTRL] == 0x001414, "LEDCNTRL 0x%06X", fake::regs[AFE44XX_REG_LEDCNTRL]);
    CHECK(fake::regs[AFE44XX_REG_CONTROL2] == 0, "CONTROL2 0x%06X", fake::regs[AFE44XX_REG_CONTROL2]);
    for (int i = 0; i < AFE44XX_NUM_TIMING_REGS; i++) {
        CHECK(fake::regs[1 + i] == V1_TIMING[i], "begin(): reg 0x%02X", i + 1);
    }
    CHECK(afe.isConnected(), "isConnected() after begin");
    CHECK(afe.getSampleRate() == 500, "getSampleRate");

    // Sample read: six registers, sign-extended from 22 bits
    fake::regs[AFE44XX_REG_LED2VAL]       = 0x1FFFFF;  // +2097151
    fake::regs[AFE44XX_REG_ALED2VAL]      = 0x200000;  // -2097152
    fake::regs[AFE44XX_REG_LED1VAL]       = 0xFFFFFF;  // top bits ignored: -1
    fake::regs[AFE44XX_REG_ALED1VAL]      = 1234;
    fake::regs[AFE44XX_REG_LED2_ALED2VAL] = 0x3FFFFE;  // -2
    fake::regs[AFE44XX_REG_LED1_ALED1VAL] = 100000;
    AFE44xx_Sample s;
    CHECK(afe.read(s), "read()");
    CHECK(s.led2 == 2097151 && s.aled2 == -2097152 && s.led1 == -1 && s.aled1 == 1234, "sample values");
    CHECK(s.red() == -2 && s.ir() == 100000, "ambient-subtracted values");

    // Setters
    CHECK(afe.setPRF(1000), "setPRF(1000)");
    CHECK(fake::regs[AFE44XX_REG_PRPCOUNT] == 3999, "PRPCOUNT after setPRF");
    CHECK(afe.getAverages() == 5, "averages reduced to %u", afe.getAverages());
    CHECK(fake::regs[AFE44XX_REG_CONTROL1] == 0x000104, "CONTROL1 after setPRF 0x%06X", fake::regs[AFE44XX_REG_CONTROL1]);
    CHECK(!afe.setAverages(6), "6 averages accepted at 1 kHz");
    CHECK(afe.lastError() == AFE44xx_Status::InvalidArgument, "status after bad averages");
    CHECK(!afe.setPRF(20), "20 Hz accepted");
    CHECK(afe.getSampleRate() == 1000, "PRF changed by a rejected call");
    CHECK(afe.setLedCurrent(AFE44xx_Led::Red, 0x80), "setLedCurrent");
    CHECK(fake::regs[AFE44XX_REG_LEDCNTRL] == 0x001480, "LEDCNTRL 0x%06X", fake::regs[AFE44XX_REG_LEDCNTRL]);
    CHECK(afe.setReceiverGain(AFE44xx_TiaRf::RF_1M, AFE44xx_TiaCf::pF_25, AFE44xx_Stage2Gain::X2), "setReceiverGain");
    CHECK(fake::regs[AFE44XX_REG_TIA_AMB_GAIN] == (0x4000 | 0x200 | (3 << 3) | 6), "TIA_AMB_GAIN 0x%06X",
          fake::regs[AFE44XX_REG_TIA_AMB_GAIN]);
    CHECK(afe.setLed1ReceiverGain(true, AFE44xx_TiaRf::RF_100K), "setLed1ReceiverGain");
    CHECK(fake::regs[AFE44XX_REG_TIAGAIN] == 0x8002, "TIAGAIN 0x%06X", fake::regs[AFE44XX_REG_TIAGAIN]);
    CHECK(afe.setAmbientDac(10) && !afe.setAmbientDac(11), "ambient DAC range");
    CHECK(afe.setTxReference(AFE44xx_TxRef::V0_5) && afe.ledFullScaleMilliamps() == 100, "TX_REF 0.5 V");
    CHECK(afe.setLedRange(AFE44xx_LedRange::Half) && afe.ledFullScaleMilliamps() == 50, "half range");
    CHECK(afe.enableDataReadyInterrupt(), "enableDataReadyInterrupt");

    AFE44xx_Diagnostics d;
    CHECK(afe.runDiagnostics(d) && d.ok(), "diagnostics on a clean fake");

    // AFE4400: forced bits, no separate gain
    fake::reset();
    Protocentral_AFE44xx ox(7, 2, 4, 5);
    AFE44xx_Config cfg;
    cfg.chip = AFE44xx_Chip::AFE4400;
    CHECK(ox.begin(cfg), "AFE4400 begin() failed, status %d", (int)ox.lastError());
    CHECK(fake::regs[AFE44XX_REG_LEDCNTRL] == 0x011414, "AFE4400 LEDCNTRL 0x%06X", fake::regs[AFE44XX_REG_LEDCNTRL]);
    CHECK(fake::regs[AFE44XX_REG_CONTROL2] == 0x020100, "AFE4400 CONTROL2 0x%06X", fake::regs[AFE44XX_REG_CONTROL2]);
    CHECK(ox.ledFullScaleMilliamps() == 50, "AFE4400 full scale");
    CHECK(!ox.setLed1ReceiverGain(true) && ox.lastError() == AFE44xx_Status::Unsupported, "AFE4400 separate gain");
    CHECK(!ox.setFilterCorner1kHz(true), "AFE4400 filter corner");
    CHECK(ox.writeRegister(AFE44XX_REG_TIAGAIN, 0x8000) && fake::regs[AFE44XX_REG_TIAGAIN] == 0, "AFE4400 TIAGAIN masked");
    cfg.separateGain = true;
    CHECK(!ox.begin(cfg) && ox.lastError() == AFE44xx_Status::Unsupported, "AFE4400 config with separate gain");

    // No chip
    fake::reset();
    fake::absent = true;
    Protocentral_AFE44xx none(7);
    CHECK(!none.begin() && none.lastError() == AFE44xx_Status::SpiError, "begin() without a chip");
    CHECK(!none.available(), "available() without a chip");
    fake::absent = false;
}

// ---------------------------------------------------------------------------
// Algorithms on synthetic PPG
// ---------------------------------------------------------------------------

static double expectedSpo2(double r) {
    return AFE44XX_SPO2_A * r * r + AFE44XX_SPO2_B * r + AFE44XX_SPO2_C;
}

// Uniform noise in [-1, 1] from a fixed LCG, so every platform sees the same input
// (rand() differs between libc implementations)
static uint32_t noiseState = 1;
static double noise() {
    noiseState = noiseState * 1664525UL + 1013904223UL;
    return ((noiseState >> 8) / 16777215.0 - 0.5) * 2.0;
}

// Light reaching the photodiode: DC minus a pulse with a dicrotic wave, plus wander and noise
struct Ppg {
    double hr, r, piIr, noiseFrac;
    void at(double t, int32_t& ir, int32_t& red) const {
        double ph = fmod(t * hr / 60.0, 1.0);
        double p  = exp(-pow((ph - 0.15) / 0.07, 2)) + 0.4 * exp(-pow((ph - 0.45) / 0.08, 2));
        double wander = 0.2 * sin(2 * M_PI * 0.25 * t);
        double irAc   = piIr * (p + wander + noiseFrac * noise());
        double redAc  = piIr * r * (p + wander + noiseFrac * noise());
        ir  = (int32_t)(400000.0 * (1.0 - irAc));
        red = (int32_t)(300000.0 * (1.0 - redAc));
    }
};

// Tolerances: the synthetic noise is added per input sample, so at low PRF (few samples
// averaged per 50 Hz output) single runs scatter by up to ~2.3 SpO2 points around an
// unbiased mean. Each run must be within SPO2_TOL; the mean over a rate's runs must be
// within SPO2_BIAS_TOL, which catches a systematic error.
static const double SPO2_TOL      = 2.5;
static const double SPO2_BIAS_TOL = 0.5;
static const double HR_TOL        = 2.0;

static void testAlgorithmsAtRate() {
    static const uint16_t rates[] = {63, 100, 125, 250, 333, 500, 625, 1000, 1200, 3000, 5000};
    static const double hrs[]     = {45, 75, 120, 180};
    static const double ratios[]  = {0.5, 0.8, 1.1};
    for (uint16_t rate : rates) {
        double errSum = 0;
        int    runs   = 0;
        for (double hr : hrs) {
            for (double r : ratios) {
                Ppg ppg = {hr, r, 0.02, 0.03};
                AFE44xx_Algorithms algo;
                CHECK(algo.begin(rate), "begin(%u)", rate);
                for (uint32_t n = 0; n < rate * 30UL; n++) {
                    int32_t ir, red;
                    ppg.at((double)n / rate, ir, red);
                    algo.processSample(ir, red);
                }
                double want = expectedSpo2(r);
                CHECK(algo.heartRateValid() && fabs(algo.heartRate() - hr) <= HR_TOL,
                      "%u Hz, %.0f bpm, R %.1f: HR %u (valid %d, quality 0x%02X)", rate, hr, r, algo.heartRate(),
                      algo.heartRateValid(), algo.signalQuality());
                CHECK(algo.spo2Valid() && fabs(algo.spo2() - want) <= SPO2_TOL,
                      "%u Hz, %.0f bpm, R %.1f: SpO2 %u, want %.1f (ratio %u, valid %d, quality 0x%02X)", rate, hr, r,
                      algo.spo2(), want, algo.ratio(), algo.spo2Valid(), algo.signalQuality());
                errSum += algo.spo2() - want;
                runs++;
            }
        }
        CHECK(fabs(errSum / runs) <= SPO2_BIAS_TOL, "%u Hz: mean SpO2 error %+.2f", rate, errSum / runs);
    }
}

// The 1.x path: a sketch reads the latest 500 Hz sample whenever loop() comes round
static void runTimed(double baseUs, double jitterUs, double hr, double r, AFE44xx_Algorithms& algo) {
    Ppg ppg = {hr, r, 0.02, 0.03};
    algo.reset();
    double t = 0;
    while (t < 30.0) {
        double dt = (baseUs + jitterUs * noise()) / 1e6;
        t += dt;
        double held = floor(t * 500.0) / 500.0;  // the AFE holds its last 500 Hz sample
        int32_t ir, red;
        ppg.at(held, ir, red);
        algo.processSampleTimed(ir, red, (uint32_t)(dt * 1e6 + 0.5));
    }
}

static void testAlgorithmsTimed() {
    AFE44xx_Algorithms algo;
    static const double periods[] = {2000, 8000, 10000, 15000, 19000};
    for (double p : periods) {
        runTimed(p, p * 0.15, 72, 0.7, algo);
        CHECK(algo.heartRateValid() && fabs(algo.heartRate() - 72) <= HR_TOL, "timed %.0f us: HR %u (quality 0x%02X)", p,
              algo.heartRate(), algo.signalQuality());
        CHECK(algo.spo2Valid() && fabs(algo.spo2() - expectedSpo2(0.7)) <= SPO2_TOL, "timed %.0f us: SpO2 %u", p,
              algo.spo2());
    }
    runTimed(25000, 1000, 72, 0.7, algo);
    CHECK(!algo.heartRateValid() && !algo.spo2Valid() && (algo.signalQuality() & AFE44XX_QUALITY_RATE_TOO_LOW),
          "40 Hz calls must be reported invalid (quality 0x%02X)", algo.signalQuality());
}

static void testAlgorithmsInvalid() {
    AFE44xx_Algorithms algo;
    CHECK(!algo.begin(40), "begin(40) accepted");
    CHECK(algo.signalQuality() & AFE44XX_QUALITY_RATE_TOO_LOW, "rate flag");

    // No finger: ambient-subtracted level near zero
    algo.begin(500);
    for (int n = 0; n < 500 * 10; n++) algo.processSample(200 + (int32_t)(50 * noise()), 150);
    CHECK((algo.signalQuality() & AFE44XX_QUALITY_NO_SIGNAL) && !algo.spo2Valid() && algo.heartRate() == 0,
          "no finger (quality 0x%02X)", algo.signalQuality());

    // Saturated ADC
    algo.begin(500);
    for (int n = 0; n < 500 * 10; n++) algo.processSample(2097151, 2097151);
    CHECK((algo.signalQuality() & AFE44XX_QUALITY_SATURATED) && !algo.spo2Valid(), "saturation");

    // Steady light, no pulse
    algo.begin(500);
    for (int n = 0; n < 500 * 10; n++) algo.processSample(400000 + (int32_t)(20 * noise()), 300000);
    CHECK((algo.signalQuality() & AFE44XX_QUALITY_NO_PULSE) && !algo.heartRateValid(), "no pulse (quality 0x%02X)",
          algo.signalQuality());

    // Finger removed after a good reading: invalid within a few seconds
    algo.begin(500);
    Ppg ppg = {75, 0.8, 0.02, 0.03};
    for (int n = 0; n < 500 * 20; n++) {
        int32_t ir, red;
        ppg.at(n / 500.0, ir, red);
        algo.processSample(ir, red);
    }
    CHECK(algo.spo2Valid(), "valid before removal");
    for (int n = 0; n < 500 * 3; n++) algo.processSample(400000, 300000);
    CHECK(!algo.heartRateValid() && !algo.spo2Valid(), "still valid 3 s after the pulse stopped");
}

int main() {
    testTiming();
    testDriver();
    testAlgorithmsAtRate();
    testAlgorithmsTimed();
    testAlgorithmsInvalid();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
