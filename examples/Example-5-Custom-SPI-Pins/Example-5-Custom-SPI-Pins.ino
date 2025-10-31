//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE44XX Pulse Oximeter - Custom SPI Pins Example
//    Demonstrates how to use custom SPI pins on different platforms (ESP32, ESP8266, etc.)
//
//    This example shows various ways to configure custom SPI pins:
//    1. Using constructor with individual pins
//    2. Using constructor with SPI configuration struct
//    3. Using configuration methods after initialization
//
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//    For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_afe44xx.h"

// Standard pin definitions
#define AFE44XX_CS_PIN 7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

// Custom SPI pin definitions for different platforms
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
  // ESP32-S3 custom SPI pins (VSPI/SPI3 default)
  #define CUSTOM_SCK_PIN  12
  #define CUSTOM_MISO_PIN 13
  #define CUSTOM_MOSI_PIN 11
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S2)
  // ESP32-S2 custom SPI pins
  #define CUSTOM_SCK_PIN  36
  #define CUSTOM_MISO_PIN 37
  #define CUSTOM_MOSI_PIN 35
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP32C3)
  // ESP32-C3 custom SPI pins
  #define CUSTOM_SCK_PIN  6
  #define CUSTOM_MISO_PIN 5
  #define CUSTOM_MOSI_PIN 7
#elif defined(ESP32)
  // ESP32 classic custom SPI pins
  #define CUSTOM_SCK_PIN  18
  #define CUSTOM_MISO_PIN 19
  #define CUSTOM_MOSI_PIN 23
#elif defined(ESP8266)
  // ESP8266 custom SPI pins
  #define CUSTOM_SCK_PIN  14
  #define CUSTOM_MISO_PIN 12
  #define CUSTOM_MOSI_PIN 13
#else
  // Arduino Uno/Nano default pins (for demonstration)
  #define CUSTOM_SCK_PIN  13
  #define CUSTOM_MISO_PIN 12
  #define CUSTOM_MOSI_PIN 11
#endif

// Uncomment one of the following examples to test different initialization methods

// Example 1: Standard initialization (uses default SPI pins)
AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

// Example 2: Custom SPI pins using constructor
// AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN,
//                  CUSTOM_SCK_PIN, CUSTOM_MISO_PIN, CUSTOM_MOSI_PIN);

// Example 3: Custom SPI configuration struct
// AFE44xxSPIConfig customSPI(CUSTOM_SCK_PIN, CUSTOM_MISO_PIN, CUSTOM_MOSI_PIN);
// AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN, customSPI);

void setup() {
  Serial.begin(115200);
  Serial.println("AFE44XX Custom SPI Pins Example");
  Serial.println("================================");
  
  // Example 4: Configure SPI pins after initialization
  // afe44xx.setSPIPins(CUSTOM_SCK_PIN, CUSTOM_MISO_PIN, CUSTOM_MOSI_PIN);
  
  // Example 5: Using default config with custom SPI
  // AFE44xxConfig config = AFE44XX::getDefaultConfigWithSPI(CUSTOM_SCK_PIN, CUSTOM_MISO_PIN, CUSTOM_MOSI_PIN);
  
  // Initialize with default configuration
  AFE44xxConfig config = AFE44XX::getDefaultConfig();
  
  Serial.print("Initializing AFE44XX with SPI pins - SCK: ");
  Serial.print(config.spiConfig.sckPin);
  Serial.print(", MISO: ");
  Serial.print(config.spiConfig.misoPin);
  Serial.print(", MOSI: ");
  Serial.println(config.spiConfig.mosiPin);
  
  AFE44xxError error = afe44xx.begin(config);
  
  if (error != AFE44xxError::NONE) {
    Serial.print("AFE44XX initialization failed: ");
    Serial.println(afe44xx.getErrorString(error));
    while (1) {
      delay(1000);
    }
  }
  
  Serial.println("AFE44XX initialized successfully!");
  
  // Print current SPI configuration
  AFE44xxSPIConfig currentSPI = afe44xx.getSPIConfig();
  Serial.println("\nCurrent SPI Configuration:");
  Serial.print("  SCK Pin: ");
  Serial.println(currentSPI.sckPin);
  Serial.print("  MISO Pin: ");
  Serial.println(currentSPI.misoPin);
  Serial.print("  MOSI Pin: ");
  Serial.println(currentSPI.mosiPin);
  Serial.print("  SPI Instance: ");
  Serial.println(currentSPI.spiInstance == &SPI ? "SPI" : "Custom");
  
  // Perform self-test
  Serial.println("\nPerforming self-test...");
  error = afe44xx.performSelfTest();
  if (error == AFE44xxError::NONE) {
    Serial.println("Self-test passed!");
  } else {
    Serial.print("Self-test failed: ");
    Serial.println(afe44xx.getErrorString(error));
  }
  
  Serial.println("\nStarting data acquisition...");
}

void loop() {
  if (afe44xx.isDataReady()) {
    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);
    
    if (error == AFE44xxError::NONE && data.dataValid) {
      Serial.print("IR: ");
      Serial.print(data.irData);
      Serial.print(", Red: ");
      Serial.print(data.redData);
      Serial.print(", Timestamp: ");
      Serial.println(data.timestamp);
    } else if (error != AFE44xxError::NONE) {
      Serial.print("Data read error: ");
      Serial.println(afe44xx.getErrorString(error));
    }
  }
  
  delay(10);
}

// Platform-specific SPI pin recommendations:
/*
ESP32-S3:
- Default VSPI: SCK=12, MISO=13, MOSI=11
- Can use any GPIO pins for SPI
- More flexible pin assignment than classic ESP32

ESP32-S2:
- Default SPI: SCK=36, MISO=37, MOSI=35
- Single core, optimized for low power
- Can use any GPIO pins for SPI

ESP32-C3:
- Default SPI: SCK=6, MISO=5, MOSI=7
- RISC-V based, compact design
- Limited GPIO pins but flexible assignment

ESP32 Classic:
- Default VSPI: SCK=18, MISO=19, MOSI=23
- Default HSPI: SCK=14, MISO=12, MOSI=13
- Can use any GPIO pins for SPI

ESP8266:
- Hardware SPI: SCK=14, MISO=12, MOSI=13
- Limited flexibility in pin assignment

Arduino Uno/Nano:
- Hardware SPI: SCK=13, MISO=12, MOSI=11
- Software SPI possible but slower

STM32:
- Multiple SPI instances available
- Pin assignment depends on specific board

Usage Notes:
1. Always check your board's pinout diagram
2. Ensure SPI pins don't conflict with other peripherals
3. Some platforms require specific pin combinations for hardware SPI
4. Software SPI is slower but more flexible
5. Test with simple connections before complex setups
6. ESP32-S3 has improved GPIO matrix flexibility
*/
