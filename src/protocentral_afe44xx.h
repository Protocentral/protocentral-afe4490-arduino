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

#ifndef PROTOCENTRAL_AFE44XX_H
#define PROTOCENTRAL_AFE44XX_H

#include "Arduino.h"
#include <SPI.h>
#include "protocentral_afe44xx_uno_r3_compat.h"

// AFE44xx Error Codes
enum class AFE44xxError : uint8_t {
  NONE = 0,
  SPI_COMMUNICATION_ERROR,
  INVALID_DEVICE_ID,
  INITIALIZATION_FAILED,
  DATA_NOT_READY,
  INVALID_PARAMETER,
  TIMEOUT_ERROR,
  POWER_DOWN_FAILED,
  REGISTER_READ_ERROR,
  REGISTER_WRITE_ERROR
};

// LED Current Settings (per datasheet Table 7-15)
enum class AFE44xxLEDCurrent : uint8_t {
  LED_0MA = 0x00,     // 0 mA
  LED_6MA25 = 0x01,   // 6.25 mA
  LED_12MA5 = 0x02,   // 12.5 mA
  LED_18MA75 = 0x03,  // 18.75 mA
  LED_25MA = 0x04,    // 25 mA
  LED_31MA25 = 0x05,  // 31.25 mA
  LED_37MA5 = 0x06,   // 37.5 mA
  LED_43MA75 = 0x07,  // 43.75 mA
  LED_50MA = 0x08,    // 50 mA
  LED_56MA25 = 0x09,  // 56.25 mA
  LED_62MA5 = 0x0A,   // 62.5 mA
  LED_68MA75 = 0x0B,  // 68.75 mA
  LED_75MA = 0x0C,    // 75 mA
  LED_81MA25 = 0x0D,  // 81.25 mA
  LED_87MA5 = 0x0E,   // 87.5 mA
  LED_93MA75 = 0x0F,  // 93.75 mA
  LED_100MA = 0x10    // 100 mA
};

// TIA Feedback Resistance Settings (per datasheet Table 7-13)
enum class AFE44xxTIAGain : uint8_t {
  TIA_10K = 0x00,    // 10 kΩ
  TIA_25K = 0x01,    // 25 kΩ  
  TIA_50K = 0x02,    // 50 kΩ
  TIA_100K = 0x03,   // 100 kΩ
  TIA_250K = 0x04,   // 250 kΩ
  TIA_500K = 0x05,   // 500 kΩ
  TIA_1M = 0x06,     // 1 MΩ
  TIA_2M = 0x07      // 2 MΩ
};

// TIA Feedback Capacitance Settings (per datasheet Table 7-13)
enum class AFE44xxTIACapacitance : uint8_t {
  TIA_5PF = 0x00,    // 5 pF
  TIA_10PF = 0x01,   // 10 pF
  TIA_20PF = 0x02,   // 20 pF
  TIA_25PF = 0x03    // 25 pF
};

// ADC Averaging Settings (per datasheet Table 7-10)
enum class AFE44xxADCAverage : uint8_t {
  NO_AVERAGING = 0x00,    // No averaging
  AVERAGE_2 = 0x01,       // Average 2 samples
  AVERAGE_4 = 0x02,       // Average 4 samples
  AVERAGE_8 = 0x03,       // Average 8 samples
  AVERAGE_16 = 0x04       // Average 16 samples
};

// Operating Modes
enum class AFE44xxOperatingMode : uint8_t {
  POWER_DOWN = 0x00,
  NORMAL_OPERATION = 0x01,
  DIAGNOSTIC_MODE = 0x02
};

// Forward declaration for legacy support
struct afe44xx_data;

// Raw ADC Data Structure
struct AFE44xxRawData {
  int32_t led1_value;      // LED1 (IR) ADC value
  int32_t led2_value;      // LED2 (Red) ADC value  
  int32_t ambient1_value;  // Ambient LED1 ADC value
  int32_t ambient2_value;  // Ambient LED2 ADC value
  uint32_t timestamp_ms;   // Timestamp when data was acquired
  bool data_valid;         // Flag indicating if data is valid
  AFE44xxError error_code; // Error code if any
};

// Processed PPG Data Structure  
struct AFE44xxPPGData {
  float ir_dc;             // IR DC component
  float red_dc;            // Red DC component
  float ir_ac;             // IR AC component  
  float red_ac;            // Red AC component
  float ratio_of_ratios;   // R = (AC_red/DC_red) / (AC_ir/DC_ir)
  int32_t heart_rate_bpm;  // Calculated heart rate in BPM
  float spo2_percent;      // Calculated SpO2 percentage
  uint8_t signal_quality;  // Signal quality indicator (0-100)
  uint32_t timestamp_ms;   // Timestamp when data was processed
  bool data_valid;         // Flag indicating if calculations are valid
  AFE44xxError error_code; // Error code if any
};

// Configuration Structure
struct AFE44xxConfig {
  AFE44xxLEDCurrent led1_current;           // LED1 (IR) current setting
  AFE44xxLEDCurrent led2_current;           // LED2 (Red) current setting
  AFE44xxTIAGain tia_gain;                  // TIA feedback resistance
  AFE44xxTIACapacitance tia_capacitance;    // TIA feedback capacitance
  AFE44xxADCAverage adc_averaging;          // ADC averaging setting
  uint16_t sample_rate_hz;                  // Sample rate in Hz (25-1000)
  bool enable_ambient_rejection;            // Enable ambient light rejection
  bool enable_led_range_extension;          // Enable 2x LED current range
  AFE44xxOperatingMode operating_mode;      // Operating mode
};

// Main AFE44xx Class
class AFE44XX {
public:
  // Constructor
  AFE44XX(uint8_t cs_pin, uint8_t pwdn_pin, uint8_t drdy_pin = 2);
  
  // Initialization and Control
  AFE44xxError begin(const AFE44xxConfig& config = getDefaultConfig());
  AFE44xxError reset();
  AFE44xxError powerDown();
  AFE44xxError powerUp();
  AFE44xxError setOperatingMode(AFE44xxOperatingMode mode);
  
  // Data Acquisition
  AFE44xxError readRawData(AFE44xxRawData& data);
  AFE44xxError readPPGData(AFE44xxPPGData& data);
  bool isDataReady();
  
  // Configuration Methods
  AFE44xxError setLEDCurrents(AFE44xxLEDCurrent led1, AFE44xxLEDCurrent led2);
  AFE44xxError setTIASettings(AFE44xxTIAGain gain, AFE44xxTIACapacitance capacitance);
  AFE44xxError setADCAveraging(AFE44xxADCAverage averaging);
  AFE44xxError setSampleRate(uint16_t sample_rate_hz);
  AFE44xxError calibrateOffset();
  
  // Status and Diagnostics
  AFE44xxError getLastError() const;
  bool isInitialized() const;
  AFE44xxConfig getCurrentConfig() const;
  AFE44xxError runDiagnostics();
  
  // Utility Methods
  static AFE44xxConfig getDefaultConfig();
  static const char* errorToString(AFE44xxError error);
  static float calculateSpO2FromRatio(float ratio_of_ratios);
  
  // Legacy Support Methods (for backward compatibility)
  boolean get_AFE44XX_Data(afe44xx_data *afe44xx_raw_data);
  void afe44xx_init();
  
private:
  // Hardware Interface
  AFE44xxError writeRegister(uint8_t address, uint32_t data);
  AFE44xxError readRegister(uint8_t address, uint32_t& data);
  AFE44xxError verifyDeviceID();
  
  // Initialization Helpers
  AFE44xxError initializeTimingRegisters(uint16_t sample_rate_hz);
  AFE44xxError configureTIA(const AFE44xxConfig& config);
  AFE44xxError configureLEDs(const AFE44xxConfig& config);
  AFE44xxError configureADC(const AFE44xxConfig& config);
  
  // Data Processing Helpers
  int32_t convertADCValue(uint32_t raw_value);
  void updateSignalQuality(AFE44xxPPGData& data);
  AFE44xxError calculateHeartRate(const AFE44xxRawData& raw_data, AFE44xxPPGData& ppg_data);
  
  // Member Variables
  uint8_t _cs_pin;
  uint8_t _pwdn_pin;
  uint8_t _drdy_pin;
  bool _initialized;
  AFE44xxError _last_error;
  AFE44xxConfig _current_config;
  
  // Signal Processing Buffers
  static const uint8_t BUFFER_SIZE = AFE44XX_BUFFER_SIZE;
  int32_t _ir_buffer[AFE44XX_BUFFER_SIZE];
  int32_t _red_buffer[AFE44XX_BUFFER_SIZE];
  uint8_t _buffer_index;
  bool _buffer_full;
  
  // Timing and State
  uint32_t _last_sample_time_ms;
  uint16_t _sample_interval_ms;
  
  // SPI Settings
  static const uint32_t SPI_CLOCK_SPEED = AFE44XX_SPI_SPEED;  // Platform-specific clock speed
  static const uint8_t SPI_MODE = SPI_MODE0;         // CPOL=0, CPHA=0 per datasheet
};

// Register Definitions (per AFE4490 datasheet Table 7-1)
namespace AFE44xx {
  namespace Registers {
    // Control and Configuration Registers
    constexpr uint8_t CONTROL0       = 0x00;  // Control register 0
    constexpr uint8_t PRPCOUNT       = 0x1D;  // Pulse repetition period count
    constexpr uint8_t CONTROL1       = 0x1E;  // Control register 1
    constexpr uint8_t TIAGAIN        = 0x20;  // TIA gain register
    constexpr uint8_t TIA_AMB_GAIN   = 0x21;  // TIA ambient gain register
    constexpr uint8_t LEDCNTRL       = 0x22;  // LED control register
    constexpr uint8_t CONTROL2       = 0x23;  // Control register 2
    
    // Timing Registers for LED1 (IR)
    constexpr uint8_t LED1STC        = 0x07;  // LED1 start count
    constexpr uint8_t LED1ENDC       = 0x08;  // LED1 end count
    constexpr uint8_t LED1LEDSTC     = 0x09;  // LED1 LED start count
    constexpr uint8_t LED1LEDENDC    = 0x0A;  // LED1 LED end count
    constexpr uint8_t LED1CONVST     = 0x11;  // LED1 conversion start
    constexpr uint8_t LED1CONVEND    = 0x12;  // LED1 conversion end
    
    // Timing Registers for LED2 (Red)
    constexpr uint8_t LED2STC        = 0x01;  // LED2 start count
    constexpr uint8_t LED2ENDC       = 0x02;  // LED2 end count  
    constexpr uint8_t LED2LEDSTC     = 0x03;  // LED2 LED start count
    constexpr uint8_t LED2LEDENDC    = 0x04;  // LED2 LED end count
    constexpr uint8_t LED2CONVST     = 0x0D;  // LED2 conversion start
    constexpr uint8_t LED2CONVEND    = 0x0E;  // LED2 conversion end
    
    // Timing Registers for Ambient LED1
    constexpr uint8_t ALED1STC       = 0x0B;  // Ambient LED1 start count
    constexpr uint8_t ALED1ENDC      = 0x0C;  // Ambient LED1 end count
    constexpr uint8_t ALED1CONVST    = 0x13;  // Ambient LED1 conversion start
    constexpr uint8_t ALED1CONVEND   = 0x14;  // Ambient LED1 conversion end
    
    // Timing Registers for Ambient LED2
    constexpr uint8_t ALED2STC       = 0x05;  // Ambient LED2 start count
    constexpr uint8_t ALED2ENDC      = 0x06;  // Ambient LED2 end count
    constexpr uint8_t ALED2CONVST    = 0x0F;  // Ambient LED2 conversion start
    constexpr uint8_t ALED2CONVEND   = 0x10;  // Ambient LED2 conversion end
    
    // ADC Reset Timing Registers
    constexpr uint8_t ADCRSTCNT0     = 0x15;  // ADC reset count 0
    constexpr uint8_t ADCRSTENDCT0   = 0x16;  // ADC reset end count 0
    constexpr uint8_t ADCRSTCNT1     = 0x17;  // ADC reset count 1
    constexpr uint8_t ADCRSTENDCT1   = 0x18;  // ADC reset end count 1
    constexpr uint8_t ADCRSTCNT2     = 0x19;  // ADC reset count 2
    constexpr uint8_t ADCRSTENDCT2   = 0x1A;  // ADC reset end count 2
    constexpr uint8_t ADCRSTCNT3     = 0x1B;  // ADC reset count 3
    constexpr uint8_t ADCRSTENDCT3   = 0x1C;  // ADC reset end count 3
    
    // Data and Status Registers
    constexpr uint8_t LED1VAL        = 0x2C;  // LED1 ADC value
    constexpr uint8_t LED2VAL        = 0x2A;  // LED2 ADC value
    constexpr uint8_t ALED1VAL       = 0x2D;  // Ambient LED1 ADC value
    constexpr uint8_t ALED2VAL       = 0x2B;  // Ambient LED2 ADC value
    constexpr uint8_t LED1ABSVAL     = 0x2F;  // LED1 absolute value
    constexpr uint8_t LED2ABSVAL     = 0x2E;  // LED2 absolute value
    constexpr uint8_t DIAG           = 0x30;  // Diagnostic register
    constexpr uint8_t ALARM          = 0x29;  // Alarm register
  }
  
  // Bit Field Definitions (per datasheet Tables 7-2 to 7-12)
  namespace BitFields {
    namespace CONTROL0 {
      constexpr uint32_t SW_RST       = 0x000008;  // Software reset
      constexpr uint32_t DIAG_EN      = 0x000004;  // Diagnostic enable
      constexpr uint32_t TM_COUNT_RST = 0x000002;  // Timer counter reset
      constexpr uint32_t READ_DATA    = 0x000001;  // Read data
    }
    
    namespace CONTROL1 {
      constexpr uint32_t TIMERS_EN    = 0x000100;  // Timers enable
      constexpr uint32_t NUMAV_MASK   = 0x000007;  // Number of averages mask
    }
    
    namespace CONTROL2 {
      constexpr uint32_t DYNAMIC      = 0x000200;  // Dynamic power down
      constexpr uint32_t OSC_ENABLE   = 0x000100;  // Oscillator enable
      constexpr uint32_t ILED_2X      = 0x000080;  // LED 2x range
      constexpr uint32_t ILED_RANGE   = 0x000060;  // LED range
      constexpr uint32_t INPUT_SHORT  = 0x000001;  // Input short
    }
    
    namespace TIAGAIN {
      constexpr uint32_t STAGE2_EN    = 0x000008;  // Stage 2 enable
      constexpr uint32_t CF_MASK      = 0x000007;  // Capacitance mask
      constexpr uint32_t RF_MASK      = 0x000038;  // Resistance mask
      constexpr uint32_t RF_SHIFT     = 3;         // Resistance shift
    }
    
    namespace LEDCNTRL {
      constexpr uint32_t LED1_MASK    = 0x00003F;  // LED1 current mask
      constexpr uint32_t LED2_MASK    = 0x000FC0;  // LED2 current mask
      constexpr uint32_t LED2_SHIFT   = 6;         // LED2 current shift
    }
  }
  
  // Constants
  constexpr uint32_t DEVICE_ID_AFE4490 = 0x4490;
  constexpr uint32_t DEVICE_ID_AFE4400 = 0x4400;
  constexpr uint16_t MAX_SAMPLE_RATE_HZ = 1000;
  constexpr uint16_t MIN_SAMPLE_RATE_HZ = 25;
  constexpr uint16_t DEFAULT_SAMPLE_RATE_HZ = 100;
  constexpr uint8_t DATA_READY_TIMEOUT_MS = 100;
  constexpr uint8_t RESET_TIMEOUT_MS = 50;
  constexpr uint8_t POWER_UP_DELAY_MS = 100;
}

// Legacy Support Structure (for backward compatibility)
struct afe44xx_data {
  int32_t heart_rate;
  int32_t spo2;
  signed long IR_data;
  signed long RED_data;
  boolean buffer_count_overflow = false;
};

#endif // PROTOCENTRAL_AFE44XX_H