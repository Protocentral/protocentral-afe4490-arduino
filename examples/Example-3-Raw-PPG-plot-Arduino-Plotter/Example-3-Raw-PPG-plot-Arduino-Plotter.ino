//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oximeter Shield
//
//    Copyright (c) 2018 ProtoCentral
//
//    This example will plot the PPG signal through openview processing GUI.
//    GUI URL: https://github.com/Protocentral/protocentral_openview.git
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

#include <SPI.h>
#include "protocentral_afe44xx.h"

#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_DRDY_PIN 2

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

AFE44xxRawData afe44xx_raw_data;
bool initialization_complete = false;

void setup() {
  Serial.begin(57600);
  Serial.println("Initializing AFE44xx...");
  
  SPI.begin();
  
  AFE44xxConfig config = AFE44XX::getDefaultConfig();
  AFE44xxError init_result = afe44xx.begin(config);
  
  if (init_result == AFE44xxError::NONE) {
    Serial.println("AFE44xx initialized successfully!");
    initialization_complete = true;
  } else {
    Serial.print("AFE44xx initialization failed with error code: ");
    Serial.println((int)init_result);
    while(1) delay(1000);
  }
}

void loop() {
  if (!initialization_complete) {
    delay(1000);
    return;
  }
  
  if (afe44xx.isDataReady()) {
    AFE44xxError read_result = afe44xx.readRawData(afe44xx_raw_data);
    
    if (read_result == AFE44xxError::NONE && afe44xx_raw_data.data_valid) {
      Serial.println(afe44xx_raw_data.led2_value);
      // Serial.println(afe44xx_raw_data.led1_value);
    }
  }
  
  delay(8);
}
