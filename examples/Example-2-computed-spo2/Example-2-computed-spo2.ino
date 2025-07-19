//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE44XX Pulse Oximeter Shield - SPO2 Computation Example
//    Updated to use the new improved library with error handling and diagnostics
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
#include "Protocentral_spo2_algorithm.h"
#include "protocentral_hr_algorithm.h"

#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

spo2_algorithm spo2Calc;
hr_algo hrCalc;

uint16_t irBuffer[100];
uint16_t redBuffer[100];
uint8_t bufferIndex = 0;
uint8_t decimationCounter = 0;
const uint8_t DECIMATION_FACTOR = 20;

int32_t heartRate = 0;
int32_t spo2 = 0;
int8_t spo2Valid = 0;
int8_t hrValid = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("AFE44XX SPO2 Computation Example");
    Serial.println("================================");
    
    AFE44xxConfig config = AFE44XX::getDefaultConfig();
    config.led1Current = LEDCurrent::CURRENT_50MA;
    config.led2Current = LEDCurrent::CURRENT_50MA;
    config.tiaGain = TIAGain::GAIN_500K;
    config.sampleRate = SampleRate::RATE_500HZ;
    
    Serial.println("Initializing AFE44XX...");
    AFE44xxError error = afe44xx.begin(config);
    
    if (error != AFE44xxError::NONE) {
        Serial.print("Initialization failed: ");
        Serial.println(afe44xx.getErrorString(error));
        while (1) {
            delay(1000);
            Serial.println("Please check connections and reset");
        }
    }
    
    hrCalc.initStatHRM();
    
    Serial.println("Place finger on sensor...");
    Serial.println("SPO2\tHR\tIR\tRed\tStatus");
}

void loop() {
    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);
    
    if (error != AFE44xxError::NONE) {
        Serial.print("Read error: ");
        Serial.println(afe44xx.getErrorString(error));
        delay(100);
        return;
    }
    
    if (!data.dataValid) {
        return;
    }
    
    decimationCounter++;
    if (decimationCounter >= DECIMATION_FACTOR) {
        irBuffer[bufferIndex] = (uint16_t)(data.irData >> 4);
        redBuffer[bufferIndex] = (uint16_t)(data.redData >> 4);
        bufferIndex = (bufferIndex + 1) % 100;
        decimationCounter = 0;
        
        static uint8_t sampleCount = 0;
        sampleCount++;
        if (sampleCount >= 100) {
            spo2Calc.estimate_spo2(irBuffer, 100, redBuffer, &spo2, &spo2Valid, &heartRate, &hrValid);
            sampleCount = 0;
        
        if (spo2Valid && hrValid) {
            Serial.print(spo2);
            Serial.print("%\t");
            Serial.print(heartRate);
            Serial.print(" bpm\t");
            Serial.print(data.irData);
            Serial.print("\t");
            Serial.print(data.redData);
            Serial.println("\tValid");
        } else {
            Serial.print("--\t--\t");
            Serial.print(data.irData);
            Serial.print("\t");
            Serial.print(data.redData);
            Serial.println("\tCalculating...");
        }
        
        AFE44xxDiagnostics diag;
        error = afe44xx.getDiagnostics(diag);
        if (error == AFE44xxError::NONE && (diag.ledFault || diag.pdShort || diag.pdOpen)) {
            Serial.println("*** Sensor fault detected - check finger placement ***");
        }
    }
    
    delay(2);
}
