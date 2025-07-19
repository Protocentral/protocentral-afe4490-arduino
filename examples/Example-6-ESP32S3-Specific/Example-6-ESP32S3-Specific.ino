//////////////////////////////////////////////////////////////////////////////////////////
//
//    ESP32-S3 Specific Example for AFE44XX Library
//    Demonstrates ESP32-S3 specific features and pin configurations
//
//    ESP32-S3 Features:
//    - Enhanced GPIO matrix with improved flexibility
//    - Better pin assignment capabilities than classic ESP32
//    - Support for any GPIO pins as SPI
//    - Dual core architecture with WiFi and Bluetooth
//
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//    For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_afe44xx.h"

// AFE44XX control pins
#define AFE44XX_CS_PIN   10
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 5

// ESP32-S3 specific SPI pin configurations
// Configuration 1: Default ESP32-S3 SPI pins
#define ESP32S3_DEFAULT_SCK  12
#define ESP32S3_DEFAULT_MISO 13
#define ESP32S3_DEFAULT_MOSI 11

// Configuration 2: Alternative ESP32-S3 pins (example)
#define ESP32S3_ALT_SCK  36
#define ESP32S3_ALT_MISO 37
#define ESP32S3_ALT_MOSI 35

// Configuration 3: Custom user-defined pins
#define ESP32S3_CUSTOM_SCK  18
#define ESP32S3_CUSTOM_MISO 19
#define ESP32S3_CUSTOM_MOSI 23

// Uncomment one of the following configurations:

// Option 1: Use default ESP32-S3 pins (recommended)
AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

// Option 2: Use alternative pins
// AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN,
//                  ESP32S3_ALT_SCK, ESP32S3_ALT_MISO, ESP32S3_ALT_MOSI);

// Option 3: Use custom pins
// AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN,
//                  ESP32S3_CUSTOM_SCK, ESP32S3_CUSTOM_MISO, ESP32S3_CUSTOM_MOSI);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  
  Serial.println("ESP32-S3 AFE44XX Pulse Oximeter Example");
  Serial.println("========================================");
  
  // Print ESP32-S3 specific information
  Serial.println("ESP32-S3 Features:");
  Serial.println("- Enhanced GPIO matrix");
  Serial.println("- Flexible pin assignment");
  Serial.println("- Dual core with WiFi/BT");
  Serial.println();
  
  // Display current configuration
  AFE44xxConfig config = AFE44XX::getDefaultConfig();
  Serial.println("SPI Configuration:");
  Serial.print("  SCK Pin:  ");
  Serial.println(config.spiConfig.sckPin);
  Serial.print("  MISO Pin: ");
  Serial.println(config.spiConfig.misoPin);
  Serial.print("  MOSI Pin: ");
  Serial.println(config.spiConfig.mosiPin);
  Serial.print("  CS Pin:   ");
  Serial.println(AFE44XX_CS_PIN);
  Serial.print("  PWDN Pin: ");
  Serial.println(AFE44XX_PWDN_PIN);
  Serial.print("  DRDY Pin: ");
  Serial.println(AFE44XX_DRDY_PIN);
  Serial.println();
  
  // Initialize AFE44XX
  Serial.println("Initializing AFE44XX...");
  AFE44xxError error = afe44xx.begin(config);
  
  if (error != AFE44xxError::NONE) {
    Serial.print("Initialization failed: ");
    Serial.println(afe44xx.getErrorString(error));
    Serial.println("Please check connections and try again.");
    while (1) {
      delay(1000);
    }
  }
  
  Serial.println("AFE44XX initialized successfully!");
  
  // Perform self-test
  Serial.println("Performing self-test...");
  error = afe44xx.performSelfTest();
  if (error == AFE44xxError::NONE) {
    Serial.println("Self-test passed!");
  } else {
    Serial.print("Self-test failed: ");
    Serial.println(afe44xx.getErrorString(error));
    Serial.println("Device may still function, but check connections.");
  }
  
  // Get and display diagnostics
  AFE44xxDiagnostics diag;
  error = afe44xx.getDiagnostics(diag);
  if (error == AFE44xxError::NONE) {
    Serial.println("\nDiagnostic Information:");
    Serial.print("  LED Fault: ");
    Serial.println(diag.ledFault ? "YES" : "NO");
    Serial.print("  PD Short: ");
    Serial.println(diag.pdShort ? "YES" : "NO");
    Serial.print("  PD Open: ");
    Serial.println(diag.pdOpen ? "YES" : "NO");
    Serial.print("  Gain Error: ");
    Serial.println(diag.gainError ? "YES" : "NO");
    Serial.print("  Ambient Light: ");
    Serial.println(diag.ambientLight);
    Serial.print("  Temperature: ");
    Serial.print(diag.temperature);
    Serial.println(" °C");
  }
  
  Serial.println("\nStarting data acquisition...");
  Serial.println("Place finger on sensor for readings.");
  Serial.println("Format: IR_Value, Red_Value, Timestamp");
  Serial.println("=====================================");
}

void loop() {
  if (afe44xx.isDataReady()) {
    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);
    
    if (error == AFE44xxError::NONE && data.dataValid) {
      // Print data in CSV format for easy plotting
      Serial.print(data.irData);
      Serial.print(", ");
      Serial.print(data.redData);
      Serial.print(", ");
      Serial.println(data.timestamp);
      
      // Optional: Calculate and display basic metrics
      // Note: This is a simple example. For accurate SpO2 and HR,
      // use the dedicated algorithm libraries
      static uint32_t lastPrint = 0;
      if (millis() - lastPrint > 5000) {  // Every 5 seconds
        lastPrint = millis();
        
        // Simple signal quality indicator
        int32_t signalStrength = abs(data.irData) + abs(data.redData);
        Serial.print("Signal Strength: ");
        if (signalStrength > 100000) {
          Serial.println("GOOD");
        } else if (signalStrength > 50000) {
          Serial.println("FAIR");
        } else {
          Serial.println("POOR - Check finger placement");
        }
      }
      
    } else if (error != AFE44xxError::NONE) {
      Serial.print("Data read error: ");
      Serial.println(afe44xx.getErrorString(error));
    }
  }
  
  // Small delay to prevent overwhelming the serial output
  delay(20);
}

/*
ESP32-S3 Pin Recommendations:

Default Configuration (recommended):
- SCK:  GPIO 12
- MISO: GPIO 13  
- MOSI: GPIO 11
- CS:   GPIO 10
- PWDN: GPIO 4
- DRDY: GPIO 5

Alternative Configuration:
- SCK:  GPIO 36
- MISO: GPIO 37
- MOSI: GPIO 35
- CS:   GPIO 10
- PWDN: GPIO 4
- DRDY: GPIO 5

Custom Configuration (example):
- SCK:  GPIO 18
- MISO: GPIO 19
- MOSI: GPIO 23
- CS:   GPIO 10
- PWDN: GPIO 4
- DRDY: GPIO 5

Notes:
1. ESP32-S3 has enhanced GPIO flexibility compared to classic ESP32
2. Any GPIO pin can be used for SPI with good performance
3. Avoid pins used by internal flash (GPIO 26-32 on some modules)
4. Check your specific ESP32-S3 module's pinout
5. Consider power consumption if using this in battery applications
6. The enhanced GPIO matrix provides better signal integrity

For optimal performance:
- Use shorter wires for high-speed SPI signals
- Add proper decoupling capacitors
- Ensure stable power supply
- Shield from EMI sources
*/
