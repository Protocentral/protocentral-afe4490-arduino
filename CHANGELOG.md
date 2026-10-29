# Changelog

All notable changes to this library are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased] - 2.0.0

New driver API and rewritten SpO2 / heart-rate algorithms. The 1.x API (`AFE44XX`,
`afe44xx_init()`, `get_AFE44XX_Data()`, `afe44xx_data`) still builds and runs. This is a
major release because SpO2 and heart rate change for every sketch. See "Migrating from 1.x"
in the README.

Not released until SpO2 and heart rate are validated against a reference oximeter.

### Added
- `Protocentral_AFE44xx` driver class, matching the ProtoCentral ADS1292R / ADS1293 2.0 drivers:
  - injected `SPIClass`, with an `SPISettings` transaction around every access
  - DRDY, PWDN and RESET pins optional (`-1`)
  - `bool begin()` with an SPI read-back presence check, and an `AFE44xx_Status` from `lastError()`
  - ESP32 `begin(sck, miso, mosi, config)`
- `AFE44xx_Config` sets the whole device in one call. Its defaults are the 1.x register profile.
- **AFE4400 support** (`AFE44xx_Chip::AFE4400`, Sensything OX): the driver forces the AFE4400's must-be-1 bits and rejects AFE4490-only settings with `Unsupported`.
- **Timing engine:** PRF from 63 Hz to 5 kHz and LED duty cycle from 1 to 25 %. All 29 timing registers are computed from these two values, and the ADC averages (1-16) are checked against what fits at that PRF.
- **Data path:**
  - ADC_RDY on a RISING interrupt, with no SPI inside the ISR
  - `available()` / `read()` return all six result registers: LED1, LED2, both ambients and both ambient-subtracted values
  - `missedSamples()` and a sample sequence counter
  - `dataReadyPinFault()`: when the interrupt stops firing, the driver reads on a timer instead
- Runtime setters for every option: LED current, LED range, TX reference, TIA RF / CF, stage 2 gain, separate LED1 gain, ambient DAC, filter corner, PRF, duty cycle and averages. All of them are validated, and every register write is verified by read-back.
- `runDiagnostics()`: LED and photodiode open / short flags from the DIAG register.
- `standby()` / `wakeup()`, `printRegisters()`, raw `readRegister()` / `writeRegister()` with per-chip masking.
- `AFE44xx_Algorithms`:
  - SpO2, heart rate, perfusion index and signal-quality flags
  - validity flags for SpO2 and heart rate
  - works at any input rate of at least 50 Hz
- **Examples 01-07:** Serial Plotter, OpenView, SpO2 / heart rate, high-rate interrupt, diagnostics, custom configuration, and the 1.x API.
- **Repo tooling:**
  - `keywords.txt` and `CHANGELOG.md`
  - Arduino Lint, and an 11-board compile workflow that includes ESP32-S3
  - host tests (`extras/host_test`) and `scripts/build.sh`

### Changed
- **SpO2 and heart rate are rewritten**, because 1.x was only correct at one exact call rate:
  - every input is low-pass filtered and resampled to a fixed 50 Hz design rate, for any PRF, including non-integer ratios
  - the algorithms use the ambient-subtracted LED − ambient values
  - a streaming beat detector on IR finds each beat; SpO2 comes from the AC/DC ratio of each beat, median over 5 beats
  - results are reported invalid until 4 regular beats of good signal have been seen
  - the SpO2 curve is the 1.x one, now overridable with `AFE44XX_SPO2_A/B/C`
- **Legacy `get_AFE44XX_Data()`** times its own calls with `micros()` and resamples from them, so 1.x sketches get correct SpO2 and heart rate at any call rate of at least 50 Hz. It sets `buffer_count_overflow` at every beat, and at least every 4 s.
- **Uno RAM:** the 1.x SpO2 example drops from 1838 B to 553 B. The 1.x SpO2 code alone used 800 B of static buffers.
- **`afe44xx_init()`** no longer blocks for 2 s. It takes about 100 ms, which is the datasheet power-up time.
- **The OpenView example** now matches OpenView's `afe4490` descriptor: payload red, IR, HR, SpO2, at 100 packets/s. 1.x sent IR, red, SpO2, HR.
- **Register macros** are prefixed `AFE44XX_REG_*`.
- **Examples** are renamed to the `NN.Name` scheme.

### Fixed
- **SpO2 was wrong at the rates the 1.x examples ran.** The SpO2 code took every 20th call and assumed 25 Hz, which was only right at 500 calls/s. The `delay(8)` examples called about 125 times/s.
- **Heart rate assumed 125 calls/s.**
- **DRDY was never used.** The examples polled with `delay(8)` and lost 3 of every 4 samples at 500 Hz.
- **AFE4400:** 1.x wrote LEDCNTRL bit 16 and CONTROL2 bits 17 and 8 as 0, but the datasheet requires 1.
- **Register comments:** 1.x said "average 3 samples" (it was 8), "LED_RANGE=100mA, LED=50mA" (it was 150 mA full scale, LED code 20 = 11.7 mA) and "RF = 500k" (it was 250 kΩ).
- **Header pollution:** the 1.x headers leaked unprefixed macros (`CONTROL0`, `DIAG`, `ALARM`, `LED1VAL`…), a duplicate `SPARE4`, and a global `min()` macro.
- **README:**
  - the shield text named the "ECG/Respiration shield"
  - the breakout's START pin was described as a conversion start. On Breakout v2 it is wired to the AFE's RESET.

### Removed
- `Protocentral_spo2_algorithm.h` / `spo2_algorithm` and `protocentral_hr_algorithm.h` / `hr_algo`, replaced by `AFE44xx_Algorithms`.
- `src/obsolete/`.

## [1.4.0] - 2025-04-19

- Last release of the 1.x driver.
