//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE44XX Pulse Oximeter Shield - Raw PPG Plot for OpenView
//    Updated to use the new improved library with error handling and diagnostics
//
//    This example will plot the PPG signal through ProtoCentral OpenView processing GUI.
//    GUI URL: https://github.com/Protocentral/protocentral_openview.git
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
//    For information on how to use with ProtoCentral OpenView:
//    1. Upload this sketch to your Arduino
//    2. Open ProtoCentral OpenView software
//    3. Select the appropriate COM port
//    4. Choose "AFE4490 Raw PPG" visualization mode
//    5. Click Connect to view real-time PPG waveforms
//
//    For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_afe44xx.h"
#include "Protocentral_spo2_algorithm.h"
#include "protocentral_hr_algorithm.h"

// Default pins  CS=7, PWDN=4, DRDY=2

#define AFE44XX_CS_PIN 10
#define AFE44XX_PWDN_PIN 21
#define AFE44XX_DRDY_PIN 14

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 10

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

uint8_t ppgDataBuff[20];
uint16_t ppgStreamCnt = 0;
bool ppgBufReady = false;
bool sensorInitialized = false;

char dataPacket[DATA_LEN];
const char dataPacketFooter[2] = {0x00, CES_CMDIF_PKT_STOP};
const char dataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN,
                                  ((uint8_t)(DATA_LEN >> 8)), CES_CMDIF_TYPE_DATA};

void setup()
{
    Serial.begin(57600);
    while (!Serial)
        delay(10);

    Serial.println("AFE44XX Raw PPG Data for ProtoCentral OpenView");
    Serial.println("==============================================");

    AFE44xxConfig config = AFE44XX::getDefaultConfig();
    config.chipType = AFE44xxChipType::AFE4490;
    config.led1Current = LEDCurrent::CURRENT_50MA;
    config.led2Current = LEDCurrent::CURRENT_50MA;
    config.tiaGain = TIAGain::GAIN_500K;
    config.sampleRate = SampleRate::RATE_500HZ;
    config.averagingFactor = 3;
    config.enableDiagnostics = true;

    Serial.println("Initializing AFE44XX...");
    AFE44xxError error = afe44xx.begin(config);

    if (error != AFE44xxError::NONE)
    {
        Serial.print("ERROR: Initialization failed - ");
        Serial.println(afe44xx.getErrorString(error));
        Serial.println("Please check connections and reset Arduino");

        while (1)
        {
            delay(5000);
            Serial.println("Check connections: CS=7, PWDN=4, DRDY=2, SPI pins");
        }
    }

    error = afe44xx.performSelfTest();
    if (error != AFE44xxError::NONE)
    {
        Serial.print("WARNING: Self-test failed - ");
        Serial.println(afe44xx.getErrorString(error));
        Serial.println("Continuing anyway...");
    }
    else
    {
        Serial.println("Self-test passed!");
    }

    hrCalc.initStatHRM();
    sensorInitialized = true;

    Serial.println("Ready! Open ProtoCentral OpenView and connect to this COM port");
    Serial.println("Select 'AFE4490 Raw PPG' mode in OpenView");

    delay(2000);
}

void loop()
{
    if (!sensorInitialized)
    {
        delay(1000);
        return;
    }

    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);

    if (error != AFE44xxError::NONE)
    {
        initializeErrorPacket();
        sendDataSerialPort();
        delay(100);
        return;
    }

    if (!data.dataValid)
    {
        delay(8);
        return;
    }

    processSpO2Calculation(data);

    prepareDataPacket(data);

    sendDataSerialPort();

    static unsigned long lastDiagTime = 0;
    if (millis() - lastDiagTime > 10000)
    {
        checkSensorDiagnostics();
        lastDiagTime = millis();
    }

    delay(8);
}

void processSpO2Calculation(const AFE44xxData &data)
{
    decimationCounter++;
    if (decimationCounter >= DECIMATION_FACTOR)
    {
        irBuffer[bufferIndex] = (uint16_t)(data.irData >> 4);
        redBuffer[bufferIndex] = (uint16_t)(data.redData >> 4);
        bufferIndex = (bufferIndex + 1) % 100;
        decimationCounter = 0;
        
        static uint8_t sampleCount = 0;
        sampleCount++;
        if (sampleCount >= 100) {
            spo2Calc.estimate_spo2(irBuffer, 100, redBuffer, &spo2, &spo2Valid, &heartRate, &hrValid);
            sampleCount = 0;
        }
    }

    hrCalc.statHRMAlgo(data.redData);
    if (hrCalc.HeartRate > 0)
    {
        heartRate = hrCalc.HeartRate;
        hrValid = 1;
    }
}

void prepareDataPacket(const AFE44xxData &data)
{
    memcpy(&dataPacket[0], &data.irData, sizeof(int32_t));
    memcpy(&dataPacket[4], &data.redData, sizeof(int32_t));

    if (spo2Valid && spo2 > 0)
    {
        dataPacket[8] = (uint8_t)constrain(spo2, 0, 100);
    }
    else
    {
        dataPacket[8] = 0;
    }

    if (hrValid && heartRate > 0)
    {
        dataPacket[9] = (uint8_t)constrain(heartRate, 0, 255);
    }
    else
    {
        dataPacket[9] = 0;
    }
}

void initializeErrorPacket()
{
    memset(dataPacket, 0, DATA_LEN);
}

void sendDataSerialPort()
{
    uint8_t fullPacket[5 + DATA_LEN + 2];
    uint8_t idx = 0;
    
    memcpy(&fullPacket[idx], dataPacketHeader, 5);
    idx += 5;
    memcpy(&fullPacket[idx], dataPacket, DATA_LEN);
    idx += DATA_LEN;
    memcpy(&fullPacket[idx], dataPacketFooter, 2);
    
    Serial.write(fullPacket, sizeof(fullPacket));
}

void checkSensorDiagnostics()
{
    AFE44xxDiagnostics diag;
    AFE44xxError error = afe44xx.getDiagnostics(diag);

    if (error != AFE44xxError::NONE)
    {
        return;
    }

    if (diag.ledFault || diag.pdShort || diag.pdOpen || diag.gainError)
    {
        afe44xx.clearFaults();
    }

    if (diag.ambientLight > 2000)
    {
        afe44xx.setLEDCurrent(1, LEDCurrent::CURRENT_75MA);
        afe44xx.setLEDCurrent(2, LEDCurrent::CURRENT_75MA);
    }
    else if (diag.ambientLight < 100)
    {
        afe44xx.setLEDCurrent(1, LEDCurrent::CURRENT_25MA);
        afe44xx.setLEDCurrent(2, LEDCurrent::CURRENT_25MA);
    }
}