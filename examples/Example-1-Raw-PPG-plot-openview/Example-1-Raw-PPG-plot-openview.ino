//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE4490 Pulse Oximeter Shield - Improved Example
//
//    Copyright (c) 2018 ProtoCentral
//
//    This example demonstrates the improved AFE44XX library with better error handling
//    and configurable parameters. Plot PPG signal through openview processing GUI.
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
#define AFE44XX_DRDY_PIN 2

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN, AFE44XX_DRDY_PIN);

AFE44xxRawData afe44xx_raw_data;
AFE44xxPPGData afe44xx_ppg_data;
uint8_t ppg_data_buff[20];
int16_t ppg_wave_ir;
uint16_t ppg_stream_cnt = 0;
uint8_t spo2_value;
uint8_t heartrate_value;
bool ppg_buf_ready = false;
bool initialization_complete = false;

#define DATA_LEN 10

char DataPacket[10];
const char DataPacketFooter[2] = {0x00, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[6] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ((uint8_t)(DATA_LEN >> 8)), CES_CMDIF_TYPE_DATA};

void setup()
{
  Serial.begin(57600);
  Serial.println("Initializing AFE44xx...");

  SPI.begin();

  AFE44xxConfig config = AFE44XX::getDefaultConfig();
  config.led1_current = AFE44xxLEDCurrent::LED_50MA;
  config.led2_current = AFE44xxLEDCurrent::LED_50MA;
  config.tia_gain = AFE44xxTIAGain::TIA_500K;
  config.adc_averaging = AFE44xxADCAverage::AVERAGE_8;

  AFE44xxError init_result = afe44xx.begin(config);

  if (init_result == AFE44xxError::NONE)
  {
    Serial.println("AFE44xx initialized successfully!");
    initialization_complete = true;
  }
  else
  {
    Serial.print("AFE44xx initialization failed with error code: ");
    Serial.println((int)init_result);
    Serial.println("Check connections and try again.");
    while (1)
    {
      delay(1000);
    }
  }
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
  if (!initialization_complete)
  {
    delay(1000);
    return;
  }

  delay(8);

  if (afe44xx.isDataReady())
  {
    AFE44xxError read_result = afe44xx.readRawData(afe44xx_raw_data);
    AFE44xxError ppg_result = afe44xx.readPPGData(afe44xx_ppg_data);

    if (read_result == AFE44xxError::NONE && afe44xx_raw_data.data_valid)
    {
      ppg_wave_ir = (int16_t)(afe44xx_raw_data.led1_value >> 8);

      ppg_data_buff[ppg_stream_cnt++] = (uint8_t)ppg_wave_ir;
      ppg_data_buff[ppg_stream_cnt++] = (ppg_wave_ir >> 8);

      if (ppg_stream_cnt >= 19)
      {
        ppg_buf_ready = true;
        ppg_stream_cnt = 0;
      }

      memcpy(&DataPacket[0], &afe44xx_raw_data.led1_value, sizeof(int32_t));
      memcpy(&DataPacket[4], &afe44xx_raw_data.led2_value, sizeof(int32_t));

      if (ppg_result != AFE44xxError::NONE || afe44xx_ppg_data.spo2_percent <= 0)
      {
        DataPacket[8] = 0;
        spo2_value = 0;
        DataPacket[9] = 0;
        heartrate_value = 0;
      }
      else
      {
        DataPacket[8] = (uint8_t)afe44xx_ppg_data.spo2_percent;
        spo2_value = (uint8_t)afe44xx_ppg_data.spo2_percent;
        DataPacket[9] = (uint8_t)afe44xx_ppg_data.heart_rate_bpm;
        heartrate_value = (uint8_t)afe44xx_ppg_data.heart_rate_bpm;
      }

      send_data_serial_port();
    }
    else
    {
      Serial.print("Data read error: ");
      Serial.println((int)read_result);
    }
  }
}
