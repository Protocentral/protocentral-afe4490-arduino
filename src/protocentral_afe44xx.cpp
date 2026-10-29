// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI AFE4490 and AFE4400 pulse-oximeter analog front ends.
//
// https://github.com/Protocentral/protocentral-afe4490-arduino

#include "protocentral_afe44xx.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define AFE44XX_ISR_ATTR IRAM_ATTR
#else
#define AFE44XX_ISR_ATTR
#endif

// Timing, from SBAS602H section 7.7 and the CONTROL0 / DIAG descriptions
static const uint32_t TIMER_CLOCK_HZ  = 4000000UL;  // timing engine clock (8 MHz crystal / 2)
static const uint16_t RESET_PULSE_US  = 1000;       // RESET low > 0.5 ms (t3)
static const uint16_t RESET_WAIT_US   = 10;         // RESET high to first SPI command > 1 us (t4)
static const uint8_t  DIAG_WAIT_MS    = 20;         // diagnostics take 8 ms (fast) or 16 ms (slow, AFE4400)
static const uint8_t  DIAG_SETTLE_MS  = 30;         // receiver filters settle ~28 ms after diagnostics
static const uint16_t PRF_MIN         = 63;         // 62.5 Hz rounded up: PRPCOUNT <= 64000
static const uint16_t PRF_MAX         = 5000;
static const uint16_t MIN_PULSE       = 4;          // shortest LED / sample window, in timer counts
static const uint16_t PROBE_PATTERN   = 0xA5C3;     // written to LED2STC by the presence check
static const uint8_t  AMBIENT_DAC_MAX = 10;         // uA

Protocentral_AFE44xx* Protocentral_AFE44xx::_irqInstances[AFE44XX_MAX_IRQ_INSTANCES] = {nullptr, nullptr};

Protocentral_AFE44xx::Protocentral_AFE44xx(uint8_t csPin, int8_t drdyPin, int8_t pdnPin, int8_t resetPin,
                                           SPIClass& spi)
    : _csPin(csPin),
      _drdyPin(drdyPin),
      _pdnPin(pdnPin),
      _resetPin(resetPin),
      _spi(&spi),
      _spiSettings(AFE44XX_SPI_CLOCK, MSBFIRST, SPI_MODE0),
      _lastError(AFE44xx_Status::NotInitialized),
      _initialized(false),
      _running(false),
      _standby(false),
      _spiRead(false),
      _beginMs(0),
      _periodUs(2000),
      _irqCount(0),
      _drdyFlag(false),
      _irqSlot(-1),
      _drdyFault(false),
      _lastIrqCount(0),
      _watchCount(0),
      _watchUs(0),
      _nextPollUs(0),
      _missed(0),
      _sequence(0) {}

// ============================================================================
// Initialisation
// ============================================================================

static AFE44xx_Status validateConfig(const AFE44xx_Config& c) {
    if (c.prf < PRF_MIN || c.prf > PRF_MAX || c.dutyCycle < 1 || c.dutyCycle > 25 || c.averages < 1 ||
        c.averages > 16 || c.ambientDac > AMBIENT_DAC_MAX ||
        static_cast<uint8_t>(c.stage2Gain) > static_cast<uint8_t>(AFE44xx_Stage2Gain::X4) ||
        static_cast<uint8_t>(c.stage2Gain1) > static_cast<uint8_t>(AFE44xx_Stage2Gain::X4) ||
        static_cast<uint8_t>(c.ledRange) == 2 || static_cast<uint8_t>(c.ledRange) > 3 ||
        static_cast<uint8_t>(c.txRef) > 2) {
        return AFE44xx_Status::InvalidArgument;
    }
    uint16_t dummy[AFE44XX_NUM_TIMING_REGS];
    if (!Protocentral_AFE44xx::computeTiming(c.prf, c.dutyCycle, dummy)) {
        return AFE44xx_Status::InvalidArgument;
    }
    if (c.chip == AFE44xx_Chip::AFE4400 &&
        (c.separateGain || c.filter1kHz || c.ledRange == AFE44xx_LedRange::Half || c.txRef != AFE44xx_TxRef::V0_75)) {
        return AFE44xx_Status::Unsupported;
    }
    return AFE44xx_Status::Ok;
}

bool Protocentral_AFE44xx::begin(bool startSPI) {
    AFE44xx_Config config;
    return begin(config, startSPI);
}

bool Protocentral_AFE44xx::begin(const AFE44xx_Config& config, bool startSPI) {
    _initialized = false;
    _running     = false;
    _standby     = false;

    AFE44xx_Status st = validateConfig(config);
    if (st != AFE44xx_Status::Ok) {
        _lastError = st;
        return false;
    }
    _config = config;
    uint8_t maxAvg = maxAverages(_config.prf);
    if (_config.averages > maxAvg) _config.averages = maxAvg;
    _periodUs = 1000000UL / _config.prf;

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    if (_drdyPin >= 0) pinMode(_drdyPin, INPUT);
    if (_resetPin >= 0) {
        pinMode(_resetPin, OUTPUT);
        digitalWrite(_resetPin, HIGH);
    }

    if (startSPI) {
        _spi->begin();
    }

    if (_pdnPin >= 0) {
        // Power-cycle the AFE so it starts from a known state; RESET may follow 100 ms after PDN rises (t7)
        pinMode(_pdnPin, OUTPUT);
        digitalWrite(_pdnPin, LOW);
        delay(1);
        digitalWrite(_pdnPin, HIGH);
        delay(AFE44XX_POWERUP_MS);
    } else {
        uint32_t now = millis();
        if (now < AFE44XX_POWERUP_MS) delay(AFE44XX_POWERUP_MS - now);
    }

    hardwareReset();

    if (!isConnected()) {
        return false;  // _lastError set by isConnected()
    }
    if (!applyConfig()) {
        return false;  // _lastError set by updateRegister()
    }

    _beginMs     = millis();
    _sequence    = 0;
    _missed      = 0;
    _initialized = true;
    _lastError   = AFE44xx_Status::Ok;
    resetDataReady();

    if (_config.autoStart) {
        return start();
    }
    return true;
}

#if defined(ARDUINO_ARCH_ESP32)
bool Protocentral_AFE44xx::begin(int8_t sck, int8_t miso, int8_t mosi, const AFE44xx_Config& config) {
    _spi->begin(sck, miso, mosi, -1);
    return begin(config, false);
}
#endif

void Protocentral_AFE44xx::end() {
    disableDataReadyInterrupt();
    if (_initialized) {
        stop();
        standby();
    }
    _initialized = false;
    _lastError   = AFE44xx_Status::NotInitialized;
}

bool Protocentral_AFE44xx::isConnected() {
    uint32_t expected;
    uint32_t v;
    if (_initialized) {
        expected = registerValue(AFE44XX_REG_PRPCOUNT);
        v        = readRegister(AFE44XX_REG_PRPCOUNT);
    } else {
        // No ID register: write a pattern to a timing register (the timers are off) and read it back
        expected = PROBE_PATTERN;
        setSpiRead(false);
        rawWrite(AFE44XX_REG_LED2STC, PROBE_PATTERN);
        v = readRegister(AFE44XX_REG_LED2STC);
        setSpiRead(false);
        rawWrite(AFE44XX_REG_LED2STC, 0);
    }
    if (v == expected) return true;
    _lastError = (v == 0 || v == 0xFFFFFFUL) ? AFE44xx_Status::SpiError : AFE44xx_Status::VerifyFailed;
    return false;
}

void Protocentral_AFE44xx::hardwareReset() {
    if (_resetPin >= 0) {
        digitalWrite(_resetPin, LOW);
        delayMicroseconds(RESET_PULSE_US);
        digitalWrite(_resetPin, HIGH);
    } else {
        rawWrite(AFE44XX_REG_CONTROL0, AFE44XX_CONTROL0_SW_RST);
    }
    delayMicroseconds(RESET_WAIT_US);
    _spiRead = false;  // reset clears CONTROL0
}

bool Protocentral_AFE44xx::applyConfig() {
    return updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1)) &&
           updateRegister(AFE44XX_REG_CONTROL2, registerValue(AFE44XX_REG_CONTROL2)) &&
           updateRegister(AFE44XX_REG_LEDCNTRL, registerValue(AFE44XX_REG_LEDCNTRL)) &&
           updateRegister(AFE44XX_REG_TIA_AMB_GAIN, registerValue(AFE44XX_REG_TIA_AMB_GAIN)) &&
           updateRegister(AFE44XX_REG_TIAGAIN, registerValue(AFE44XX_REG_TIAGAIN)) &&
           writeTiming();
}

// ============================================================================
// Register values from the configuration
// ============================================================================

uint32_t Protocentral_AFE44xx::tiaBits(AFE44xx_TiaRf rf, AFE44xx_TiaCf cf, AFE44xx_Stage2Gain stage2) {
    uint32_t v = (static_cast<uint32_t>(cf) << 3) | static_cast<uint32_t>(rf);
    if (stage2 != AFE44xx_Stage2Gain::Off) {
        v |= 0x4000UL | ((uint32_t)(static_cast<uint8_t>(stage2) - 1) << 8);  // STAGE2EN, STG2GAIN
    }
    return v;
}

uint32_t Protocentral_AFE44xx::registerValue(uint8_t reg) const {
    const AFE44xx_Config& c = _config;
    bool afe4400 = c.chip == AFE44xx_Chip::AFE4400;
    uint32_t v = 0;

    switch (reg) {
        case AFE44XX_REG_CONTROL1:
            v = (uint32_t)(c.averages - 1);
            if (_running) v |= AFE44XX_CONTROL1_TIMEREN;
            break;
        case AFE44XX_REG_TIAGAIN:
            if (!afe4400 && c.separateGain) v = 0x8000UL | tiaBits(c.tiaRf1, c.tiaCf1, c.stage2Gain1);  // ENSEPGAIN
            break;
        case AFE44XX_REG_TIA_AMB_GAIN:
            v = ((uint32_t)c.ambientDac << 16) | tiaBits(c.tiaRf, c.tiaCf, c.stage2Gain);
            if (!afe4400 && c.filter1kHz) v |= 0x8000UL;  // FLTRCNRSEL
            break;
        case AFE44XX_REG_LEDCNTRL:
            v = ((uint32_t)c.led1Current << 8) | c.led2Current;
            if (afe4400) {
                if (c.ledRange == AFE44xx_LedRange::Off) v |= 0x20000UL;  // LEDCUROFF
            } else {
                v |= (uint32_t)static_cast<uint8_t>(c.ledRange) << 16;  // LED_RANGE
            }
            break;
        case AFE44XX_REG_CONTROL2:
            if (!afe4400) v = (uint32_t)static_cast<uint8_t>(c.txRef) << 17;
            if (c.ledPushPull) v |= AFE44XX_CONTROL2_TXBRGMOD;
            if (_standby) v |= AFE44XX_CONTROL2_PDN_AFE;
            break;
        case AFE44XX_REG_PRPCOUNT:
            v = TIMER_CLOCK_HZ / c.prf - 1;
            break;
        default:
            break;
    }
    return maskRegister(reg, v);
}

// Force the bits the datasheets say "must be 0/1", per chip, so callers cannot misconfigure them
uint32_t Protocentral_AFE44xx::maskRegister(uint8_t reg, uint32_t v) const {
    bool afe4400 = _config.chip == AFE44xx_Chip::AFE4400;
    v &= 0xFFFFFFUL;
    if (reg >= AFE44XX_REG_LED2STC && reg <= AFE44XX_REG_PRPCOUNT) return v & 0xFFFFUL;
    switch (reg) {
        case AFE44XX_REG_CONTROL1:     return v & 0x0FFFUL;
        case AFE44XX_REG_TIAGAIN:      return afe4400 ? 0 : (v & 0xC7FFUL);
        case AFE44XX_REG_TIA_AMB_GAIN: return v & (afe4400 ? 0xF47FFUL : 0xFC7FFUL);
        case AFE44XX_REG_LEDCNTRL:     return afe4400 ? ((v & 0x2FFFFUL) | 0x10000UL) : (v & 0x3FFFFUL);
        case AFE44XX_REG_CONTROL2:     return afe4400 ? ((v & 0x0E07UL) | 0x20100UL) : (v & 0x78F07UL);
        case AFE44XX_REG_ALARM:        return v & 0x80UL;
        default:                       return v;
    }
}

// ============================================================================
// Timing engine
// ============================================================================

uint8_t Protocentral_AFE44xx::maxAverages(uint16_t prf) {
    if (prf == 0) return 1;
    // Each conversion takes 50 us and all of them must fit in a quarter period (SBAS602H equation 5)
    uint32_t n = (250000UL / prf) / 50;
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    return (uint8_t)n;
}

bool Protocentral_AFE44xx::computeTiming(uint16_t prf, uint8_t dutyPercent, uint16_t out[AFE44XX_NUM_TIMING_REGS]) {
    if (prf < PRF_MIN || prf > PRF_MAX || dutyPercent < 1 || dutyPercent > 25) return false;

    uint32_t period = TIMER_CLOCK_HZ / prf;  // timer counts per period
    uint16_t prp    = (uint16_t)(period - 1);
    uint16_t q      = (uint16_t)(period / 4);
    uint16_t w      = (uint16_t)(period * dutyPercent / 100);  // LED on-time
    if (w > q) w = q;
    if (w < MIN_PULSE) return false;

    // Quarter k spans [s[k], e[k]]; the last one absorbs the rounding
    uint16_t s[4], e[4];
    for (uint8_t k = 0; k < 4; k++) {
        s[k] = k * q;
        e[k] = (k == 3) ? prp : (uint16_t)(s[k] + q - 1);
    }

    // SBAS602H Table 2 order. Conversions run LED2 -> ALED2 -> LED1 -> ALED1 from t0, so the
    // samples run one quarter ahead: LED2 in quarter 3, ALED2 in 0, LED1 in 1, ALED1 in 2.
    // Each LED pulse and sample window ends with its quarter; sampling stops one count early.
    // Each conversion starts two counts after a one-count ADC reset at the quarter start.
    uint16_t* t = out;
    *t++ = e[3] - w + 1;  // LED2STC
    *t++ = e[3] - 1;      // LED2ENDC
    *t++ = e[3] - w + 1;  // LED2LEDSTC
    *t++ = e[3];          // LED2LEDENDC
    *t++ = e[0] - w + 1;  // ALED2STC
    *t++ = e[0] - 1;      // ALED2ENDC
    *t++ = e[1] - w + 1;  // LED1STC
    *t++ = e[1] - 1;      // LED1ENDC
    *t++ = e[1] - w + 1;  // LED1LEDSTC
    *t++ = e[1];          // LED1LEDENDC
    *t++ = e[2] - w + 1;  // ALED1STC
    *t++ = e[2] - 1;      // ALED1ENDC
    *t++ = s[0] + 2;      // LED2CONVST
    *t++ = e[0];          // LED2CONVEND
    *t++ = s[1] + 2;      // ALED2CONVST
    *t++ = e[1];          // ALED2CONVEND
    *t++ = s[2] + 2;      // LED1CONVST
    *t++ = e[2];          // LED1CONVEND
    *t++ = s[3] + 2;      // ALED1CONVST
    *t++ = e[3];          // ALED1CONVEND
    for (uint8_t k = 0; k < 4; k++) {
        *t++ = s[k];      // ADCRSTSTCTk
        *t++ = s[k];      // ADCRSTENDCTk
    }
    *t = prp;             // PRPCOUNT
    return true;
}

bool Protocentral_AFE44xx::writeTiming() {
    uint16_t t[AFE44XX_NUM_TIMING_REGS];
    if (!computeTiming(_config.prf, _config.dutyCycle, t)) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    for (uint8_t i = 0; i < AFE44XX_NUM_TIMING_REGS; i++) {
        if (!updateRegister(AFE44XX_REG_LED2STC + i, t[i])) return false;
    }
    _periodUs = 1000000UL / _config.prf;
    return true;
}

// ============================================================================
// Timing engine control and power
// ============================================================================

bool Protocentral_AFE44xx::start() {
    if (!_initialized) {
        _lastError = AFE44xx_Status::NotInitialized;
        return false;
    }
    _running = true;
    if (!updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1))) {
        _running = false;
        return false;
    }
    resetDataReady();
    return true;
}

bool Protocentral_AFE44xx::stop() {
    if (!_initialized) {
        _lastError = AFE44xx_Status::NotInitialized;
        return false;
    }
    _running = false;
    return updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1));
}

bool Protocentral_AFE44xx::standby() {
    _standby = true;
    return updateRegister(AFE44XX_REG_CONTROL2, registerValue(AFE44XX_REG_CONTROL2));
}

bool Protocentral_AFE44xx::wakeup() {
    _standby = false;
    if (!updateRegister(AFE44XX_REG_CONTROL2, registerValue(AFE44XX_REG_CONTROL2))) return false;
    resetDataReady();
    return true;
}

bool Protocentral_AFE44xx::setPRF(uint16_t prf) {
    AFE44xx_Config c = _config;
    c.prf = prf;
    uint8_t maxAvg = maxAverages(prf);
    if (c.averages > maxAvg) c.averages = maxAvg;
    if (validateConfig(c) != AFE44xx_Status::Ok) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }

    // Hold the timers off while the 29 timing registers change
    bool wasRunning = _running;
    _running = false;
    _config  = c;
    bool ok = updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1)) && writeTiming();
    _running = wasRunning;
    ok = updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1)) && ok;
    resetDataReady();
    return ok;
}

bool Protocentral_AFE44xx::setDutyCycle(uint8_t percent) {
    AFE44xx_Config c = _config;
    c.dutyCycle = percent;
    if (validateConfig(c) != AFE44xx_Status::Ok) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    bool wasRunning = _running;
    _running = false;
    _config  = c;
    bool ok = updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1)) && writeTiming();
    _running = wasRunning;
    return updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1)) && ok;
}

bool Protocentral_AFE44xx::setAverages(uint8_t averages) {
    if (averages < 1 || averages > maxAverages(_config.prf)) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    _config.averages = averages;
    return updateRegister(AFE44XX_REG_CONTROL1, registerValue(AFE44XX_REG_CONTROL1));
}

// ============================================================================
// Transmitter and receiver
// ============================================================================

bool Protocentral_AFE44xx::setLedCurrent(AFE44xx_Led led, uint8_t code) {
    if (led == AFE44xx_Led::LED1) {
        _config.led1Current = code;
    } else {
        _config.led2Current = code;
    }
    return updateRegister(AFE44XX_REG_LEDCNTRL, registerValue(AFE44XX_REG_LEDCNTRL));
}

uint8_t Protocentral_AFE44xx::getLedCurrent(AFE44xx_Led led) const {
    return led == AFE44xx_Led::LED1 ? _config.led1Current : _config.led2Current;
}

bool Protocentral_AFE44xx::setLedRange(AFE44xx_LedRange range) {
    AFE44xx_Config c = _config;
    c.ledRange = range;
    AFE44xx_Status st = validateConfig(c);
    if (st != AFE44xx_Status::Ok) {
        _lastError = st;
        return false;
    }
    _config = c;
    return updateRegister(AFE44XX_REG_LEDCNTRL, registerValue(AFE44XX_REG_LEDCNTRL));
}

bool Protocentral_AFE44xx::setTxReference(AFE44xx_TxRef ref) {
    AFE44xx_Config c = _config;
    c.txRef = ref;
    AFE44xx_Status st = validateConfig(c);
    if (st != AFE44xx_Status::Ok) {
        _lastError = st;
        return false;
    }
    _config = c;
    return updateRegister(AFE44XX_REG_CONTROL2, registerValue(AFE44XX_REG_CONTROL2));
}

uint16_t Protocentral_AFE44xx::ledFullScaleMilliamps() const {
    if (_config.ledRange == AFE44xx_LedRange::Off) return 0;
    if (_config.chip == AFE44xx_Chip::AFE4400) return 50;
    static const uint16_t fullScale[] = {150, 100, 200};  // TX_REF 0.75 / 0.5 / 1.0 V
    uint16_t ma = fullScale[static_cast<uint8_t>(_config.txRef)];
    return _config.ledRange == AFE44xx_LedRange::Half ? ma / 2 : ma;
}

bool Protocentral_AFE44xx::setReceiverGain(AFE44xx_TiaRf rf, AFE44xx_TiaCf cf, AFE44xx_Stage2Gain stage2) {
    if (static_cast<uint8_t>(rf) > 7 || static_cast<uint8_t>(cf) > 0x1F ||
        static_cast<uint8_t>(stage2) > static_cast<uint8_t>(AFE44xx_Stage2Gain::X4)) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    _config.tiaRf      = rf;
    _config.tiaCf      = cf;
    _config.stage2Gain = stage2;
    return updateRegister(AFE44XX_REG_TIA_AMB_GAIN, registerValue(AFE44XX_REG_TIA_AMB_GAIN));
}

bool Protocentral_AFE44xx::setLed1ReceiverGain(bool enable, AFE44xx_TiaRf rf, AFE44xx_TiaCf cf,
                                               AFE44xx_Stage2Gain stage2) {
    if (_config.chip == AFE44xx_Chip::AFE4400) {
        _lastError = AFE44xx_Status::Unsupported;
        return false;
    }
    if (static_cast<uint8_t>(rf) > 7 || static_cast<uint8_t>(cf) > 0x1F ||
        static_cast<uint8_t>(stage2) > static_cast<uint8_t>(AFE44xx_Stage2Gain::X4)) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    _config.separateGain = enable;
    _config.tiaRf1       = rf;
    _config.tiaCf1       = cf;
    _config.stage2Gain1  = stage2;
    return updateRegister(AFE44XX_REG_TIAGAIN, registerValue(AFE44XX_REG_TIAGAIN));
}

bool Protocentral_AFE44xx::setAmbientDac(uint8_t microamps) {
    if (microamps > AMBIENT_DAC_MAX) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    _config.ambientDac = microamps;
    return updateRegister(AFE44XX_REG_TIA_AMB_GAIN, registerValue(AFE44XX_REG_TIA_AMB_GAIN));
}

bool Protocentral_AFE44xx::setFilterCorner1kHz(bool enable) {
    if (_config.chip == AFE44xx_Chip::AFE4400) {
        _lastError = AFE44xx_Status::Unsupported;
        return false;
    }
    _config.filter1kHz = enable;
    return updateRegister(AFE44XX_REG_TIA_AMB_GAIN, registerValue(AFE44XX_REG_TIA_AMB_GAIN));
}

// ============================================================================
// Reading data
// ============================================================================

uint32_t Protocentral_AFE44xx::irqCount() const {
    noInterrupts();
    uint32_t c = _irqCount;
    interrupts();
    return c;
}

void Protocentral_AFE44xx::resetDataReady() {
    uint32_t c = irqCount();
    uint32_t now = micros();
    _lastIrqCount = c;
    _watchCount   = c;
    _watchUs      = now;
    _nextPollUs   = now + _periodUs;
    _drdyFault    = false;
    _drdyFlag     = false;
}

bool Protocentral_AFE44xx::available() {
    if (!_initialized || !_running || _standby) return false;
    uint32_t now = micros();

    if (_irqSlot >= 0 && !_drdyFault) {
        if (_drdyFlag) return true;
        // ADC_RDY fires every period. If it stops, fall back to timed reads
        uint32_t c = irqCount();
        if (c != _watchCount) {
            _watchCount = c;
            _watchUs    = now;
            return false;
        }
        if (now - _watchUs <= 8 * _periodUs + 20000UL) return false;
        _drdyFault  = true;
        _nextPollUs = now;
    }
    return (int32_t)(now - _nextPollUs) >= 0;
}

bool Protocentral_AFE44xx::read(AFE44xx_Sample& sample) {
    if (!_initialized) {
        _lastError = AFE44xx_Status::NotInitialized;
        return false;
    }

    if (_irqSlot >= 0 && !_drdyFault) {
        noInterrupts();
        uint32_t c = _irqCount;
        _drdyFlag  = false;
        interrupts();
        uint32_t n = c - _lastIrqCount;
        if (n > 1) {
            _missed += n - 1;
            _sequence += n - 1;
        }
        _lastIrqCount = c;
        _watchCount   = c;
        _watchUs      = micros();
    } else {
        uint32_t now = micros();
        if ((int32_t)(now - _nextPollUs) >= 0) {
            uint32_t late = (now - _nextPollUs) / _periodUs;
            _missed += late;
            _sequence += late;
            _nextPollUs += (late + 1) * _periodUs;
        }
    }
    return readLatest(sample);
}

bool Protocentral_AFE44xx::readLatest(AFE44xx_Sample& sample) {
    if (!_initialized) {
        _lastError = AFE44xx_Status::NotInitialized;
        return false;
    }
    bool ok = readResults(sample);
    sample.sequence = _sequence++;
    return ok;
}

static int32_t signExtend22(uint32_t v) {
    return (int32_t)(v << 10) >> 10;  // 22-bit two's complement; bits 23:22 are ignored
}

bool Protocentral_AFE44xx::readResults(AFE44xx_Sample& sample) {
    setSpiRead(true);  // stays set between samples, so a sample costs six register reads
    sample.led2           = signExtend22(rawRead(AFE44XX_REG_LED2VAL));
    sample.aled2          = signExtend22(rawRead(AFE44XX_REG_ALED2VAL));
    sample.led1           = signExtend22(rawRead(AFE44XX_REG_LED1VAL));
    sample.aled1          = signExtend22(rawRead(AFE44XX_REG_ALED1VAL));
    sample.led2MinusAled2 = signExtend22(rawRead(AFE44XX_REG_LED2_ALED2VAL));
    sample.led1MinusAled1 = signExtend22(rawRead(AFE44XX_REG_LED1_ALED1VAL));
    return true;
}

void AFE44XX_ISR_ATTR Protocentral_AFE44xx::isr0() {
    if (_irqInstances[0]) _irqInstances[0]->handleDataReady();
}

void AFE44XX_ISR_ATTR Protocentral_AFE44xx::isr1() {
    if (_irqInstances[1]) _irqInstances[1]->handleDataReady();
}

void AFE44XX_ISR_ATTR Protocentral_AFE44xx::handleDataReady() {
    // No SPI here: read() does it outside the interrupt
    _irqCount = _irqCount + 1;
    _drdyFlag = true;
}

bool Protocentral_AFE44xx::enableDataReadyInterrupt() {
    if (_irqSlot >= 0) return true;
    if (_drdyPin < 0) {
        _lastError = AFE44xx_Status::NotInterruptPin;
        return false;
    }

    int irq = digitalPinToInterrupt(_drdyPin);
#ifdef NOT_AN_INTERRUPT
    if (irq == NOT_AN_INTERRUPT) {
        _lastError = AFE44xx_Status::NotInterruptPin;
        return false;
    }
#endif
    if (irq < 0) {
        _lastError = AFE44xx_Status::NotInterruptPin;
        return false;
    }

    int8_t slot = -1;
    for (uint8_t i = 0; i < AFE44XX_MAX_IRQ_INSTANCES; i++) {
        if (_irqInstances[i] == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }

    _irqInstances[slot] = this;
    _irqSlot = slot;
    resetDataReady();
    attachInterrupt(irq, slot == 0 ? isr0 : isr1, RISING);  // ADC_RDY is an active-high pulse
    return true;
}

void Protocentral_AFE44xx::disableDataReadyInterrupt() {
    if (_irqSlot < 0) return;
    detachInterrupt(digitalPinToInterrupt(_drdyPin));
    _irqInstances[_irqSlot] = nullptr;
    _irqSlot = -1;
    resetDataReady();
}

// ============================================================================
// Diagnostics
// ============================================================================

bool Protocentral_AFE44xx::runDiagnostics(AFE44xx_Diagnostics& result) {
    result.flags = 0;
    if (!_initialized) {
        _lastError = AFE44xx_Status::NotInitialized;
        return false;
    }
    if (_standby) {
        _lastError = AFE44xx_Status::InvalidArgument;  // the diagnostics need every block powered
        return false;
    }

    setSpiRead(false);
    rawWrite(AFE44XX_REG_CONTROL0, AFE44XX_CONTROL0_DIAG_EN);
    delay(DIAG_WAIT_MS);
    result.flags = (uint16_t)(readRegister(AFE44XX_REG_DIAG) & 0x1FFFUL);
    delay(DIAG_SETTLE_MS);

    // Samples converted meanwhile are invalid: drop them without counting them as missed
    resetDataReady();
    return true;
}

// ============================================================================
// Register access
// ============================================================================

uint32_t Protocentral_AFE44xx::readRegister(uint8_t reg) {
    if (reg == AFE44XX_REG_CONTROL0 || reg > AFE44XX_REG_DIAG) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return 0;
    }
    setSpiRead(true);
    return rawRead(reg);
}

bool Protocentral_AFE44xx::writeRegister(uint8_t reg, uint32_t value) {
    if (reg == AFE44XX_REG_CONTROL0 || reg >= AFE44XX_REG_LED2VAL || reg == 0x1F ||
        (reg >= 0x24 && reg <= 0x28)) {
        _lastError = AFE44xx_Status::InvalidArgument;
        return false;
    }
    // Raw writes bypass the configuration shadow: setters called later rewrite these registers
    return updateRegister(reg, value);
}

bool Protocentral_AFE44xx::updateRegister(uint8_t reg, uint32_t value) {
    value = maskRegister(reg, value);
    setSpiRead(false);
    rawWrite(reg, value);
    uint32_t readBack = readRegister(reg);
    if (readBack != value) {
        _lastError = (readBack == 0xFFFFFFUL) ? AFE44xx_Status::SpiError : AFE44xx_Status::VerifyFailed;
        return false;
    }
    return true;
}

void Protocentral_AFE44xx::setSpiRead(bool enable) {
    if (_spiRead == enable) return;
    rawWrite(AFE44XX_REG_CONTROL0, enable ? AFE44XX_CONTROL0_SPI_READ : 0);
    _spiRead = enable;
}

static const __FlashStringHelper* registerName(uint8_t reg) {
    switch (reg) {
        case AFE44XX_REG_PRPCOUNT:      return F("PRPCOUNT");
        case AFE44XX_REG_CONTROL1:      return F("CONTROL1");
        case AFE44XX_REG_TIAGAIN:       return F("TIAGAIN");
        case AFE44XX_REG_TIA_AMB_GAIN:  return F("TIA_AMB_GAIN");
        case AFE44XX_REG_LEDCNTRL:      return F("LEDCNTRL");
        case AFE44XX_REG_CONTROL2:      return F("CONTROL2");
        case AFE44XX_REG_ALARM:         return F("ALARM");
        case AFE44XX_REG_LED2VAL:       return F("LED2VAL");
        case AFE44XX_REG_ALED2VAL:      return F("ALED2VAL");
        case AFE44XX_REG_LED1VAL:       return F("LED1VAL");
        case AFE44XX_REG_ALED1VAL:      return F("ALED1VAL");
        case AFE44XX_REG_LED2_ALED2VAL: return F("LED2-ALED2VAL");
        case AFE44XX_REG_LED1_ALED1VAL: return F("LED1-ALED1VAL");
        case AFE44XX_REG_DIAG:          return F("DIAG");
        default:                        return F("timing");
    }
}

void Protocentral_AFE44xx::printRegisters(Stream& out) {
    for (uint8_t reg = AFE44XX_REG_LED2STC; reg <= AFE44XX_REG_DIAG; reg++) {
        if (reg == 0x1F || (reg >= 0x24 && reg <= 0x28)) continue;  // spare and factory registers
        uint32_t v = readRegister(reg);
        out.print(F("0x"));
        if (reg < 0x10) out.print('0');
        out.print(reg, HEX);
        out.print(F("  "));
        out.print(registerName(reg));
        out.print(F("\t0x"));
        for (uint32_t m = 0x100000UL; m > 1 && v < m; m >>= 4) out.print('0');
        out.println(v, HEX);
    }
}

// ============================================================================
// Low-level SPI
// ============================================================================

void Protocentral_AFE44xx::rawWrite(uint8_t reg, uint32_t value) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg);
    _spi->transfer((uint8_t)(value >> 16));
    _spi->transfer((uint8_t)(value >> 8));
    _spi->transfer((uint8_t)value);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

uint32_t Protocentral_AFE44xx::rawRead(uint8_t reg) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg);
    uint32_t v = (uint32_t)_spi->transfer(0) << 16;
    v |= (uint32_t)_spi->transfer(0) << 8;
    v |= _spi->transfer(0);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    return v;
}
