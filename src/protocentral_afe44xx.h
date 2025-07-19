//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE44XX Pulse Oximeter and Heart Rate Sensor
//    Supports AFE4490 and AFE4400 chips with full feature implementation
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

// Platform-specific default SPI pin definitions
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
// ESP32-S3 specific SPI pins (VSPI/SPI3)
#define AFE44XX_DEFAULT_SCK 12
#define AFE44XX_DEFAULT_MISO 13
#define AFE44XX_DEFAULT_MOSI 11
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S2)
// ESP32-S2 specific SPI pins
#define AFE44XX_DEFAULT_SCK 36
#define AFE44XX_DEFAULT_MISO 37
#define AFE44XX_DEFAULT_MOSI 35
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
// ESP32-C3 specific SPI pins
#define AFE44XX_DEFAULT_SCK 6
#define AFE44XX_DEFAULT_MISO 5
#define AFE44XX_DEFAULT_MOSI 7
#elif defined(ESP32)
// ESP32 classic SPI pins (VSPI/SPI3)
#define AFE44XX_DEFAULT_SCK 18
#define AFE44XX_DEFAULT_MISO 19
#define AFE44XX_DEFAULT_MOSI 23
#elif defined(ESP8266)
#define AFE44XX_DEFAULT_SCK 14
#define AFE44XX_DEFAULT_MISO 12
#define AFE44XX_DEFAULT_MOSI 13
#elif defined(ARDUINO_ARCH_STM32)
#define AFE44XX_DEFAULT_SCK PA5
#define AFE44XX_DEFAULT_MISO PA6
#define AFE44XX_DEFAULT_MOSI PA7
#else
// Default Arduino pins (Uno, Nano, etc.)
#define AFE44XX_DEFAULT_SCK 13
#define AFE44XX_DEFAULT_MISO 12
#define AFE44XX_DEFAULT_MOSI 11
#endif

enum class AFE44xxChipType
{
  AFE4400,
  AFE4490
};

enum class AFE44xxError
{
  NONE = 0,
  SPI_ERROR,
  CHIP_NOT_RESPONDING,
  INVALID_PARAMETER,
  BUFFER_OVERFLOW,
  LED_FAULT,
  AMBIENT_LIGHT_HIGH,
  POWER_DOWN_ERROR
};

enum class LEDCurrent
{
  CURRENT_0MA = 0x00,
  CURRENT_25MA = 0x01,
  CURRENT_50MA = 0x02,
  CURRENT_75MA = 0x03,
  CURRENT_100MA = 0x04
};

enum class TIAGain
{
  GAIN_10K = 0x00,
  GAIN_25K = 0x01,
  GAIN_50K = 0x02,
  GAIN_100K = 0x03,
  GAIN_250K = 0x04,
  GAIN_500K = 0x05,
  GAIN_1M = 0x06,
  GAIN_2M = 0x07
};

enum class SampleRate
{
  RATE_100HZ = 100,
  RATE_200HZ = 200,
  RATE_500HZ = 500,
  RATE_1000HZ = 1000
};

struct AFE44xxSPIConfig
{
  int8_t sckPin;
  int8_t misoPin;
  int8_t mosiPin;
  SPIClass *spiInstance;

  // Constructor with default pins
  AFE44xxSPIConfig() : sckPin(AFE44XX_DEFAULT_SCK),
                       misoPin(AFE44XX_DEFAULT_MISO),
                       mosiPin(AFE44XX_DEFAULT_MOSI),
                       spiInstance(&SPI) {}

  // Constructor with custom pins
  AFE44xxSPIConfig(int8_t sck, int8_t miso, int8_t mosi, SPIClass *spi = nullptr) : sckPin(sck),
                                                                                    misoPin(miso),
                                                                                    mosiPin(mosi),
                                                                                    spiInstance(spi ? spi : &SPI) {}
};

struct AFE44xxConfig
{
  AFE44xxChipType chipType;
  LEDCurrent led1Current;
  LEDCurrent led2Current;
  TIAGain tiaGain;
  SampleRate sampleRate;
  uint8_t averagingFactor;
  bool enableDiagnostics;
  bool enablePowerDown;
  AFE44xxSPIConfig spiConfig;
};

struct AFE44xxData
{
  int32_t heartRate;
  int32_t spo2;
  int32_t irData;
  int32_t redData;
  bool dataValid;
  AFE44xxError lastError;
  uint32_t timestamp;
};

struct AFE44xxDiagnostics
{
  bool ledFault;
  bool pdShort;
  bool pdOpen;
  bool gainError;
  uint16_t ambientLight;
  float temperature;
};

class AFE44XX
{
public:
  // Default constructor with standard SPI
  AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin = 2);

  // Constructor with custom SPI pins
  AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin,
          const AFE44xxSPIConfig &spiConfig);

  // Constructor with custom SPI pins (individual parameters)
  AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin,
          int8_t sckPin, int8_t misoPin, int8_t mosiPin,
          SPIClass *spiInstance = nullptr);

  ~AFE44XX();

  AFE44xxError begin(const AFE44xxConfig &config = getDefaultConfig());
  AFE44xxError end();

  AFE44xxError readData(AFE44xxData &data);
  AFE44xxError setConfig(const AFE44xxConfig &config);
  AFE44xxError getConfig(AFE44xxConfig &config);

  AFE44xxError setLEDCurrent(uint8_t ledNum, LEDCurrent current);
  AFE44xxError setTIAGain(TIAGain gain);
  AFE44xxError setSampleRate(SampleRate rate);
  AFE44xxError setAveraging(uint8_t factor);

  AFE44xxError enablePowerDown(bool enable);
  AFE44xxError softReset();
  AFE44xxError performSelfTest();

  AFE44xxError getDiagnostics(AFE44xxDiagnostics &diag);
  AFE44xxError clearFaults();

  bool isDataReady();
  AFE44xxError getLastError() const { return _lastError; }
  const char *getErrorString(AFE44xxError error);

  // SPI configuration methods
  AFE44xxError setSPIConfig(const AFE44xxSPIConfig &spiConfig);
  AFE44xxError setSPIPins(int8_t sckPin, int8_t misoPin, int8_t mosiPin);
  AFE44xxSPIConfig getSPIConfig() const { return _config.spiConfig; }

  static AFE44xxConfig getDefaultConfig();
  static AFE44xxConfig getDefaultConfigWithSPI(const AFE44xxSPIConfig &spiConfig);
  static AFE44xxConfig getDefaultConfigWithSPI(int8_t sckPin, int8_t misoPin, int8_t mosiPin);

private:
  AFE44xxError writeRegister(uint8_t address, uint32_t data);
  AFE44xxError readRegister(uint8_t address, uint32_t &data);
  AFE44xxError verifyChipID();
  AFE44xxError configureTiming();
  AFE44xxError configureADC();
  AFE44xxError configureLEDs();
  AFE44xxError enableInterrupt();

  static void dataReadyISR();
  void handleDataReady();

  uint8_t _csPin;
  uint8_t _pwdnPin;
  uint8_t _drdyPin;

  AFE44xxConfig _config;
  AFE44xxError _lastError;
  volatile bool _dataReady;

  SPIClass *_spi;
  bool _customSPI;

  static AFE44XX *_instance;
};

namespace AFE44xxRegisters
{
  constexpr uint8_t CONTROL0 = 0x00;
  constexpr uint8_t LED2STC = 0x01;
  constexpr uint8_t LED2ENDC = 0x02;
  constexpr uint8_t LED2LEDSTC = 0x03;
  constexpr uint8_t LED2LEDENDC = 0x04;
  constexpr uint8_t ALED2STC = 0x05;
  constexpr uint8_t ALED2ENDC = 0x06;
  constexpr uint8_t LED1STC = 0x07;
  constexpr uint8_t LED1ENDC = 0x08;
  constexpr uint8_t LED1LEDSTC = 0x09;
  constexpr uint8_t LED1LEDENDC = 0x0a;
  constexpr uint8_t ALED1STC = 0x0b;
  constexpr uint8_t ALED1ENDC = 0x0c;
  constexpr uint8_t LED2CONVST = 0x0d;
  constexpr uint8_t LED2CONVEND = 0x0e;
  constexpr uint8_t ALED2CONVST = 0x0f;
  constexpr uint8_t ALED2CONVEND = 0x10;
  constexpr uint8_t LED1CONVST = 0x11;
  constexpr uint8_t LED1CONVEND = 0x12;
  constexpr uint8_t ALED1CONVST = 0x13;
  constexpr uint8_t ALED1CONVEND = 0x14;
  constexpr uint8_t ADCRSTCNT0 = 0x15;
  constexpr uint8_t ADCRSTENDCT0 = 0x16;
  constexpr uint8_t ADCRSTCNT1 = 0x17;
  constexpr uint8_t ADCRSTENDCT1 = 0x18;
  constexpr uint8_t ADCRSTCNT2 = 0x19;
  constexpr uint8_t ADCRSTENDCT2 = 0x1a;
  constexpr uint8_t ADCRSTCNT3 = 0x1b;
  constexpr uint8_t ADCRSTENDCT3 = 0x1c;
  constexpr uint8_t PRPCOUNT = 0x1d;
  constexpr uint8_t CONTROL1 = 0x1e;
  constexpr uint8_t SPARE1 = 0x1f;
  constexpr uint8_t TIAGAIN = 0x20;
  constexpr uint8_t TIA_AMB_GAIN = 0x21;
  constexpr uint8_t LEDCNTRL = 0x22;
  constexpr uint8_t CONTROL2 = 0x23;
  constexpr uint8_t SPARE2 = 0x24;
  constexpr uint8_t SPARE3 = 0x25;
  constexpr uint8_t SPARE4 = 0x26;
  constexpr uint8_t RESERVED1 = 0x27;
  constexpr uint8_t RESERVED2 = 0x28;
  constexpr uint8_t ALARM = 0x29;
  constexpr uint8_t LED2VAL = 0x2a;
  constexpr uint8_t ALED2VAL = 0x2b;
  constexpr uint8_t LED1VAL = 0x2c;
  constexpr uint8_t ALED1VAL = 0x2d;
  constexpr uint8_t LED2ABSVAL = 0x2e;
  constexpr uint8_t LED1ABSVAL = 0x2f;
  constexpr uint8_t DIAG = 0x30;
}

namespace AFE44xxBits
{
  constexpr uint32_t CONTROL0_SW_RST = 0x000008;
  constexpr uint32_t CONTROL0_REG_READ = 0x000001;
  constexpr uint32_t CONTROL1_TIMERS_EN = 0x000100;
  constexpr uint32_t DIAG_LED_ALM = 0x000100;
  constexpr uint32_t DIAG_PD_ALM = 0x000200;
}

namespace AFE44xxConstants
{
  constexpr uint32_t CHIP_ID_AFE4490 = 0x000490;
  constexpr uint32_t CHIP_ID_AFE4400 = 0x000400;
  constexpr uint32_t SPI_SPEED = 2000000;
  constexpr uint8_t MAX_RETRIES = 3;
  constexpr uint16_t RESET_DELAY_MS = 100;
  constexpr uint16_t INIT_DELAY_MS = 1000;
}

#endif
