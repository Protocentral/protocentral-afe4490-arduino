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
#define AFE44XX_DRDY_PIN 2
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_INTNUM   0

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN, AFE44XX_INTNUM);

int32_t heart_rate_prev=0;
int32_t spo2_prev=0;

void setup()
{
    Serial.begin(57600);
    Serial.println("Intilaziting AFE44xx.. ");

    delay(2000) ;   // pause for a moment

    SPI.begin();
   //SPI.setClockDivider (SPI_CLOCK_DIV8); // set Speed as 2MHz , 16MHz/ClockDiv
   //SPI.setDataMode (SPI_MODE0);          //Set SPI mode as 0
   //SPI.setBitOrder (MSBFIRST);           //MSB first

    afe44xx.afe44xx_init();
    Serial.println("Init complete");
}

void loop()
{
    afe44xx_output_values afe44xxData;
    boolean sampled_value = afe44xx.getDataIfAvailable(&afe44xxData);

    if(sampled_value == true)
    {
      if(afe44xxData.spo2 == -999){

        Serial.println("Probe error!!!!");
      }else if ((heart_rate_prev != afe44xxData.heart_rate) || (spo2_prev != afe44xxData.spo2)){

        heart_rate_prev = afe44xxData.heart_rate;
        spo2_prev = afe44xxData.spo2;

        Serial.print("calculating sp02...");
        Serial.print(" Sp02 : ");
        Serial.print(afe44xxData.spo2);
        Serial.print("% ,");
        Serial.print("Pulse rate :");
        Serial.print(afe44xxData.heart_rate);
        Serial.println(" bpm");
      }
    }
}
