//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oxiometer Shield
//
//    Copyright (c) 2018 ProtoCentral
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
const int RESET =5; // data ready pin 
const int PWDN =4; // data ready pin

int datalen = 10;

volatile char DataPacket[10];
volatile char DataPacketFooter[2];
volatile char DataPacketHeader[6];

AFE4490 afe4490;

void sendDataThroughUart(afe44xx_output_values * afe4490Data){
  
  DataPacket[0] = afe4490Data.seeg;
  DataPacket[1] = afe4490Data.seeg>>8;
  DataPacket[2] = afe4490Data.seeg>>16;
  DataPacket[3] = afe4490Data.seeg>>24; 
 
  DataPacket[4] = afe4490Data.seeg2;
  DataPacket[5] = afe4490Data.seeg2>>8;
  DataPacket[6] = afe4490Data.seeg2>>16;
  DataPacket[7] = afe4490Data.seeg2>>24;

  if(afe4490Data.spo2 == -999){     //-999 is the error code.
    DataPacket[8] = 0;     
  }else{
    DataPacket[8] = afe4490Data.spo2; 
    DataPacket[9] = afe4490Data.heart_rate;
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
  afe4490Data.calculated_value == false;
}

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

  //Packet structure
  DataPacketHeader[0] = CES_CMDIF_PKT_START_1;  //packet header1 0x0A
  DataPacketHeader[1] = CES_CMDIF_PKT_START_2;  //packet header2 0xFA
  DataPacketHeader[2] = datalen;                // data length 9
  DataPacketHeader[3] = (uint8_t)(datalen >> 8);
  DataPacketHeader[4] = CES_CMDIF_TYPE_DATA;
  DataPacketFooter[0] = 0x00;
  DataPacketFooter[1] = CES_CMDIF_PKT_STOP;
  
  afe4490.afe44xxInit (SPISTE); 
  Serial.println("intilazition is done");
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
     
