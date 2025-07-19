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

#include "protocentral_afe44xx.h"

AFE44XX *AFE44XX::_instance = nullptr;

AFE44XX::AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin)
    : _csPin(csPin), _pwdnPin(pwdnPin), _drdyPin(drdyPin),
      _lastError(AFE44xxError::NONE), _dataReady(false),
      _spi(&SPI), _customSPI(false)
{

  _instance = this;

  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);

  pinMode(_pwdnPin, OUTPUT);
  digitalWrite(_pwdnPin, LOW);

  pinMode(_drdyPin, INPUT_PULLUP);
}

AFE44XX::AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin,
                 const AFE44xxSPIConfig &spiConfig)
    : _csPin(csPin), _pwdnPin(pwdnPin), _drdyPin(drdyPin),
      _lastError(AFE44xxError::NONE), _dataReady(false),
      _spi(spiConfig.spiInstance), _customSPI(true)
{

  _instance = this;

  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);

  pinMode(_pwdnPin, OUTPUT);
  digitalWrite(_pwdnPin, LOW);

  pinMode(_drdyPin, INPUT_PULLUP);

  // Store SPI configuration
  _config.spiConfig = spiConfig;
}

AFE44XX::AFE44XX(uint8_t csPin, uint8_t pwdnPin, uint8_t drdyPin,
                 int8_t sckPin, int8_t misoPin, int8_t mosiPin,
                 SPIClass *spiInstance)
    : _csPin(csPin), _pwdnPin(pwdnPin), _drdyPin(drdyPin),
      _lastError(AFE44xxError::NONE), _dataReady(false),
      _spi(spiInstance ? spiInstance : &SPI), _customSPI(true)
{

  _instance = this;

  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);

  pinMode(_pwdnPin, OUTPUT);
  digitalWrite(_pwdnPin, LOW);

  pinMode(_drdyPin, INPUT_PULLUP);

  // Store SPI configuration
  _config.spiConfig = AFE44xxSPIConfig(sckPin, misoPin, mosiPin, _spi);
}

AFE44XX::~AFE44XX()
{
  end();
  _instance = nullptr;
}

AFE44xxError AFE44XX::begin(const AFE44xxConfig &config)
{
  _config = config;
  _lastError = AFE44xxError::NONE;

  digitalWrite(_pwdnPin, LOW);
  delay(AFE44xxConstants::RESET_DELAY_MS);
  digitalWrite(_pwdnPin, HIGH);
  delay(AFE44xxConstants::RESET_DELAY_MS);

  // Initialize SPI with custom pins if specified
  if (_customSPI && _config.spiConfig.sckPin != -1)
  {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3) || defined(ESP32)
    _spi->begin(_config.spiConfig.sckPin, _config.spiConfig.misoPin,
                _config.spiConfig.mosiPin);
#elif defined(ESP8266)
    _spi->begin();
    // ESP8266 doesn't support pin remapping in SPI.begin(),
    // pins must be configured before calling begin()
#else
    _spi->begin();
#endif
  }
  else
  {
    _spi->begin();
  }

  AFE44xxError error = softReset();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = verifyChipID();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = configureTiming();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = configureADC();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = configureLEDs();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = enableInterrupt();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  delay(AFE44xxConstants::INIT_DELAY_MS);

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::end()
{
  detachInterrupt(digitalPinToInterrupt(_drdyPin));
  return enablePowerDown(true);
}

AFE44xxError AFE44XX::readData(AFE44xxData &data)
{
  if (!_dataReady)
  {
    data.dataValid = false;
    return AFE44xxError::NONE;
  }

  uint32_t irValue, redValue;

  AFE44xxError error = writeRegister(AFE44xxRegisters::CONTROL0, AFE44xxBits::CONTROL0_REG_READ);
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = readRegister(AFE44xxRegisters::LED1VAL, irValue);
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = writeRegister(AFE44xxRegisters::CONTROL0, AFE44xxBits::CONTROL0_REG_READ);
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = readRegister(AFE44xxRegisters::LED2VAL, redValue);
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  irValue = (irValue << 10) >> 10;
  redValue = (redValue << 10) >> 10;

  data.irData = static_cast<int32_t>(irValue);
  data.redData = static_cast<int32_t>(redValue);
  data.dataValid = true;
  data.lastError = AFE44xxError::NONE;
  data.timestamp = millis();

  _dataReady = false;

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::setConfig(const AFE44xxConfig &config)
{
  _config = config;

  AFE44xxError error = configureLEDs();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = configureADC();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  error = configureTiming();
  if (error != AFE44xxError::NONE)
  {
    return (_lastError = error);
  }

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::getConfig(AFE44xxConfig &config)
{
  config = _config;
  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::setLEDCurrent(uint8_t ledNum, LEDCurrent current)
{
  if (ledNum < 1 || ledNum > 2)
  {
    return (_lastError = AFE44xxError::INVALID_PARAMETER);
  }

  if (ledNum == 1)
  {
    _config.led1Current = current;
  }
  else
  {
    _config.led2Current = current;
  }

  return configureLEDs();
}

AFE44xxError AFE44XX::setTIAGain(TIAGain gain)
{
  _config.tiaGain = gain;
  return configureADC();
}

AFE44xxError AFE44XX::setSampleRate(SampleRate rate)
{
  _config.sampleRate = rate;
  return configureTiming();
}

AFE44xxError AFE44XX::setAveraging(uint8_t factor)
{
  if (factor > 15)
  {
    return (_lastError = AFE44xxError::INVALID_PARAMETER);
  }

  _config.averagingFactor = factor;
  return configureADC();
}

AFE44xxError AFE44XX::enablePowerDown(bool enable)
{
  _config.enablePowerDown = enable;

  if (enable)
  {
    digitalWrite(_pwdnPin, LOW);
  }
  else
  {
    digitalWrite(_pwdnPin, HIGH);
    delay(AFE44xxConstants::RESET_DELAY_MS);
  }

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::softReset()
{
  AFE44xxError error = writeRegister(AFE44xxRegisters::CONTROL0, 0x000000);
  if (error != AFE44xxError::NONE)
  {
    return error;
  }

  error = writeRegister(AFE44xxRegisters::CONTROL0, AFE44xxBits::CONTROL0_SW_RST);
  if (error != AFE44xxError::NONE)
  {
    return error;
  }

  delay(AFE44xxConstants::RESET_DELAY_MS);

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::performSelfTest()
{
  AFE44xxDiagnostics diag;
  AFE44xxError error = getDiagnostics(diag);

  if (error != AFE44xxError::NONE)
  {
    return error;
  }

  if (diag.ledFault || diag.pdShort || diag.pdOpen || diag.gainError)
  {
    return AFE44xxError::LED_FAULT;
  }

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::getDiagnostics(AFE44xxDiagnostics &diag)
{
  uint32_t diagValue;
  AFE44xxError error = readRegister(AFE44xxRegisters::DIAG, diagValue);

  if (error != AFE44xxError::NONE)
  {
    return error;
  }

  diag.ledFault = (diagValue & AFE44xxBits::DIAG_LED_ALM) != 0;
  diag.pdShort = (diagValue & AFE44xxBits::DIAG_PD_ALM) != 0;
  diag.pdOpen = (diagValue & 0x000400) != 0;
  diag.gainError = (diagValue & 0x000800) != 0;
  diag.ambientLight = static_cast<uint16_t>((diagValue >> 16) & 0xFFFF);
  diag.temperature = 25.0f + ((diagValue & 0xFF) - 128) * 0.125f;

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::clearFaults()
{
  return writeRegister(AFE44xxRegisters::ALARM, 0x000000);
}

bool AFE44XX::isDataReady()
{
  return _dataReady;
}

const char *AFE44XX::getErrorString(AFE44xxError error)
{
  switch (error)
  {
  case AFE44xxError::NONE:
    return "No error";
  case AFE44xxError::SPI_ERROR:
    return "SPI communication error";
  case AFE44xxError::CHIP_NOT_RESPONDING:
    return "Chip not responding";
  case AFE44xxError::INVALID_PARAMETER:
    return "Invalid parameter";
  case AFE44xxError::BUFFER_OVERFLOW:
    return "Buffer overflow";
  case AFE44xxError::LED_FAULT:
    return "LED fault detected";
  case AFE44xxError::AMBIENT_LIGHT_HIGH:
    return "Ambient light too high";
  case AFE44xxError::POWER_DOWN_ERROR:
    return "Power down error";
  default:
    return "Unknown error";
  }
}

AFE44xxConfig AFE44XX::getDefaultConfig()
{
  AFE44xxConfig config;
  config.chipType = AFE44xxChipType::AFE4490;
  config.led1Current = LEDCurrent::CURRENT_50MA;
  config.led2Current = LEDCurrent::CURRENT_50MA;
  config.tiaGain = TIAGain::GAIN_500K;
  config.sampleRate = SampleRate::RATE_500HZ;
  config.averagingFactor = 3;
  config.enableDiagnostics = true;
  config.enablePowerDown = false;
  config.spiConfig = AFE44xxSPIConfig(); // Use default SPI pins
  return config;
}

AFE44xxConfig AFE44XX::getDefaultConfigWithSPI(const AFE44xxSPIConfig &spiConfig)
{
  AFE44xxConfig config = getDefaultConfig();
  config.spiConfig = spiConfig;
  return config;
}

AFE44xxConfig AFE44XX::getDefaultConfigWithSPI(int8_t sckPin, int8_t misoPin, int8_t mosiPin)
{
  AFE44xxConfig config = getDefaultConfig();
  config.spiConfig = AFE44xxSPIConfig(sckPin, misoPin, mosiPin);
  return config;
}

AFE44xxError AFE44XX::setSPIConfig(const AFE44xxSPIConfig &spiConfig)
{
  _config.spiConfig = spiConfig;
  _spi = spiConfig.spiInstance;
  _customSPI = true;

// Reinitialize SPI with new configuration
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3) || defined(ESP32)
  if (spiConfig.sckPin != -1)
  {
    _spi->begin(spiConfig.sckPin, spiConfig.misoPin, spiConfig.mosiPin);
  }
  else
  {
    _spi->begin();
  }
#else
  _spi->begin();
#endif

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::setSPIPins(int8_t sckPin, int8_t misoPin, int8_t mosiPin)
{
  return setSPIConfig(AFE44xxSPIConfig(sckPin, misoPin, mosiPin, _spi));
}

AFE44xxError AFE44XX::writeRegister(uint8_t address, uint32_t data)
{
  SPISettings spiSettings(AFE44xxConstants::SPI_SPEED, MSBFIRST, SPI_MODE0);

  for (uint8_t retry = 0; retry < AFE44xxConstants::MAX_RETRIES; retry++)
  {
    _spi->beginTransaction(spiSettings);
    digitalWrite(_csPin, LOW);

    _spi->transfer(address);
    _spi->transfer((data >> 16) & 0xFF);
    _spi->transfer((data >> 8) & 0xFF);
    _spi->transfer(data & 0xFF);

    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    delayMicroseconds(10);

    uint32_t readback;
    AFE44xxError error = readRegister(address, readback);

    if (error == AFE44xxError::NONE && readback == data)
    {
      return AFE44xxError::NONE;
    }

    delayMicroseconds(100);
  }

  return AFE44xxError::SPI_ERROR;
}

AFE44xxError AFE44XX::readRegister(uint8_t address, uint32_t &data)
{
  SPISettings spiSettings(AFE44xxConstants::SPI_SPEED, MSBFIRST, SPI_MODE0);

  for (uint8_t retry = 0; retry < AFE44xxConstants::MAX_RETRIES; retry++)
  {
    _spi->beginTransaction(spiSettings);
    digitalWrite(_csPin, LOW);

    _spi->transfer(address);
    data = 0;
    data |= static_cast<uint32_t>(_spi->transfer(0)) << 16;
    data |= static_cast<uint32_t>(_spi->transfer(0)) << 8;
    data |= _spi->transfer(0);

    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    if (data != 0xFFFFFF && data != 0x000000)
    {
      return AFE44xxError::NONE;
    }

    delayMicroseconds(100);
  }

  return AFE44xxError::SPI_ERROR;
}

AFE44xxError AFE44XX::verifyChipID()
{
  uint32_t expectedID = (_config.chipType == AFE44xxChipType::AFE4490) ? AFE44xxConstants::CHIP_ID_AFE4490 : AFE44xxConstants::CHIP_ID_AFE4400;

  uint32_t actualID;
  AFE44xxError error = readRegister(AFE44xxRegisters::SPARE1, actualID);

  if (error != AFE44xxError::NONE)
  {
    return error;
  }

  if ((actualID & 0xFFFF) != expectedID)
  {
    return AFE44xxError::CHIP_NOT_RESPONDING;
  }

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::configureTiming()
{
  uint32_t prpCount = 8000000 / static_cast<uint32_t>(_config.sampleRate) - 1;

  AFE44xxError error = writeRegister(AFE44xxRegisters::PRPCOUNT, prpCount);
  if (error != AFE44xxError::NONE)
    return error;

  uint32_t period = prpCount + 1;
  uint32_t ledOnTime = period / 4;
  uint32_t convTime = ledOnTime / 2;

  error = writeRegister(AFE44xxRegisters::LED2STC, period * 0);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED2ENDC, period * 0 + ledOnTime - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED2LEDSTC, period * 0);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED2LEDENDC, period * 0 + ledOnTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED2STC, period * 0 + ledOnTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED2ENDC, period * 0 + ledOnTime * 2 - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED2CONVST, period * 0 + convTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED2CONVEND, period * 0 + ledOnTime + convTime - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED2CONVST, period * 0 + ledOnTime + convTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED2CONVEND, period * 0 + ledOnTime * 2 + convTime - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1STC, period * 0 + ledOnTime * 2);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1ENDC, period * 0 + ledOnTime * 3 - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1LEDSTC, period * 0 + ledOnTime * 2);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1LEDENDC, period * 0 + ledOnTime * 3);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED1STC, period * 0 + ledOnTime * 3);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED1ENDC, period - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1CONVST, period * 0 + ledOnTime * 2 + convTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::LED1CONVEND, period * 0 + ledOnTime * 3 + convTime - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED1CONVST, period * 0 + ledOnTime * 3 + convTime);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ALED1CONVEND, period - 1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTCNT0, 0x000000);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTENDCT0, 0x000000);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTCNT1, period * 0 + ledOnTime * 2);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTENDCT1, period * 0 + ledOnTime * 2);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTCNT2, period * 0 + ledOnTime * 3);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTENDCT2, period * 0 + ledOnTime * 3);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTCNT3, period * 0);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::ADCRSTENDCT3, period * 0);
  if (error != AFE44xxError::NONE)
    return error;

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::configureADC()
{
  uint32_t tiaGainValue = static_cast<uint32_t>(_config.tiaGain);
  AFE44xxError error = writeRegister(AFE44xxRegisters::TIAGAIN, tiaGainValue);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::TIA_AMB_GAIN, 0x000001);
  if (error != AFE44xxError::NONE)
    return error;

  uint32_t control1 = AFE44xxBits::CONTROL1_TIMERS_EN |
                      (static_cast<uint32_t>(_config.averagingFactor) & 0x0F);

  error = writeRegister(AFE44xxRegisters::CONTROL1, control1);
  if (error != AFE44xxError::NONE)
    return error;

  error = writeRegister(AFE44xxRegisters::CONTROL2, 0x000000);
  if (error != AFE44xxError::NONE)
    return error;

  return AFE44xxError::NONE;
}

AFE44xxError AFE44XX::configureLEDs()
{
  uint32_t led1Current = static_cast<uint32_t>(_config.led1Current);
  uint32_t led2Current = static_cast<uint32_t>(_config.led2Current);

  uint32_t ledControl = (led2Current << 8) | led1Current;

  return writeRegister(AFE44xxRegisters::LEDCNTRL, ledControl);
}

AFE44xxError AFE44XX::enableInterrupt()
{
  attachInterrupt(digitalPinToInterrupt(_drdyPin), dataReadyISR, FALLING);
  return AFE44xxError::NONE;
}

void AFE44XX::dataReadyISR()
{
  if (_instance)
  {
    _instance->handleDataReady();
  }
}

void AFE44XX::handleDataReady()
{
  _dataReady = true;
}