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

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

void setup()
{
  Serial.begin(57600);
  Serial.println("Initializing AFE44xx...");

  SPI.begin();

  AFE44xxConfig cfg = AFE44XX::getDefaultConfig();
  AFE44xxError err = afe44xx.begin(cfg);
  if (err != AFE44xxError::NONE) {
    Serial.print("Init failed: ");
    Serial.println(afe44xx.getErrorString(err));
    while (1) delay(1000);
  }

  Serial.println("Inited...");
}

void loop()
{
  AFE44xxData data;
  AFE44xxError err = afe44xx.readData(data);
  if (err == AFE44xxError::NONE && data.dataValid) {
    Serial.println(data.redData);
    // Serial.println(data.irData);
  }
  delay(8);
}
