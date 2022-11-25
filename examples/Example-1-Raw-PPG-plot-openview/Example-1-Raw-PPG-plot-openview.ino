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
#include "protocentral_afe44xx.h"

#define AFE44XX_CS_PIN   7
#define AFE44XX_DRDY_PIN 2
#define AFE44XX_PWDN_PIN 4
#define AFE44XX_INTNUM   0

int datalen = 10;

#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA   0x02
#define CES_CMDIF_PKT_STOP    0x0B

volatile char DataPacket[10];
volatile char DataPacketFooter[2]={0x00, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[6] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, datalen, ((uint8_t)(datalen >> 8)), CES_CMDIF_TYPE_DATA};

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN, AFE44XX_INTNUM);

afe44xx_data afe44xx_raw_data;
uint8_t ppg_data_buff[20];
int16_t ppg_wave_ir;
uint16_t ppg_stream_cnt = 0;
uint8_t sp02;
bool ppg_buf_ready = false;
bool spo2_calc_done = false;

/*void sendDataThroughUart(afe44xx_output_values * afe4490Data){

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
}*/

void setup()
{
  Serial.begin(57600);
  Serial.println("Intialising AFE44xx.. ");

  delay(2000) ;   // pause for a moment

  SPI.begin();
  //SPI.setClockDivider (SPI_CLOCK_DIV8); // set Speed as 2MHz , 16MHz/ClockDiv
  //SPI.setDataMode (SPI_MODE0);          //Set SPI mode as 0
  //SPI.setBitOrder (MSBFIRST);           //MSB first

  afe44xx.afe44xx_init();
  Serial.println("intilazition done");
}

void loop()
{
    afe44xx.get_AFE44XX_Data(&afe44xx_raw_data);
    ppg_wave_ir = (int16_t)(afe44xx_raw_data.IR_data >> 8);
    ppg_wave_ir = ppg_wave_ir;

    ppg_data_buff[ppg_stream_cnt++] = (uint8_t)ppg_wave_ir;
    ppg_data_buff[ppg_stream_cnt++] = (ppg_wave_ir >> 8);

    if (ppg_stream_cnt >= 19)
    {
      ppg_buf_ready = true;
      ppg_stream_cnt = 0;
    }

    memcpy(&DataPacket[4], &afe44xx_raw_data.IR_data, sizeof(signed long));
    memcpy(&DataPacket[8], &afe44xx_raw_data.RED_data, sizeof(signed long));

    if (afe44xx_raw_data.buffer_count_overflow)
    {

      if (afe44xx_raw_data.spo2 == -999)
      {
        DataPacket[15] = 0;
        sp02 = 0;
      }
      else
      {
        DataPacket[15] = afe44xx_raw_data.spo2;
        sp02 = (uint8_t)afe44xx_raw_data.spo2;
        spo2_calc_done = true;
      }

      afe44xx_raw_data.buffer_count_overflow = false;
    }
}
