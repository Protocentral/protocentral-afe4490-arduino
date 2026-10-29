// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI AFE4490 and AFE4400 pulse-oximeter analog front ends,
// as used on the ProtoCentral AFE4490 Breakout and Shield and on Sensything OX (AFE4400).
//
// The header keeps its 1.x name so existing sketches and SensythingCore keep building.
// It declares the 2.0 driver (Protocentral_AFE44xx) and the 1.x compatibility class (AFE44XX).
//
// For research and education only - not a medical device.
//
// https://github.com/Protocentral/protocentral-afe4490-arduino

#ifndef PROTOCENTRAL_AFE44XX_H
#define PROTOCENTRAL_AFE44XX_H

#include <Arduino.h>
#include <SPI.h>

#include "Protocentral_AFE44xx_Algorithms.h"

#define AFE44XX_LIBRARY_VERSION "2.0.0-dev"

// ============================================================================
// Compile-time configuration (override with -D or #define before #include)
// ============================================================================

#ifndef AFE44XX_SPI_CLOCK
#define AFE44XX_SPI_CLOCK 2000000UL  ///< SPI clock in Hz (the 1.x value, proven on the breakout's level shifter)
#endif

#ifndef AFE44XX_POWERUP_MS
#define AFE44XX_POWERUP_MS 100       ///< Minimum ms from supplies stable (or AFE_PDN high) to RESET release and SPI (t2/t7)
#endif

#ifndef AFE44XX_SETTLE_MS
#define AFE44XX_SETTLE_MS 1000       ///< ms after begin() until the signal chain gives high-accuracy data (t6, 2.2 uF on BG/TX_REF)
#endif

#define AFE44XX_MAX_IRQ_INSTANCES 2  ///< Driver instances that may use DRDY interrupts at the same time (one ISR trampoline each)

// ============================================================================
// Register map (AFE4490 SBAS602H / AFE4400 SBAS601H, section 8.6)
// ============================================================================

#define AFE44XX_REG_CONTROL0      0x00  ///< Write-only: SW_RST, DIAG_EN, TIM_CNT_RST, SPI_READ
#define AFE44XX_REG_LED2STC       0x01
#define AFE44XX_REG_LED2ENDC      0x02
#define AFE44XX_REG_LED2LEDSTC    0x03
#define AFE44XX_REG_LED2LEDENDC   0x04
#define AFE44XX_REG_ALED2STC      0x05
#define AFE44XX_REG_ALED2ENDC     0x06
#define AFE44XX_REG_LED1STC       0x07
#define AFE44XX_REG_LED1ENDC      0x08
#define AFE44XX_REG_LED1LEDSTC    0x09
#define AFE44XX_REG_LED1LEDENDC   0x0A
#define AFE44XX_REG_ALED1STC      0x0B
#define AFE44XX_REG_ALED1ENDC     0x0C
#define AFE44XX_REG_LED2CONVST    0x0D
#define AFE44XX_REG_LED2CONVEND   0x0E
#define AFE44XX_REG_ALED2CONVST   0x0F
#define AFE44XX_REG_ALED2CONVEND  0x10
#define AFE44XX_REG_LED1CONVST    0x11
#define AFE44XX_REG_LED1CONVEND   0x12
#define AFE44XX_REG_ALED1CONVST   0x13
#define AFE44XX_REG_ALED1CONVEND  0x14
#define AFE44XX_REG_ADCRSTSTCT0   0x15
#define AFE44XX_REG_ADCRSTENDCT0  0x16
#define AFE44XX_REG_ADCRSTSTCT1   0x17
#define AFE44XX_REG_ADCRSTENDCT1  0x18
#define AFE44XX_REG_ADCRSTSTCT2   0x19
#define AFE44XX_REG_ADCRSTENDCT2  0x1A
#define AFE44XX_REG_ADCRSTSTCT3   0x1B
#define AFE44XX_REG_ADCRSTENDCT3  0x1C
#define AFE44XX_REG_PRPCOUNT      0x1D
#define AFE44XX_REG_CONTROL1      0x1E
#define AFE44XX_REG_TIAGAIN       0x20  ///< AFE4490 only (reserved, must be 0 on the AFE4400)
#define AFE44XX_REG_TIA_AMB_GAIN  0x21
#define AFE44XX_REG_LEDCNTRL      0x22
#define AFE44XX_REG_CONTROL2      0x23
#define AFE44XX_REG_ALARM         0x29
#define AFE44XX_REG_LED2VAL       0x2A
#define AFE44XX_REG_ALED2VAL      0x2B
#define AFE44XX_REG_LED1VAL       0x2C
#define AFE44XX_REG_ALED1VAL      0x2D
#define AFE44XX_REG_LED2_ALED2VAL 0x2E
#define AFE44XX_REG_LED1_ALED1VAL 0x2F
#define AFE44XX_REG_DIAG          0x30
#define AFE44XX_NUM_TIMING_REGS   29    ///< LED2STC (0x01) .. PRPCOUNT (0x1D)

// CONTROL0
#define AFE44XX_CONTROL0_SW_RST      0x000008
#define AFE44XX_CONTROL0_DIAG_EN     0x000004
#define AFE44XX_CONTROL0_TIM_CNT_RST 0x000002
#define AFE44XX_CONTROL0_SPI_READ    0x000001

// CONTROL1
#define AFE44XX_CONTROL1_TIMEREN     0x000100

// CONTROL2
#define AFE44XX_CONTROL2_TXBRGMOD    0x000800  ///< LED driver push-pull instead of H-bridge
#define AFE44XX_CONTROL2_PDN_TX      0x000004
#define AFE44XX_CONTROL2_PDN_RX      0x000002
#define AFE44XX_CONTROL2_PDN_AFE     0x000001

// DIAG flags (AFE44xx_Diagnostics::flags)
#define AFE44XX_DIAG_INPSCLED   0x0001  ///< PD INP shorted to LED cable
#define AFE44XX_DIAG_INNSCLED   0x0002  ///< PD INN shorted to LED cable
#define AFE44XX_DIAG_INPSCGND   0x0004  ///< PD INP shorted to GND
#define AFE44XX_DIAG_INNSCGND   0x0008  ///< PD INN shorted to GND
#define AFE44XX_DIAG_PDSC       0x0010  ///< Photodiode short
#define AFE44XX_DIAG_PDOC       0x0020  ///< Photodiode open
#define AFE44XX_DIAG_OUTNSHGND  0x0040  ///< LED OUTN shorted to GND
#define AFE44XX_DIAG_OUTPSHGND  0x0080  ///< LED OUTP shorted to GND
#define AFE44XX_DIAG_LEDSC      0x0100  ///< LED short
#define AFE44XX_DIAG_LED2OPEN   0x0200  ///< LED2 open
#define AFE44XX_DIAG_LED1OPEN   0x0400  ///< LED1 open
#define AFE44XX_DIAG_LED_ALM    0x0800  ///< Any transmitter (LED-side) fault
#define AFE44XX_DIAG_PD_ALM     0x1000  ///< Any photodiode-side fault

#define AFE44XX_ADC_FULL_SCALE  2097151L  ///< Largest positive 22-bit code

// ============================================================================
// Enumerations (values are the register bit patterns unless noted)
// ============================================================================

/// Which chip is on the board. They share a register map but not every bit,
/// and neither has an ID register, so the driver has to be told.
enum class AFE44xx_Chip : uint8_t {
    AFE4490 = 0,  ///< ProtoCentral AFE4490 Breakout and Shield
    AFE4400 = 1   ///< Sensything OX
};

/// Error codes reported by lastError().
enum class AFE44xx_Status : uint8_t {
    Ok = 0,           ///< No error
    NotInitialized,   ///< begin() has not succeeded yet
    SpiError,         ///< No response from the chip (register read back as 0x000000 or 0xFFFFFF)
    VerifyFailed,     ///< Register read-back did not match what was written
    InvalidArgument,  ///< Out-of-range parameter
    NotInterruptPin,  ///< DRDY pin cannot be used with attachInterrupt() on this board
    Timeout,          ///< Diagnostics did not finish in time
    Unsupported       ///< Setting does not exist on this chip (e.g. separate gain on the AFE4400)
};

/// LED selector. On ProtoCentral probes LED1 is infrared and LED2 is red.
enum class AFE44xx_Led : uint8_t {
    LED1 = 0,
    LED2 = 1,
    IR   = 0,  ///< Alias for LED1
    Red  = 1   ///< Alias for LED2
};

/// Transimpedance amplifier feedback resistor (RF).
enum class AFE44xx_TiaRf : uint8_t {
    RF_500K = 0,
    RF_250K = 1,
    RF_100K = 2,
    RF_50K  = 3,
    RF_25K  = 4,
    RF_10K  = 5,
    RF_1M   = 6,
    None    = 7
};

/// Transimpedance amplifier feedback capacitor (CF). Any 5-bit combination is
/// valid: 5 pF + 5 (bit 0) + 15 (bit 1) + 25 (bit 2) + 50 (bit 3) + 150 (bit 4).
enum class AFE44xx_TiaCf : uint8_t {
    pF_5   = 0x00,
    pF_10  = 0x01,
    pF_20  = 0x02,
    pF_25  = 0x03,
    pF_30  = 0x04,
    pF_35  = 0x05,
    pF_45  = 0x06,
    pF_50  = 0x07,
    pF_55  = 0x08,
    pF_80  = 0x0C,
    pF_100 = 0x0F,
    pF_155 = 0x10,
    pF_250 = 0x1F
};

/// Second-stage gain after ambient cancellation (not a register pattern: Off clears STAGE2EN).
enum class AFE44xx_Stage2Gain : uint8_t {
    Off  = 0,  ///< Stage 2 bypassed
    X1   = 1,
    X1_5 = 2,
    X2   = 3,
    X3   = 4,
    X4   = 5
};

/// LED current range (AFE4490 LED_RANGE). The AFE4400 is fixed at 50 mA full scale and only supports Full and Off.
enum class AFE44xx_LedRange : uint8_t {
    Full = 0,  ///< 150 / 100 / 200 mA at TX_REF 0.75 / 0.5 / 1.0 V
    Half = 1,  ///< 75 / 50 / 100 mA
    Off  = 3   ///< Transmitter off
};

/// Transmitter reference voltage (AFE4490 only). Use 0.5 V for a 3 V LED supply, 0.75 or 1.0 V for 5 V.
enum class AFE44xx_TxRef : uint8_t {
    V0_75 = 0,
    V0_5  = 1,
    V1_0  = 2
};

// ============================================================================
// Data types
// ============================================================================

/// One pulse-repetition period: all six result registers, as signed 22-bit counts.
/// On ProtoCentral probes LED1 is infrared and LED2 is red.
struct AFE44xx_Sample {
    int32_t led2;            ///< LED2 (red)
    int32_t aled2;           ///< Ambient sampled in the LED2 slot
    int32_t led1;            ///< LED1 (IR)
    int32_t aled1;           ///< Ambient sampled in the LED1 slot
    int32_t led2MinusAled2;  ///< LED2 - ambient, subtracted on chip
    int32_t led1MinusAled1;  ///< LED1 - ambient, subtracted on chip
    uint32_t sequence;       ///< Sample counter, including missed samples

    int32_t ir() const { return led1MinusAled1; }   ///< Ambient-subtracted IR
    int32_t red() const { return led2MinusAled2; }  ///< Ambient-subtracted red
};

/// Result of runDiagnostics().
struct AFE44xx_Diagnostics {
    uint16_t flags;  ///< AFE44XX_DIAG_* bits; 0 = no fault
    bool ok() const { return flags == 0; }
    bool ledFault() const { return flags & (AFE44XX_DIAG_LED_ALM | 0x07C0); }
    bool photodiodeFault() const { return flags & (AFE44XX_DIAG_PD_ALM | 0x003F); }
};

/// Full device configuration applied by begin(). Defaults reproduce the 1.x
/// profile: 500 Hz PRF, 25 % duty, 8 averages, both LEDs at DAC code 20,
/// TIA 250 kOhm / 5 pF, stage 2 off, no ambient DAC.
struct AFE44xx_Config {
    AFE44xx_Chip chip = AFE44xx_Chip::AFE4490;

    uint16_t prf       = 500;  ///< Pulse repetition frequency = sample rate, 63-5000 Hz
    uint8_t  dutyCycle = 25;   ///< LED on-time per phase, 1-25 % of the period
    uint8_t  averages  = 8;    ///< ADC conversions averaged per phase, 1-16. Reduced to maxAverages(prf) if larger

    uint8_t led1Current = 20;  ///< LED1 (IR) DAC code 0-255: current = code / 256 x full scale
    uint8_t led2Current = 20;  ///< LED2 (red) DAC code 0-255
    AFE44xx_LedRange ledRange = AFE44xx_LedRange::Full;
    AFE44xx_TxRef    txRef    = AFE44xx_TxRef::V0_75;  ///< AFE4490 only
    bool ledPushPull = false;  ///< Push-pull drive for 3-pin common-anode LEDs; false = H-bridge (back-to-back probes)

    AFE44xx_TiaRf      tiaRf      = AFE44xx_TiaRf::RF_250K;  ///< Both LEDs, or LED2 only with separateGain
    AFE44xx_TiaCf      tiaCf      = AFE44xx_TiaCf::pF_5;
    AFE44xx_Stage2Gain stage2Gain = AFE44xx_Stage2Gain::Off;

    bool               separateGain = false;  ///< AFE4490 only: LED1 uses the three settings below
    AFE44xx_TiaRf      tiaRf1       = AFE44xx_TiaRf::RF_250K;
    AFE44xx_TiaCf      tiaCf1       = AFE44xx_TiaCf::pF_5;
    AFE44xx_Stage2Gain stage2Gain1  = AFE44xx_Stage2Gain::Off;

    uint8_t ambientDac = 0;     ///< Ambient cancellation current, 0-10 uA
    bool    filter1kHz = false; ///< AFE4490 only: 1 kHz receiver filter corner instead of 500 Hz

    bool autoStart = true;      ///< Start the timing engine at the end of begin()
};

// ============================================================================
// Driver class
// ============================================================================

class Protocentral_AFE44xx {
public:
    /**
     * @param csPin     Chip select
     * @param drdyPin   ADC_RDY (active-high pulse), or -1 to read on a timer once per period
     * @param pdnPin    AFE_PDN (PWDN on the breakout), or -1 if not wired. Held HIGH by the driver
     * @param resetPin  RESET (labelled START on Breakout v2), or -1 to use the software reset
     * @param spi       SPI bus
     */
    Protocentral_AFE44xx(uint8_t csPin, int8_t drdyPin = -1, int8_t pdnPin = -1, int8_t resetPin = -1,
                         SPIClass& spi = SPI);

    // ---- Initialisation ----------------------------------------------------

    /** @brief Reset the chip, check SPI read-back and apply the default (1.x) profile. */
    bool begin(bool startSPI = true);

    /** @brief Reset the chip, check SPI read-back and apply @p config. */
    bool begin(const AFE44xx_Config& config, bool startSPI = true);

#if defined(ARDUINO_ARCH_ESP32)
    /** @brief ESP32 only: start SPI on custom pins, then begin(config). */
    bool begin(int8_t sck, int8_t miso, int8_t mosi, const AFE44xx_Config& config = AFE44xx_Config());
#endif

    /** @brief Stop the timing engine, power the AFE down and release the DRDY interrupt. */
    void end();

    /**
     * @brief True when an AFE44xx answers on SPI. There is no ID register: before begin()
     * this writes and reads back a timing register; afterwards it checks PRPCOUNT.
     */
    bool isConnected();
    AFE44xx_Status lastError() const { return _lastError; }
    const AFE44xx_Config& config() const { return _config; }
    static const char* getLibraryVersion() { return AFE44XX_LIBRARY_VERSION; }

    /** @brief True once AFE44XX_SETTLE_MS have passed since begin() (high-accuracy data). */
    bool isSettled() const { return _initialized && millis() - _beginMs >= AFE44XX_SETTLE_MS; }

    // ---- Timing engine and power -------------------------------------------

    bool start();      ///< Enable the timing engine (conversions)
    bool stop();       ///< Disable the timing engine
    bool standby();    ///< Power the AFE down (PDN_AFE); registers kept
    bool wakeup();     ///< Leave standby. Allow AFE44XX_SETTLE_MS for accurate data
    bool isRunning() const { return _running; }

    /** @brief Set the pulse repetition frequency (= sample rate). Reduces averages if the new PRF needs it. */
    bool setPRF(uint16_t prf);
    uint16_t getSampleRate() const { return _config.prf; }  ///< Samples per second (= PRF)
    bool setDutyCycle(uint8_t percent);
    bool setAverages(uint8_t averages);
    uint8_t getAverages() const { return _config.averages; }

    /** @brief Most ADC averages that fit in 25 % of the period at @p prf (1-16). */
    static uint8_t maxAverages(uint16_t prf);

    /**
     * @brief Compute the 29 timing registers (LED2STC .. PRPCOUNT, in address order) for @p prf and
     * @p dutyPercent. At 500 Hz / 25 % this reproduces the 1.x register values exactly.
     */
    static bool computeTiming(uint16_t prf, uint8_t dutyPercent, uint16_t out[AFE44XX_NUM_TIMING_REGS]);

    // ---- Transmitter -------------------------------------------------------

    /** @brief LED drive as an 8-bit DAC code: current = code / 256 x ledFullScaleMilliamps(). */
    bool setLedCurrent(AFE44xx_Led led, uint8_t code);
    uint8_t getLedCurrent(AFE44xx_Led led) const;
    bool setLedRange(AFE44xx_LedRange range);
    bool setTxReference(AFE44xx_TxRef ref);  ///< AFE4490 only. Allow ~1 s to settle after a change
    uint16_t ledFullScaleMilliamps() const;  ///< Current for DAC code 256 at the present chip / range / TX_REF

    // ---- Receiver ----------------------------------------------------------

    /** @brief TIA feedback and stage 2 gain for both LEDs (or LED2 only when separate gain is on). */
    bool setReceiverGain(AFE44xx_TiaRf rf, AFE44xx_TiaCf cf, AFE44xx_Stage2Gain stage2 = AFE44xx_Stage2Gain::Off);

    /** @brief AFE4490 only: separate gain for LED1. Pass enable = false to share the LED2 setting again. */
    bool setLed1ReceiverGain(bool enable, AFE44xx_TiaRf rf = AFE44xx_TiaRf::RF_250K,
                             AFE44xx_TiaCf cf = AFE44xx_TiaCf::pF_5,
                             AFE44xx_Stage2Gain stage2 = AFE44xx_Stage2Gain::Off);

    bool setAmbientDac(uint8_t microamps);  ///< 0-10 uA of ambient cancellation current
    bool setFilterCorner1kHz(bool enable);  ///< AFE4490 only

    // ---- Reading data ------------------------------------------------------

    /**
     * @brief True when a new sample is waiting: the DRDY interrupt fired, or (without one) one
     * period has passed since the last sample.
     */
    bool available();

    /** @brief Read all six result registers. Call when available() is true. */
    bool read(AFE44xx_Sample& sample);

    /** @brief Read the result registers now, without waiting for data ready (1.x behaviour). */
    bool readLatest(AFE44xx_Sample& sample);

    /** @brief Catch ADC_RDY with a RISING interrupt. Needs a DRDY pin with interrupt support. */
    bool enableDataReadyInterrupt();
    void disableDataReadyInterrupt();

    /** @brief Samples that arrived before the previous one was read. */
    uint32_t missedSamples() const { return _missed; }

    /** @brief True when the DRDY interrupt stopped firing and the driver switched to timed reads. */
    bool dataReadyPinFault() const { return _drdyFault; }

    // ---- Diagnostics -------------------------------------------------------

    /**
     * @brief Run the on-chip LED / photodiode open and short checks. Blocks ~50 ms: the sequence
     * takes up to 16 ms and the receiver filters need ~30 ms to settle afterwards. Samples taken
     * meanwhile are discarded. (DIAG_END is a pulse a few clock cycles long, too short to poll.)
     */
    bool runDiagnostics(AFE44xx_Diagnostics& result);

    // ---- Raw register access -----------------------------------------------

    uint32_t readRegister(uint8_t reg);
    bool     writeRegister(uint8_t reg, uint32_t value);  ///< Masked for the chip and verified (not CONTROL0)
    void     printRegisters(Stream& out = Serial);

private:
    uint8_t   _csPin;
    int8_t    _drdyPin;
    int8_t    _pdnPin;
    int8_t    _resetPin;
    SPIClass* _spi;
    SPISettings _spiSettings;

    AFE44xx_Config _config;
    AFE44xx_Status _lastError;
    bool     _initialized;
    bool     _running;
    bool     _standby;
    bool     _spiRead;     // CONTROL0.SPI_READ is set
    uint32_t _beginMs;
    uint32_t _periodUs;

    // Data ready
    volatile uint32_t _irqCount;
    volatile bool     _drdyFlag;
    int8_t   _irqSlot;
    bool     _drdyFault;
    uint32_t _lastIrqCount;  // _irqCount at the previous read()
    uint32_t _watchCount;    // _irqCount when the fault watchdog last saw it change
    uint32_t _watchUs;
    uint32_t _nextPollUs;    // timed reads: when the next sample is due
    uint32_t _missed;
    uint32_t _sequence;

    void     hardwareReset();
    bool     applyConfig();
    bool     writeTiming();
    bool     updateRegister(uint8_t reg, uint32_t value);
    uint32_t registerValue(uint8_t reg) const;
    uint32_t maskRegister(uint8_t reg, uint32_t value) const;
    void     rawWrite(uint8_t reg, uint32_t value);
    uint32_t rawRead(uint8_t reg);
    void     setSpiRead(bool enable);
    bool     readResults(AFE44xx_Sample& sample);
    void     resetDataReady();
    uint32_t irqCount() const;

    static uint32_t tiaBits(AFE44xx_TiaRf rf, AFE44xx_TiaCf cf, AFE44xx_Stage2Gain stage2);

    static Protocentral_AFE44xx* _irqInstances[AFE44XX_MAX_IRQ_INSTANCES];
    static void isr0();
    static void isr1();
    void handleDataReady();
};

// ============================================================================
// 1.x compatibility API
// ============================================================================

/// 1.x result record, filled by AFE44XX::get_AFE44XX_Data().
typedef struct afe44xx_Record {
    int32_t heart_rate;          ///< bpm, 0 until locked
    int32_t spo2;                ///< %, -999 when invalid. Updated when buffer_count_overflow is set
    signed long IR_data;         ///< LED1VAL (IR), signed 22-bit counts
    signed long RED_data;        ///< LED2VAL (red), signed 22-bit counts
    boolean buffer_count_overflow = false;  ///< Set when a new spo2 / heart_rate estimate is ready; clear it after use
} afe44xx_data;

/**
 * 1.x API, kept so existing sketches build and run. New code should use
 * Protocentral_AFE44xx with available() / read() and AFE44xx_Algorithms.
 *
 * get_AFE44XX_Data() reads the latest sample whenever it is called. It times its
 * own calls with micros() and resamples to the algorithms' design rate, so SpO2 and
 * heart rate are correct at any call rate of at least 50 Hz (1.x was only right at 500 Hz).
 */
class AFE44XX {
public:
    AFE44XX(int cs_pin, int pwdn_pin);
    void afe44xx_init();
    boolean get_AFE44XX_Data(afe44xx_data* afe44xx_raw_data);

private:
    Protocentral_AFE44xx _afe;
    AFE44xx_Algorithms   _algo;
    uint32_t _lastCallUs;
    uint32_t _lastReportMs;
    bool     _firstCall;
};

#endif  // PROTOCENTRAL_AFE44XX_H
