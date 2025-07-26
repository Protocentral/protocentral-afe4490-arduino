//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oximeter Shield
//    Example: Computed SpO2 and Heart Rate readings using new API
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

#include <SPI.h>
#include "protocentral_afe44xx.h"

// Pin definitions for AFE44xx breakout board
#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

// Create AFE44xx instance
AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

// Data structures for readings
AFE44xxPPGData ppg_data;
AFE44xxRawData raw_data;

// Previous values for change detection
int32_t heart_rate_prev = 0;
float spo2_prev = 0;
bool initialization_complete = false;

void setup() {
  Serial.begin(115200);
  Serial.println("AFE4490 Pulse Oximeter - Computed SpO2 and Heart Rate Example");
  Serial.println("==============================================================");
  
  // Initialize SPI
  SPI.begin();
  
  // Configure AFE44xx with optimal settings for pulse oximetry
  AFE44xxConfig config = AFE44XX::getDefaultConfig();
  config.led1_current = AFE44xxLEDCurrent::LED_50MA;     // IR LED current
  config.led2_current = AFE44xxLEDCurrent::LED_50MA;     // Red LED current  
  config.tia_gain = AFE44xxTIAGain::TIA_500K;            // TIA gain for good sensitivity
  config.tia_capacitance = AFE44xxTIACapacitance::TIA_5PF; // Low noise
  config.adc_averaging = AFE44xxADCAverage::AVERAGE_4;   // Reduce noise
  config.sample_rate_hz = 100;                           // 100 Hz sample rate
  config.enable_ambient_rejection = true;                // Enable ambient light rejection
  config.enable_led_range_extension = false;             // Standard LED range
  config.operating_mode = AFE44xxOperatingMode::NORMAL_OPERATION;
  
  // Initialize AFE44xx
  AFE44xxError init_result = afe44xx.begin(config);
  
  if (init_result == AFE44xxError::NONE) {
    Serial.println("AFE44xx initialized successfully!");
    Serial.println("Configuration:");
    Serial.print("  LED1 (IR) Current: ");
    Serial.print((int)config.led1_current * 6.25);
    Serial.println(" mA");
    Serial.print("  LED2 (Red) Current: ");
    Serial.print((int)config.led2_current * 6.25);
    Serial.println(" mA");
    Serial.print("  TIA Gain: ");
    Serial.print((int)config.tia_gain);
    Serial.println(" setting");
    Serial.print("  Sample Rate: ");
    Serial.print(config.sample_rate_hz);
    Serial.println(" Hz");
    Serial.println();
    Serial.println("Waiting for sensor to stabilize...");
    delay(5000);  // Allow sensor to stabilize
    Serial.println("Starting measurements:");
    Serial.println("Time(ms)\tSpO2(%)\tHR(bpm)\tSig Quality\tIR DC\tRed DC\tRatio");
    initialization_complete = true;
  } else {
    Serial.print("AFE44xx initialization failed: ");
    Serial.println(AFE44XX::errorToString(init_result));
    while(1) {
      delay(1000);
    }
  }
}

void loop() {
  if (!initialization_complete) {
    delay(1000);
    return;
  }
  
  // Check if new data is available
  if (afe44xx.isDataReady()) {
    
    // Read processed PPG data with SpO2 and heart rate calculations
    AFE44xxError read_result = afe44xx.readPPGData(ppg_data);
    
    if (read_result == AFE44xxError::NONE && ppg_data.data_valid) {
      
      // Check for valid readings
      if (ppg_data.spo2_percent > 0 && ppg_data.heart_rate_bpm > 0) {
        
        // Only print when values change significantly or every 5 seconds
        static uint32_t last_print_time = 0;
        bool values_changed = (abs(heart_rate_prev - ppg_data.heart_rate_bpm) > 2) || 
                             (abs(spo2_prev - ppg_data.spo2_percent) > 1.0);
        bool time_to_print = (millis() - last_print_time) > 5000;
        
        if (values_changed || time_to_print) {
          // Update previous values
          heart_rate_prev = ppg_data.heart_rate_bpm;
          spo2_prev = ppg_data.spo2_percent;
          last_print_time = millis();
          
          // Print detailed measurements in tabular format
          Serial.print(ppg_data.timestamp_ms);
          Serial.print("\t\t");
          Serial.print(ppg_data.spo2_percent, 1);
          Serial.print("\t\t");
          Serial.print(ppg_data.heart_rate_bpm);
          Serial.print("\t\t");
          Serial.print(ppg_data.signal_quality);
          Serial.print("\t\t");
          Serial.print(ppg_data.ir_dc, 0);
          Serial.print("\t");
          Serial.print(ppg_data.red_dc, 0);
          Serial.print("\t");
          Serial.print(ppg_data.ratio_of_ratios, 3);
          Serial.println();
          
          // Check signal quality and provide feedback
          if (ppg_data.signal_quality < 30) {
            Serial.println("Warning: Low signal quality. Check sensor placement.");
          }
        }
        
      } else {
        static uint32_t last_error_print = 0;
        if ((millis() - last_error_print) > 2000) {
          last_error_print = millis();
          Serial.println("No valid pulse detected. Please check sensor placement.");
        }
      }
      
    } else {
      // Handle read errors
      static uint32_t last_error_print = 0;
      if ((millis() - last_error_print) > 1000) {
        last_error_print = millis();
        Serial.print("Data read error: ");
        Serial.println(AFE44XX::errorToString(read_result));
      }
    }
    
    // Optional: Read raw data for debugging
    if (false) {  // Set to true to enable raw data output
      AFE44xxError raw_result = afe44xx.readRawData(raw_data);
      if (raw_result == AFE44xxError::NONE && raw_data.data_valid) {
        Serial.print("Raw - IR: ");
        Serial.print(raw_data.led1_value);
        Serial.print(", Red: ");
        Serial.print(raw_data.led2_value);
        Serial.print(", Ambient IR: ");
        Serial.print(raw_data.ambient1_value);
        Serial.print(", Ambient Red: ");
        Serial.println(raw_data.ambient2_value);
      }
    }
  }
  
  // Small delay to prevent overwhelming the serial output
  delay(10);
}

// Optional function to demonstrate configuration changes at runtime
void changeLEDCurrents() {
  // Example: Increase LED currents for better signal
  AFE44xxError result = afe44xx.setLEDCurrents(AFE44xxLEDCurrent::LED_75MA, AFE44xxLEDCurrent::LED_75MA);
  if (result == AFE44xxError::NONE) {
    Serial.println("LED currents increased to 75 mA");
  } else {
    Serial.print("Failed to change LED currents: ");
    Serial.println(AFE44XX::errorToString(result));
  }
}

// Optional function to demonstrate TIA gain adjustment
void adjustTIAGain() {
  // Example: Change TIA gain for different signal levels
  AFE44xxError result = afe44xx.setTIASettings(AFE44xxTIAGain::TIA_250K, AFE44xxTIACapacitance::TIA_10PF);
  if (result == AFE44xxError::NONE) {
    Serial.println("TIA gain adjusted to 250k ohms");
  } else {
    Serial.print("Failed to adjust TIA gain: ");
    Serial.println(AFE44XX::errorToString(result));
  }
}