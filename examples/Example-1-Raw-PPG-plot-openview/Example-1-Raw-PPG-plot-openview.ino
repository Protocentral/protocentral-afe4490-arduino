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

// Default pins used by this example
#define AFE44XX_CS_PIN 7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    Serial.println("AFE44XX Raw PPG Plot - Example");

    AFE44xxConfig config = AFE44XX::getDefaultConfig();
    config.led1Current = LEDCurrent::CURRENT_50MA;
    config.led2Current = LEDCurrent::CURRENT_50MA;
    config.tiaGain = TIAGain::GAIN_500K;
    config.sampleRate = SampleRate::RATE_500HZ;
    config.enableDiagnostics = true;

    Serial.println("Initializing AFE44XX...");
    AFE44xxError error = afe44xx.begin(config);
    if (error != AFE44xxError::NONE)
    {
        Serial.print("Initialization failed: ");
        Serial.println(afe44xx.getErrorString(error));
        while (1)
            delay(1000);
    }

    // Optional: run self-test
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

    Serial.println("Streaming raw PPG data (IR,RED) as CSV");
    Serial.println("IR,RED");
}

void loop()
{
    AFE44xxData data;
    AFE44xxError error = afe44xx.readData(data);
    if (error != AFE44xxError::NONE)
    {
        Serial.print("Read error: ");
        Serial.println(afe44xx.getErrorString(error));
        delay(50);
        return;
    }

    if (data.dataValid)
    {
        // Output CSV for plotters or post-processing tools
        Serial.print(data.irData);
        Serial.print(",");
        Serial.println(data.redData);
    }

    delay(10);
}
