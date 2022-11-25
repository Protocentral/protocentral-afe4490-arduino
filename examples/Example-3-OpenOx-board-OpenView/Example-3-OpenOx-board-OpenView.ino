//////////////////////////////////////////////////////////////////////////////////////////
//
//    This example is for the new Open-Ox board.
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

#define AFE44XX_CS_PIN   25
#define AFE44XX_DRDY_PIN 13
#define AFE44XX_PWDN_PIN 4

#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA   0x02
#define CES_CMDIF_PKT_STOP    0x0B

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;
uint8_t ppg_data_buff[20];
int16_t ppg_wave_ir;
uint16_t ppg_stream_cnt = 0;
uint8_t sp02;
bool ppg_buf_ready = false;
bool spo2_calc_done = false;

#define DATA_LEN 10

char DataPacket[10];
const char DataPacketFooter[2]={0x00, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[6] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ((uint8_t)(DATA_LEN >> 8)), CES_CMDIF_TYPE_DATA};

void setup()
{
  Serial.begin(57600);
  Serial.println("Intilaziting AFE44xx.. ");
  delay(2000) ;   // pause for a moment

  SPI.begin();

  afe44xx.afe44xx_init();
  Serial.println("Inited...");
}

void send_data_serial_port(void)
{
  for (int i = 0; i < 5; i++)
  {
    Serial.write(DataPacketHeader[i]); // transmit the data over USB
  }
  for (int i = 0; i < DATA_LEN; i++)
  {
    Serial.write(DataPacket[i]); // transmit the data over USB
  }
  for (int i = 0; i < 2; i++)
  {
    Serial.write(DataPacketFooter[i]); // transmit the data over USB
  }
}

void loop()
{
    delay(8);
    
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

    memcpy(&DataPacket[0], &afe44xx_raw_data.IR_data, sizeof(signed long));
    memcpy(&DataPacket[4], &afe44xx_raw_data.RED_data, sizeof(signed long));

    if (afe44xx_raw_data.buffer_count_overflow)
    {

      if (afe44xx_raw_data.spo2 == -999)
      {
        DataPacket[8] = 0;
        sp02 = 0;
      }
      else
      {
        DataPacket[8] = afe44xx_raw_data.spo2;
        sp02 = (uint8_t)afe44xx_raw_data.spo2;
        spo2_calc_done = true;
      }

      
      afe44xx_raw_data.buffer_count_overflow = false;
    }
    send_data_serial_port();
}
