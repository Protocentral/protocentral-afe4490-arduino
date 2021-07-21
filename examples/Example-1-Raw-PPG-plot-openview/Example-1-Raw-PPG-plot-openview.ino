//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oxiometer Shield
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
#include "Protocentral_AFE4490_Oximeter.h"

const int SPISTE = 7; // chip select
const int SPIDRDY = 2; // data ready pin
const int PWDN =4;
const int DRDY_INTNUM =0;   //digital pin2 interrupt num = 0. Please pass correct interrupt number if you are using any boards otherthan arduino uno

int datalen = 10;

volatile char DataPacket[10];
volatile char DataPacketFooter[2]={0x00, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[6] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, datalen, ((uint8_t)(datalen >> 8)), CES_CMDIF_TYPE_DATA};

AFE4490 afe4490;

void sendDataThroughUart(afe44xx_output_values * afe4490Data){

  DataPacket[0] = afe4490Data->ir;
  DataPacket[1] = afe4490Data->ir>>8;
  DataPacket[2] = afe4490Data->ir>>16;
  DataPacket[3] = afe4490Data->ir>>24;

  DataPacket[4] = afe4490Data->red;
  DataPacket[5] = afe4490Data->red>>8;
  DataPacket[6] = afe4490Data->red>>16;
  DataPacket[7] = afe4490Data->red>>24;

  if(afe4490Data->spo2 == -999){     //-999 is the error code.
    DataPacket[8] = 0;
  }else{
    DataPacket[8] = afe4490Data->spo2;
    DataPacket[9] = afe4490Data->heart_rate;
  }

  for(int i=0;i<5;i++){
    Serial.write(DataPacketHeader[i]);
  }

  for(int i=0; i<datalen; i++){// transmit the data
    Serial.write(DataPacket[i]);
  }

  for(int i=0; i<2; i++){ // transmit the data
    Serial.write(DataPacketFooter[i]);
  }
  afe4490Data->calculated_value == false;
}

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
  Serial.println("intilazition done");
}

void loop()
{
  afe44xx_output_values afe4490Data;
  boolean sampled_value = afe4490.getDataIfAvailable(&afe4490Data,SPISTE);

  if (afe4490Data.calculated_value == true)
  {
    sendDataThroughUart(&afe4490Data);
  }
}
