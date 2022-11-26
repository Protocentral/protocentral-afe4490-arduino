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
#define AFE44XX_INTNUM   0

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;

void setup()
{
  Serial.begin(57600);
  Serial.println("Intilaziting AFE44xx.. ");
  
  SPI.begin();
  afe44xx.afe44xx_init();
  Serial.println("Inited...");
}

void loop()
{
  afe44xx.get_AFE44XX_Data(&afe44xx_raw_data);
    
  Serial.println(afe44xx_raw_data.RED_data);  
  // Serial.println(afe44xx_raw_data.IR_data);  
  delay(8); 

}
