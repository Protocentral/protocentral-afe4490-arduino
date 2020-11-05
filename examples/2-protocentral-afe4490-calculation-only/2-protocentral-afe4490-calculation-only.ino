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
const int RESET =5; // data ready pin 
const int PWDN =4; // data ready pin

int datalen = 10;

AFE4490 afe4490;

void setup()
{
   Serial.begin(57600);
   Serial.println("Intilazition AFE44xx.. ");   
   
   delay(2000) ;   // pause for a moment
  
   SPI.begin(); 
   
   // set the directions
   pinMode (SPISTE,OUTPUT);//Slave Select
   pinMode (SPIDRDY,INPUT);// data ready 
 
   attachInterrupt(0, afe44xx_drdy_event, RISING ); // Digital2 is attached to Data ready pin of AFE is interrupt0 in ARduino
   // set SPI transmission
   SPI.setClockDivider (SPI_CLOCK_DIV8); // set Speed as 2MHz , 16MHz/ClockDiv
   SPI.setDataMode (SPI_MODE0);          //Set SPI mode as 0
   SPI.setBitOrder (MSBFIRST);           //MSB first
   
   afe4490.afe44xxInit (SPISTE); 
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
    }else{

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
     