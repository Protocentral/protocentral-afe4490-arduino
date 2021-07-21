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
#include "Protocentral_AFE4490_Oximeter.h"

const int SPISTE = 7; // chip select
const int SPIDRDY = 2; // data ready pin
const int PWDN =4;
const int DRDY_INTNUM =0;   //digital pin2 interrupt num = 0. Please pass correct interrupt number if you are using any boards otherthan arduino uno

AFE4490 afe4490;

int32_t heart_rate_prev=0;
int32_t spo2_prev=0;

void setup()
{
   Serial.begin(57600);
   Serial.println("Intilaziting AFE44xx.. ");

   delay(2000) ;   // pause for a moment

   SPI.begin();
   SPI.setClockDivider (SPI_CLOCK_DIV8); // set Speed as 2MHz , 16MHz/ClockDiv
   SPI.setDataMode (SPI_MODE0);          //Set SPI mode as 0
   SPI.setBitOrder (MSBFIRST);           //MSB first

   afe4490.afe44xxInit (SPISTE, SPIDRDY, DRDY_INTNUM, PWDN);
   Serial.println("intilazition is done");
}

void loop()
{
  afe44xx_output_values afe4490Data;
  boolean sampled_value = afe4490.getDataIfAvailable(&afe4490Data,SPISTE);

  if(sampled_value == true)
  {
    if(afe4490Data.spo2 == -999){

      Serial.println("Probe error!!!!");
    }else if ((heart_rate_prev != afe4490Data.heart_rate) || (spo2_prev != afe4490Data.spo2)){

      heart_rate_prev = afe4490Data.heart_rate;
      spo2_prev = afe4490Data.spo2;

      Serial.print("calculating sp02...");
      Serial.print(" Sp02 : ");
      Serial.print(afe4490Data.spo2);
      Serial.print("% ,");
      Serial.print("Pulse rate :");
      Serial.print(afe4490Data.heart_rate);
      Serial.println(" bpm");
    }
  }
}
