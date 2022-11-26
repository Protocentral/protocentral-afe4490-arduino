//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oxiometer Shield
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

#define AFE44XX_CS_PIN   7
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_INTNUM   0

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;
int32_t heart_rate_prev=0;
int32_t spo2_prev=0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Intilaziting AFE44xx.. ");
  
  SPI.begin();
  afe44xx.afe44xx_init();
  Serial.println("Inited...");
}

void loop()
{
    delay(8);
    
    afe44xx.get_AFE44XX_Data(&afe44xx_raw_data);
        
    if (afe44xx_raw_data.buffer_count_overflow)
    {
      if(afe44xx_raw_data.spo2 == -999)
      {
        Serial.println("Probe error!!!!");
      }
      else if ((heart_rate_prev != afe44xx_raw_data.heart_rate) || (spo2_prev != afe44xx_raw_data.spo2))
      {
        heart_rate_prev = afe44xx_raw_data.heart_rate;
        spo2_prev = afe44xx_raw_data.spo2;

        Serial.print("calculating sp02...");
        Serial.print(" Sp02 : ");
        Serial.print(afe44xx_raw_data.spo2);
        Serial.print("% ,");
        Serial.print("Pulse rate :");
        Serial.print(afe44xx_raw_data.heart_rate);
        Serial.println(" bpm");
      } 
    }          
}
