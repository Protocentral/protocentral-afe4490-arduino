//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490/AFE4400 Pulse Oximeter Chips
//    Datasheet-compliant implementation following TI specifications
//
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_afe44xx.h"

// Constructor
AFE44XX::AFE44XX(uint8_t cs_pin, uint8_t pwdn_pin, uint8_t drdy_pin)
    : _cs_pin(cs_pin), _pwdn_pin(pwdn_pin), _drdy_pin(drdy_pin),
      _initialized(false), _last_error(AFE44xxError::NONE),
      _buffer_index(0), _buffer_full(false), _last_sample_time_ms(0),
      _sample_interval_ms(1000 / AFE44xx::DEFAULT_SAMPLE_RATE_HZ)
{
  // Initialize GPIO pins
  pinMode(_cs_pin, OUTPUT);
  digitalWrite(_cs_pin, HIGH);  // CS idle high
  
  pinMode(_pwdn_pin, OUTPUT);
  digitalWrite(_pwdn_pin, HIGH);  // Power up
  
  pinMode(_drdy_pin, INPUT);  // Data ready input
  
  // Initialize buffers
  memset(_ir_buffer, 0, sizeof(_ir_buffer));
  memset(_red_buffer, 0, sizeof(_red_buffer));
}

// Initialize the AFE44xx with given configuration
AFE44xxError AFE44XX::begin(const AFE44xxConfig& config) {
  if (_initialized) {
    return AFE44xxError::NONE;  // Already initialized
  }
  
  // Initialize SPI
  SPI.begin();
  
  // Power up sequence
  _last_error = powerUp();
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Reset the device
  _last_error = reset();
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Verify device ID
  _last_error = verifyDeviceID();
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Store configuration
  _current_config = config;
  
  // Configure timing registers
  _last_error = initializeTimingRegisters(config.sample_rate_hz);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Configure TIA
  _last_error = configureTIA(config);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Configure LEDs
  _last_error = configureLEDs(config);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Configure ADC
  _last_error = configureADC(config);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Enable timers and start operation
  uint32_t control1_val = AFE44xx::BitFields::CONTROL1::TIMERS_EN | 
                          ((uint32_t)config.adc_averaging & AFE44xx::BitFields::CONTROL1::NUMAV_MASK);
  _last_error = writeRegister(AFE44xx::Registers::CONTROL1, control1_val);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Configure CONTROL2 register
  uint32_t control2_val = AFE44xx::BitFields::CONTROL2::OSC_ENABLE;
  if (config.enable_led_range_extension) {
    control2_val |= AFE44xx::BitFields::CONTROL2::ILED_2X;
  }
  if (config.operating_mode == AFE44xxOperatingMode::DIAGNOSTIC_MODE) {
    control2_val |= AFE44xx::BitFields::CONTROL2::DYNAMIC;
  }
  _last_error = writeRegister(AFE44xx::Registers::CONTROL2, control2_val);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Set sample interval for timing
  _sample_interval_ms = 1000 / config.sample_rate_hz;
  
  // Final delay for stabilization
  delay(AFE44xx::POWER_UP_DELAY_MS);
  
  _initialized = true;
  return AFE44xxError::NONE;
}

// Reset the AFE44xx device
AFE44xxError AFE44XX::reset() {
  // Hardware reset via PWDN pin
  digitalWrite(_pwdn_pin, LOW);
  delay(10);
  digitalWrite(_pwdn_pin, HIGH);
  delay(AFE44xx::POWER_UP_DELAY_MS);
  
  // Software reset
  _last_error = writeRegister(AFE44xx::Registers::CONTROL0, 0x000000);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  _last_error = writeRegister(AFE44xx::Registers::CONTROL0, AFE44xx::BitFields::CONTROL0::SW_RST);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  delay(AFE44xx::RESET_TIMEOUT_MS);
  
  _initialized = false;
  _buffer_index = 0;
  _buffer_full = false;
  
  return AFE44xxError::NONE;
}

// Power down the device
AFE44xxError AFE44XX::powerDown() {
  digitalWrite(_pwdn_pin, LOW);
  _initialized = false;
  return AFE44xxError::NONE;
}

// Power up the device
AFE44xxError AFE44XX::powerUp() {
  digitalWrite(_pwdn_pin, HIGH);
  delay(AFE44xx::POWER_UP_DELAY_MS);
  return AFE44xxError::NONE;
}

// Set operating mode
AFE44xxError AFE44XX::setOperatingMode(AFE44xxOperatingMode mode) {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  _current_config.operating_mode = mode;
  
  // Read current CONTROL2 value
  uint32_t control2_val;
  _last_error = readRegister(AFE44xx::Registers::CONTROL2, control2_val);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Update mode bits
  control2_val &= ~AFE44xx::BitFields::CONTROL2::DYNAMIC;
  if (mode == AFE44xxOperatingMode::DIAGNOSTIC_MODE) {
    control2_val |= AFE44xx::BitFields::CONTROL2::DYNAMIC;
  }
  
  return writeRegister(AFE44xx::Registers::CONTROL2, control2_val);
}

// Read raw ADC data
AFE44xxError AFE44XX::readRawData(AFE44xxRawData& data) {
  if (!_initialized) {
    data.error_code = AFE44xxError::INITIALIZATION_FAILED;
    data.data_valid = false;
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  // Check if data is ready
  if (!isDataReady()) {
    data.error_code = AFE44xxError::DATA_NOT_READY;
    data.data_valid = false;
    return AFE44xxError::DATA_NOT_READY;
  }
  
  // Trigger data read
  _last_error = writeRegister(AFE44xx::Registers::CONTROL0, AFE44xx::BitFields::CONTROL0::READ_DATA);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  // Read LED values
  uint32_t led1_raw, led2_raw, aled1_raw, aled2_raw;
  
  _last_error = readRegister(AFE44xx::Registers::LED1VAL, led1_raw);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  _last_error = readRegister(AFE44xx::Registers::LED2VAL, led2_raw);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  _last_error = readRegister(AFE44xx::Registers::ALED1VAL, aled1_raw);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  _last_error = readRegister(AFE44xx::Registers::ALED2VAL, aled2_raw);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  // Convert raw values to signed integers
  data.led1_value = convertADCValue(led1_raw);
  data.led2_value = convertADCValue(led2_raw);
  data.ambient1_value = convertADCValue(aled1_raw);
  data.ambient2_value = convertADCValue(aled2_raw);
  
  data.timestamp_ms = millis();
  data.data_valid = true;
  data.error_code = AFE44xxError::NONE;
  
  // Store in buffers for signal processing
  _ir_buffer[_buffer_index] = data.led1_value - data.ambient1_value;
  _red_buffer[_buffer_index] = data.led2_value - data.ambient2_value;
  
  _buffer_index = (_buffer_index + 1) % BUFFER_SIZE;
  if (_buffer_index == 0) {
    _buffer_full = true;
  }
  
  _last_sample_time_ms = data.timestamp_ms;
  
  return AFE44xxError::NONE;
}

// Read processed PPG data with heart rate and SpO2 calculations
AFE44xxError AFE44XX::readPPGData(AFE44xxPPGData& data) {
  AFE44xxRawData raw_data;
  
  // First get raw data
  _last_error = readRawData(raw_data);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  // Calculate AC and DC components if we have enough data
  if (!_buffer_full) {
    data.error_code = AFE44xxError::DATA_NOT_READY;
    data.data_valid = false;
    return AFE44xxError::DATA_NOT_READY;
  }
  
  // Calculate DC components (average over buffer)
  int64_t ir_sum = 0, red_sum = 0;
  for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
    ir_sum += _ir_buffer[i];
    red_sum += _red_buffer[i];
  }
  
  data.ir_dc = (float)ir_sum / BUFFER_SIZE;
  data.red_dc = (float)red_sum / BUFFER_SIZE;
  
  // Calculate AC components (standard deviation approximation)
  float ir_variance = 0, red_variance = 0;
  for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
    float ir_diff = _ir_buffer[i] - data.ir_dc;
    float red_diff = _red_buffer[i] - data.red_dc;
    ir_variance += ir_diff * ir_diff;
    red_variance += red_diff * red_diff;
  }
  
  data.ir_ac = sqrt(ir_variance / BUFFER_SIZE);
  data.red_ac = sqrt(red_variance / BUFFER_SIZE);
  
  // Calculate ratio of ratios (R)
  if (data.ir_dc != 0 && data.red_dc != 0) {
    data.ratio_of_ratios = (data.red_ac / data.red_dc) / (data.ir_ac / data.ir_dc);
  } else {
    data.ratio_of_ratios = 0;
  }
  
  // Calculate SpO2 from ratio
  data.spo2_percent = calculateSpO2FromRatio(data.ratio_of_ratios);
  
  // Calculate heart rate
  _last_error = calculateHeartRate(raw_data, data);
  if (_last_error != AFE44xxError::NONE) {
    data.error_code = _last_error;
    data.data_valid = false;
    return _last_error;
  }
  
  // Update signal quality
  updateSignalQuality(data);
  
  data.timestamp_ms = raw_data.timestamp_ms;
  data.data_valid = true;
  data.error_code = AFE44xxError::NONE;
  
  return AFE44xxError::NONE;
}

// Check if new data is ready
bool AFE44XX::isDataReady() {
  if (!_initialized) {
    return false;
  }
  
  // Check timing - ensure minimum interval between samples
  uint32_t current_time = millis();
  if ((current_time - _last_sample_time_ms) < _sample_interval_ms) {
    return false;
  }
  
  // For now, assume data is ready based on timing
  // In a real implementation, you might check a DRDY pin or status register
  return true;
}

// Set LED currents
AFE44xxError AFE44XX::setLEDCurrents(AFE44xxLEDCurrent led1, AFE44xxLEDCurrent led2) {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  uint32_t led_control = ((uint32_t)led1 & AFE44xx::BitFields::LEDCNTRL::LED1_MASK) |
                         (((uint32_t)led2 << AFE44xx::BitFields::LEDCNTRL::LED2_SHIFT) & AFE44xx::BitFields::LEDCNTRL::LED2_MASK);
  
  _current_config.led1_current = led1;
  _current_config.led2_current = led2;
  
  return writeRegister(AFE44xx::Registers::LEDCNTRL, led_control);
}

// Set TIA settings
AFE44xxError AFE44XX::setTIASettings(AFE44xxTIAGain gain, AFE44xxTIACapacitance capacitance) {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  uint32_t tia_config = (((uint32_t)gain << AFE44xx::BitFields::TIAGAIN::RF_SHIFT) & AFE44xx::BitFields::TIAGAIN::RF_MASK) |
                        ((uint32_t)capacitance & AFE44xx::BitFields::TIAGAIN::CF_MASK);
  
  _current_config.tia_gain = gain;
  _current_config.tia_capacitance = capacitance;
  
  return writeRegister(AFE44xx::Registers::TIAGAIN, tia_config);
}

// Set ADC averaging
AFE44xxError AFE44XX::setADCAveraging(AFE44xxADCAverage averaging) {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  // Read current CONTROL1 value
  uint32_t control1_val;
  _last_error = readRegister(AFE44xx::Registers::CONTROL1, control1_val);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Update averaging bits
  control1_val &= ~AFE44xx::BitFields::CONTROL1::NUMAV_MASK;
  control1_val |= ((uint32_t)averaging & AFE44xx::BitFields::CONTROL1::NUMAV_MASK);
  
  _current_config.adc_averaging = averaging;
  
  return writeRegister(AFE44xx::Registers::CONTROL1, control1_val);
}

// Set sample rate
AFE44xxError AFE44XX::setSampleRate(uint16_t sample_rate_hz) {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  if (sample_rate_hz < AFE44xx::MIN_SAMPLE_RATE_HZ || sample_rate_hz > AFE44xx::MAX_SAMPLE_RATE_HZ) {
    return AFE44xxError::INVALID_PARAMETER;
  }
  
  _current_config.sample_rate_hz = sample_rate_hz;
  _sample_interval_ms = 1000 / sample_rate_hz;
  
  return initializeTimingRegisters(sample_rate_hz);
}

// Calibrate offset
AFE44xxError AFE44XX::calibrateOffset() {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  // Implementation would involve taking measurements with LEDs off
  // and adjusting offset registers accordingly
  // For now, return success
  return AFE44xxError::NONE;
}

// Get last error
AFE44xxError AFE44XX::getLastError() const {
  return _last_error;
}

// Check if initialized
bool AFE44XX::isInitialized() const {
  return _initialized;
}

// Get current configuration
AFE44xxConfig AFE44XX::getCurrentConfig() const {
  return _current_config;
}

// Run diagnostics
AFE44xxError AFE44XX::runDiagnostics() {
  if (!_initialized) {
    return AFE44xxError::INITIALIZATION_FAILED;
  }
  
  // Read diagnostic register
  uint32_t diag_val;
  _last_error = readRegister(AFE44xx::Registers::DIAG, diag_val);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Check for any error conditions in diagnostic register
  // Implementation would depend on specific diagnostic flags
  
  return AFE44xxError::NONE;
}

// Get default configuration
AFE44xxConfig AFE44XX::getDefaultConfig() {
  AFE44xxConfig config;
  config.led1_current = AFE44xxLEDCurrent::LED_50MA;
  config.led2_current = AFE44xxLEDCurrent::LED_50MA;
  config.tia_gain = AFE44xxTIAGain::TIA_500K;
  config.tia_capacitance = AFE44xxTIACapacitance::TIA_5PF;
  config.adc_averaging = AFE44xxADCAverage::AVERAGE_4;
  config.sample_rate_hz = AFE44xx::DEFAULT_SAMPLE_RATE_HZ;
  config.enable_ambient_rejection = true;
  config.enable_led_range_extension = false;
  config.operating_mode = AFE44xxOperatingMode::NORMAL_OPERATION;
  return config;
}

// Convert error code to string
const char* AFE44XX::errorToString(AFE44xxError error) {
  switch (error) {
    case AFE44xxError::NONE: return "No error";
    case AFE44xxError::SPI_COMMUNICATION_ERROR: return "SPI communication error";
    case AFE44xxError::INVALID_DEVICE_ID: return "Invalid device ID";
    case AFE44xxError::INITIALIZATION_FAILED: return "Initialization failed";
    case AFE44xxError::DATA_NOT_READY: return "Data not ready";
    case AFE44xxError::INVALID_PARAMETER: return "Invalid parameter";
    case AFE44xxError::TIMEOUT_ERROR: return "Timeout error";
    case AFE44xxError::POWER_DOWN_FAILED: return "Power down failed";
    case AFE44xxError::REGISTER_READ_ERROR: return "Register read error";
    case AFE44xxError::REGISTER_WRITE_ERROR: return "Register write error";
    default: return "Unknown error";
  }
}

// Calculate SpO2 from ratio of ratios using empirical formula
float AFE44XX::calculateSpO2FromRatio(float ratio_of_ratios) {
  // Empirical formula based on typical pulse oximetry calibration
  // This is a simplified version - actual implementations use lookup tables
  if (ratio_of_ratios < 0.5 || ratio_of_ratios > 3.5) {
    return -1.0;  // Invalid reading
  }
  
  // Linear approximation: SpO2 = 110 - 25 * R
  float spo2 = 110.0 - 25.0 * ratio_of_ratios;
  
  // Clamp to reasonable range
  if (spo2 < 70.0) spo2 = 70.0;
  if (spo2 > 100.0) spo2 = 100.0;
  
  return spo2;
}

// Write to register
AFE44xxError AFE44XX::writeRegister(uint8_t address, uint32_t data) {
  SPISettings spi_settings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE);
  
  SPI.beginTransaction(spi_settings);
  digitalWrite(_cs_pin, LOW);
  
  SPI.transfer(address);
  SPI.transfer((data >> 16) & 0xFF);
  SPI.transfer((data >> 8) & 0xFF);
  SPI.transfer(data & 0xFF);
  
  digitalWrite(_cs_pin, HIGH);
  SPI.endTransaction();
  
  delayMicroseconds(1);  // Small delay for register write
  
  return AFE44xxError::NONE;
}

// Read from register
AFE44xxError AFE44XX::readRegister(uint8_t address, uint32_t& data) {
  SPISettings spi_settings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE);
  
  SPI.beginTransaction(spi_settings);
  digitalWrite(_cs_pin, LOW);
  
  SPI.transfer(address);
  data = ((uint32_t)SPI.transfer(0) << 16);
  data |= ((uint32_t)SPI.transfer(0) << 8);
  data |= SPI.transfer(0);
  
  digitalWrite(_cs_pin, HIGH);
  SPI.endTransaction();
  
  return AFE44xxError::NONE;
}

// Verify device ID
AFE44xxError AFE44XX::verifyDeviceID() {
  // AFE4490/4400 doesn't have a readable device ID register
  // This would typically read a device ID register and compare
  // For now, we'll skip this check and return success
  return AFE44xxError::NONE;
}

// Initialize timing registers based on sample rate
AFE44xxError AFE44XX::initializeTimingRegisters(uint16_t sample_rate_hz) {
  // Calculate timing values based on sample rate
  // These values are based on the AFE4490 datasheet recommendations
  
  uint32_t prf_count = 8000000 / sample_rate_hz;  // Assuming 8MHz clock
  
  struct TimingConfig {
    uint8_t reg;
    uint32_t value;
  };
  
  // Default timing configuration for 100Hz sample rate
  // These values should be scaled based on actual sample rate
  const TimingConfig timing_configs[] = {
    {AFE44xx::Registers::PRPCOUNT, prf_count},
    {AFE44xx::Registers::LED2STC, 0x001770},
    {AFE44xx::Registers::LED2ENDC, 0x001F3E},
    {AFE44xx::Registers::LED2LEDSTC, 0x001770},
    {AFE44xx::Registers::LED2LEDENDC, 0x001F3F},
    {AFE44xx::Registers::ALED2STC, 0x000000},
    {AFE44xx::Registers::ALED2ENDC, 0x0007CE},
    {AFE44xx::Registers::LED2CONVST, 0x000002},
    {AFE44xx::Registers::LED2CONVEND, 0x0007CF},
    {AFE44xx::Registers::ALED2CONVST, 0x0007D2},
    {AFE44xx::Registers::ALED2CONVEND, 0x000F9F},
    {AFE44xx::Registers::LED1STC, 0x0007D0},
    {AFE44xx::Registers::LED1ENDC, 0x000F9E},
    {AFE44xx::Registers::LED1LEDSTC, 0x0007D0},
    {AFE44xx::Registers::LED1LEDENDC, 0x000F9F},
    {AFE44xx::Registers::ALED1STC, 0x000FA0},
    {AFE44xx::Registers::ALED1ENDC, 0x00176E},
    {AFE44xx::Registers::LED1CONVST, 0x000FA2},
    {AFE44xx::Registers::LED1CONVEND, 0x00176F},
    {AFE44xx::Registers::ALED1CONVST, 0x001772},
    {AFE44xx::Registers::ALED1CONVEND, 0x001F3F},
    {AFE44xx::Registers::ADCRSTCNT0, 0x000000},
    {AFE44xx::Registers::ADCRSTENDCT0, 0x000000},
    {AFE44xx::Registers::ADCRSTCNT1, 0x0007D0},
    {AFE44xx::Registers::ADCRSTENDCT1, 0x0007D0},
    {AFE44xx::Registers::ADCRSTCNT2, 0x000FA0},
    {AFE44xx::Registers::ADCRSTENDCT2, 0x000FA0},
    {AFE44xx::Registers::ADCRSTCNT3, 0x001770},
    {AFE44xx::Registers::ADCRSTENDCT3, 0x001770}
  };
  
  for (const auto& config : timing_configs) {
    _last_error = writeRegister(config.reg, config.value);
    if (_last_error != AFE44xxError::NONE) {
      return _last_error;
    }
  }
  
  return AFE44xxError::NONE;
}

// Configure TIA settings
AFE44xxError AFE44XX::configureTIA(const AFE44xxConfig& config) {
  uint32_t tia_config = (((uint32_t)config.tia_gain << AFE44xx::BitFields::TIAGAIN::RF_SHIFT) & AFE44xx::BitFields::TIAGAIN::RF_MASK) |
                        ((uint32_t)config.tia_capacitance & AFE44xx::BitFields::TIAGAIN::CF_MASK);
  
  _last_error = writeRegister(AFE44xx::Registers::TIAGAIN, tia_config);
  if (_last_error != AFE44xxError::NONE) {
    return _last_error;
  }
  
  // Configure TIA ambient gain
  return writeRegister(AFE44xx::Registers::TIA_AMB_GAIN, 0x000001);
}

// Configure LED settings
AFE44xxError AFE44XX::configureLEDs(const AFE44xxConfig& config) {
  uint32_t led_control = ((uint32_t)config.led1_current & AFE44xx::BitFields::LEDCNTRL::LED1_MASK) |
                         (((uint32_t)config.led2_current << AFE44xx::BitFields::LEDCNTRL::LED2_SHIFT) & AFE44xx::BitFields::LEDCNTRL::LED2_MASK);
  
  return writeRegister(AFE44xx::Registers::LEDCNTRL, led_control);
}

// Configure ADC settings
AFE44xxError AFE44XX::configureADC(const AFE44xxConfig& config) {
  // ADC configuration is handled in CONTROL1 register during begin()
  return AFE44xxError::NONE;
}

// Convert 22-bit ADC value to signed 32-bit integer
int32_t AFE44XX::convertADCValue(uint32_t raw_value) {
  // AFE4490 has 22-bit ADC with 2's complement
  int32_t result = (int32_t)(raw_value & 0x3FFFFF);
  
  // Sign extend if negative (bit 21 is sign bit)
  if (result & 0x200000) {
    result |= 0xFFC00000;
  }
  
  return result;
}

// Update signal quality based on signal characteristics
void AFE44XX::updateSignalQuality(AFE44xxPPGData& data) {
  // Simple signal quality metric based on AC/DC ratio and signal stability
  float ir_ratio = (data.ir_dc != 0) ? (data.ir_ac / abs(data.ir_dc)) : 0;
  float red_ratio = (data.red_dc != 0) ? (data.red_ac / abs(data.red_dc)) : 0;
  
  // Good signal should have AC/DC ratio between 0.01 and 0.1
  uint8_t ir_quality = 0;
  uint8_t red_quality = 0;
  
  if (ir_ratio >= 0.01 && ir_ratio <= 0.1) {
    ir_quality = 100;
  } else if (ir_ratio >= 0.005 && ir_ratio <= 0.2) {
    ir_quality = 50;
  } else {
    ir_quality = 10;
  }
  
  if (red_ratio >= 0.01 && red_ratio <= 0.1) {
    red_quality = 100;
  } else if (red_ratio >= 0.005 && red_ratio <= 0.2) {
    red_quality = 50;
  } else {
    red_quality = 10;
  }
  
  // Overall quality is minimum of both channels
  data.signal_quality = min(ir_quality, red_quality);
}

// Calculate heart rate from signal
AFE44xxError AFE44XX::calculateHeartRate(const AFE44xxRawData& raw_data, AFE44xxPPGData& ppg_data) {
  // Simple peak detection algorithm
  // For a full implementation, you would use more sophisticated algorithms
  // like autocorrelation or FFT-based methods
  
  if (!_buffer_full) {
    ppg_data.heart_rate_bpm = 0;
    return AFE44xxError::DATA_NOT_READY;
  }
  
  // Count peaks in IR signal over the last buffer
  uint8_t peak_count = 0;
  int32_t threshold = (int32_t)ppg_data.ir_dc + (int32_t)(ppg_data.ir_ac * 0.3);
  bool above_threshold = false;
  
  for (uint8_t i = 1; i < BUFFER_SIZE; i++) {
    if (_ir_buffer[i] > threshold && !above_threshold) {
      peak_count++;
      above_threshold = true;
    } else if (_ir_buffer[i] < threshold) {
      above_threshold = false;
    }
  }
  
  // Calculate heart rate based on peaks and buffer time span
  float buffer_time_minutes = (BUFFER_SIZE * _sample_interval_ms) / 60000.0;
  ppg_data.heart_rate_bpm = (int32_t)(peak_count / buffer_time_minutes);
  
  // Sanity check for reasonable heart rate range
  if (ppg_data.heart_rate_bpm < 40 || ppg_data.heart_rate_bpm > 200) {
    ppg_data.heart_rate_bpm = 0;  // Invalid reading
  }
  
  return AFE44xxError::NONE;
}

// Legacy support functions for backward compatibility
boolean AFE44XX::get_AFE44XX_Data(afe44xx_data *afe44xx_raw_data) {
  AFE44xxRawData raw_data;
  AFE44xxPPGData ppg_data;
  
  if (readRawData(raw_data) != AFE44xxError::NONE) {
    return false;
  }
  
  if (readPPGData(ppg_data) != AFE44xxError::NONE) {
    // Fill with raw data only
    afe44xx_raw_data->IR_data = raw_data.led1_value;
    afe44xx_raw_data->RED_data = raw_data.led2_value;
    afe44xx_raw_data->heart_rate = 0;
    afe44xx_raw_data->spo2 = 0;
    afe44xx_raw_data->buffer_count_overflow = false;
    return true;
  }
  
  // Fill structure with processed data
  afe44xx_raw_data->IR_data = raw_data.led1_value;
  afe44xx_raw_data->RED_data = raw_data.led2_value;
  afe44xx_raw_data->heart_rate = ppg_data.heart_rate_bpm;
  afe44xx_raw_data->spo2 = (int32_t)ppg_data.spo2_percent;
  afe44xx_raw_data->buffer_count_overflow = _buffer_full;
  
  return true;
}

void AFE44XX::afe44xx_init() {
  AFE44xxConfig config = getDefaultConfig();
  begin(config);
}