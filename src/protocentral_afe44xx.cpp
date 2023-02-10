//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the AFE44XX Pulse Oxiometer Shield
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use, visit https://github.com/Protocentral/AFE44XX_Oximeter
/////////////////////////////////////////////////////////////////////////////////////////



#include "protocentral_afe44xx.h"
#include "Protocentral_spo2_algorithm.h"
#include "protocentral_hr_algorithm.h"

#define AFE44XX_SPI_SPEED 2000000
SPISettings SPI_SETTINGS(AFE44XX_SPI_SPEED, MSBFIRST, SPI_MODE0); 

volatile boolean afe44xx_data_ready = false;
volatile int8_t n_buffer_count; //data length

int dec=0;

unsigned long IRtemp,REDtemp;

int32_t n_spo2;  //SPO2 value
int32_t n_heart_rate; //heart rate value

uint16_t aun_ir_buffer[100]; //infrared LED sensor data
uint16_t aun_red_buffer[100];  //red LED sensor data

int8_t ch_spo2_valid;  //indicator to show if the SPO2 calculation is valid
int8_t  ch_hr_valid;  //indicator to show if the heart rate calculation is valid

const uint8_t uch_spo2_table[184]={ 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
                                    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                   100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
                                    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
                                    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
                                    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
                                    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
                                    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
                                    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
                                    3,   2,  1  } ;

spo2_algorithm Spo2;
hr_algo hral;

AFE44XX::AFE44XX(int cs_pin, int pwdn_pin)
{
    _cs_pin=cs_pin;
    
    _pwdn_pin=pwdn_pin;

    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin,HIGH);

    pinMode (_pwdn_pin,OUTPUT);

    hral.initStatHRM();
    
    /*pinMode (_drdy_pin,INPUT);// data ready

    digitalWrite(_pwdn_pin, LOW);
    delay(500);
    digitalWrite(_pwdn_pin, HIGH);
    delay(500);
    */
}

boolean AFE44XX::get_AFE44XX_Data(afe44xx_data *afe44xx_raw_data)
{
  afe44xxWrite(CONTROL0, 0x000001);
  IRtemp = afe44xxRead(LED1VAL);
  afe44xxWrite(CONTROL0, 0x000001);
  REDtemp = afe44xxRead(LED2VAL);
  afe44xx_data_ready = true;
  IRtemp = (unsigned long) (IRtemp << 10);
  afe44xx_raw_data->IR_data = (signed long) (IRtemp);
  afe44xx_raw_data->IR_data = (signed long) ((afe44xx_raw_data->IR_data) >> 10);
  REDtemp = (unsigned long) (REDtemp << 10);
  afe44xx_raw_data->RED_data = (signed long) (REDtemp);
  afe44xx_raw_data->RED_data = (signed long) ((afe44xx_raw_data->RED_data) >> 10);

  if (dec == 20)
  {
    aun_ir_buffer[n_buffer_count] = (uint16_t) ((afe44xx_raw_data->IR_data) >> 4);
    aun_red_buffer[n_buffer_count] = (uint16_t) ((afe44xx_raw_data->RED_data) >> 4);
    n_buffer_count++;
    dec = 0;
  }

  dec++;

  if (n_buffer_count > 99)
  {
    Spo2.estimate_spo2(aun_ir_buffer, 100, aun_red_buffer, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
    afe44xx_raw_data->spo2 = n_spo2;
    //afe44xx_raw_data->heart_rate = n_heart_rate;
    n_buffer_count = 0;
    afe44xx_raw_data->buffer_count_overflow = true;
  }

  hral.statHRMAlgo(afe44xx_raw_data->RED_data);
  afe44xx_raw_data->heart_rate = hral.HeartRate;

  afe44xx_data_ready = false;
  return true;
}

void AFE44XX::afe44xx_init()
{
  digitalWrite(_pwdn_pin, LOW);
  delay(500);
  digitalWrite(_pwdn_pin, HIGH);
  delay(500);

  afe44xxWrite(CONTROL0, 0x000000);
  afe44xxWrite(CONTROL0, 0x000008);
  afe44xxWrite(TIAGAIN, 0x000000); // CF = 5pF, RF = 500kR
  afe44xxWrite(TIA_AMB_GAIN, 0x000001);
  afe44xxWrite(LEDCNTRL, 0x001414);
  afe44xxWrite(CONTROL2, 0x000000); // LED_RANGE=100mA, LED=50mA
  afe44xxWrite(CONTROL1, 0x010707); // Timers ON, average 3 samples
  afe44xxWrite(PRPCOUNT, 0X001F3F);
  afe44xxWrite(LED2STC, 0X001770);
  afe44xxWrite(LED2ENDC, 0X001F3E);
  afe44xxWrite(LED2LEDSTC, 0X001770);
  afe44xxWrite(LED2LEDENDC, 0X001F3F);
  afe44xxWrite(ALED2STC, 0X000000);
  afe44xxWrite(ALED2ENDC, 0X0007CE);
  afe44xxWrite(LED2CONVST, 0X000002);
  afe44xxWrite(LED2CONVEND, 0X0007CF);
  afe44xxWrite(ALED2CONVST, 0X0007D2);
  afe44xxWrite(ALED2CONVEND, 0X000F9F);
  afe44xxWrite(LED1STC, 0X0007D0);
  afe44xxWrite(LED1ENDC, 0X000F9E);
  afe44xxWrite(LED1LEDSTC, 0X0007D0);
  afe44xxWrite(LED1LEDENDC, 0X000F9F);
  afe44xxWrite(ALED1STC, 0X000FA0);
  afe44xxWrite(ALED1ENDC, 0X00176E);
  afe44xxWrite(LED1CONVST, 0X000FA2);
  afe44xxWrite(LED1CONVEND, 0X00176F);
  afe44xxWrite(ALED1CONVST, 0X001772);
  afe44xxWrite(ALED1CONVEND, 0X001F3F);
  afe44xxWrite(ADCRSTCNT0, 0X000000);
  afe44xxWrite(ADCRSTENDCT0, 0X000000);
  afe44xxWrite(ADCRSTCNT1, 0X0007D0);
  afe44xxWrite(ADCRSTENDCT1, 0X0007D0);
  afe44xxWrite(ADCRSTCNT2, 0X000FA0);
  afe44xxWrite(ADCRSTENDCT2, 0X000FA0);
  afe44xxWrite(ADCRSTCNT3, 0X001770);
  afe44xxWrite(ADCRSTENDCT3, 0X001770);
  delay(1000);
}

void AFE44XX :: afe44xxWrite (uint8_t address, uint32_t data)
{
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite (_cs_pin, LOW); // enable device
  SPI.transfer (address); // send address to device
  SPI.transfer ((data >> 16) & 0xFF); // write top 8 bits
  SPI.transfer ((data >> 8) & 0xFF); // write middle 8 bits
  SPI.transfer (data & 0xFF); // write bottom 8 bits
  digitalWrite (_cs_pin, HIGH); // disable device
  SPI.endTransaction();
}

unsigned long AFE44XX :: afe44xxRead (uint8_t address)
{
  unsigned long data = 0;

  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite (_cs_pin, LOW); // enable device
  SPI.transfer (address); // send address to device
  data |= ((unsigned long)SPI.transfer (0) << 16); // read top 8 bits data
  data |= ((unsigned long)SPI.transfer (0) << 8); // read middle 8 bits  data
  data |= SPI.transfer (0); // read bottom 8 bits data
  digitalWrite (_cs_pin, HIGH); // disable device
  SPI.endTransaction();

  return data; // return with 24 bits of read data
}