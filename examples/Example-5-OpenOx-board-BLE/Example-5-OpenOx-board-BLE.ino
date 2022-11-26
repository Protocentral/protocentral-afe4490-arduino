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

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define Heartrate_SERVICE_UUID (uint16_t(0x180D))
#define Heartrate_CHARACTERISTIC_UUID (uint16_t(0x2A37))
#define sp02_SERVICE_UUID (uint16_t(0x1822))
#define sp02_CHARACTERISTIC_UUID (uint16_t(0x2A5E))
#define HRV_SERVICE_UUID "cd5c7491-4448-7db8-ae4c-d1da8cba36d0"
#define HIST_CHARACTERISTIC_UUID "cd5c1525-4448-7db8-ae4c-d1da8cba36d0"
#define DATASTREAM_SERVICE_UUID (uint16_t(0x1122))
#define DATASTREAM_CHARACTERISTIC_UUID (uint16_t(0x1424))

#define AFE44XX_CS_PIN   25
#define AFE44XX_DRDY_PIN 13
#define AFE44XX_PWDN_PIN 4

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;
uint8_t ppg_data_buff[20];
int16_t ppg_wave_ir;
uint16_t ppg_stream_cnt = 0;
uint8_t sp02;
uint8_t heartrate;
bool ppg_buf_ready = false;
bool spo2_calc_done = false;
bool heartrate_calc_done = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;

String strValue = "";

BLEServer *pServer = NULL;
BLECharacteristic *Heartrate_Characteristic = NULL;
BLECharacteristic *sp02_Characteristic = NULL;
BLECharacteristic *hist_Characteristic = NULL;
BLECharacteristic *hrv_Characteristic = NULL;
BLECharacteristic *datastream_Characteristic = NULL;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("connected");
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("disconnected");
  }
};

class MyCallbackHandler : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *datastream_Characteristic)
  {
    std::string value = datastream_Characteristic->getValue();
    int len = value.length();
    strValue = "0";

    if (value.length() > 0)
    {
      Serial.print("New value: ");

      for (int i = 0; i < value.length(); i++)
      {
        Serial.print(String(value[i]));
        strValue += value[i];
      }

      Serial.println();
    }
  }
};

void OpenOx_BLE_Init()
{
  BLEDevice::init("OpenOx");     // Create the BLE Device
  pServer = BLEDevice::createServer(); // Create the BLE Server
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *HeartrateService = pServer->createService(Heartrate_SERVICE_UUID); // Create the BLE Service
  BLEService *sp02Service = pServer->createService(sp02_SERVICE_UUID);           // Create the BLE Service
  BLEService *hrvService = pServer->createService(HRV_SERVICE_UUID);
  BLEService *datastreamService = pServer->createService(DATASTREAM_SERVICE_UUID);

  Heartrate_Characteristic = HeartrateService->createCharacteristic(
      Heartrate_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);

  sp02_Characteristic = sp02Service->createCharacteristic(
      sp02_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);
 
  hist_Characteristic = hrvService->createCharacteristic(
      HIST_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);   

  datastream_Characteristic = datastreamService->createCharacteristic(
      DATASTREAM_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);  

  Heartrate_Characteristic->addDescriptor(new BLE2902());
  sp02_Characteristic->addDescriptor(new BLE2902());     
  hist_Characteristic->addDescriptor(new BLE2902());
  datastream_Characteristic->addDescriptor(new BLE2902());
  datastream_Characteristic->setCallbacks(new MyCallbackHandler());

  // Start the service
  HeartrateService->start();
  sp02Service->start();
  hrvService->start(); 
  datastreamService->start(); 

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(Heartrate_SERVICE_UUID);
  pAdvertising->addServiceUUID(sp02_SERVICE_UUID);
  pAdvertising->addServiceUUID(HRV_SERVICE_UUID);
  pAdvertising->addServiceUUID(DATASTREAM_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00); // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void handle_ble_stack()
{

   
 if (ppg_buf_ready)
  {
    ppg_buf_ready = false;
    hist_Characteristic->setValue(ppg_data_buff, 19);
    hist_Characteristic->notify();
  }

  if (spo2_calc_done)
  {
    // afe44xx_raw_data.buffer_count_overflow = false;
    uint8_t spo2_att_ble[5];
    spo2_att_ble[0] = 0x00;
    spo2_att_ble[1] = (uint8_t)sp02;
    spo2_att_ble[2] = (uint8_t)(sp02 >> 8);
    spo2_att_ble[3] = (uint8_t)heartrate;;
    spo2_att_ble[4] = (uint8_t)(heartrate >> 8);
    sp02_Characteristic->setValue(spo2_att_ble, 5);
    sp02_Characteristic->notify();
    spo2_calc_done = false;
  }

if (heartrate_calc_done)
  {
    // afe44xx_raw_data.buffer_count_overflow = false;
    uint8_t heartrate_att_ble[5];
    heartrate_att_ble[0] = 0x00;
    heartrate_att_ble[1] = (uint8_t)heartrate;
    heartrate_att_ble[2] = (uint8_t)(heartrate >> 8);
    Heartrate_Characteristic->setValue(heartrate_att_ble, 3);
    Heartrate_Characteristic->notify();
    heartrate_calc_done = false;
  }  

   if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }

   // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

}
  
    
void setup()
{
  Serial.begin(57600);
  Serial.println("Intilaziting AFE44xx.. ");
  delay(2000) ;   // pause for a moment
  
  OpenOx_BLE_Init();
  SPI.begin();

  afe44xx.afe44xx_init();
  Serial.println("Inited...");
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

    if (afe44xx_raw_data.buffer_count_overflow)
    {

      if (afe44xx_raw_data.spo2 == -999)
      {
        sp02 = 0;
        heartrate = 0;
      }
      else
      {
        sp02 = (uint8_t)afe44xx_raw_data.spo2;
        spo2_calc_done = true;
        heartrate = (uint8_t) afe44xx_raw_data.heart_rate;
        heartrate_calc_done = true;
      }

      
      afe44xx_raw_data.buffer_count_overflow = false;
    }
    handle_ble_stack();
}
