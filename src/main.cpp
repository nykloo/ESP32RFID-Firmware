#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <MagicHome.h>
#include <Rfid.h>
#include <Adafruit_NeoPixel.h>
#define LED_PIN 14
#define LED_COUNT 14
#define CONTROL_SERVICE_UUID "e63d24ed-6bc9-483a-b8ca-54a4ac05643f"
#define RX_CHARACTERISTIC_UUID "65b41553-acb3-4321-96d6-a2958cd3920f"
#define TX_CHARACTERISTIC_UUID "2ac96011-6181-447d-b4e7-1ae6c8394e59"
#define DEVICE_NAME "x-wing"
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *RxCharacteristic;
BLECharacteristic *TxtifyCharacteristic;
BLEAdvertising *pAdvertising;
bool bleDeviceConnected = false;
bool bleOldDeviceConnected = false;
const char *ssid = "Kyber Nexus";
const char *password = "123456789";
const char *TAG = "MAIN";
const int lightDiscovreyInterval = 120 * 1000;
int lastDiscovery = 0;
volatile bool ledDiscoveryNeeded = false;
MagicHome LightsController;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Rfid Rfid;

enum Mode
{
  read_rfid,
  read_kyber,
  write_rfid,
  write_kyber,
  disable_rfid
};

volatile Mode mode = read_kyber;
volatile uint32_t writeData = 0x3E183000;
volatile uint32_t readAddress = 6;
volatile uint32_t writeAddress = 6;
volatile uint32_t interval = 5;
uint rfid_interval_tracker_ms=0;

/*
Commands
//reads a value from an address
{
  message_type:"read",
  address:int(1-15),
  interval: int, seconds //default 0 reads once never repeats
}
//reads address 6 and set lights to match kybers
{
  message_type:"read_kyber"
  interval: int, seconds //default 0 reads once never repeats

}
{
  message_type:"write"
  address:int(1-15),
  data:uint32,
  interval: int, seconds //default 0 writes once never repeats
}
//reads current value if not equal to data
//writes 0x1ff header to address 5 and data to 6
//reads to verify
{
  message_type:"write_kyber"
  data:uint32,
  interval: int, seconds //default 0 writes once never repeats
}
{
  message_type: "disable_rfid",
}
//
{
  message_type: "response",
  response_of:{initiateing message type},
  data: uint32,optional
  error: boolean

}


//example write 0x0C803000
{
  message_type:"write_kyber"
  data:209727488,
  interval: 0
}
*/

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    bleDeviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    bleDeviceConnected = false;
  }
};
class RxCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();
    // read data as byte pCharacteristic->getData() pCharacteristic instead of   pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
      Serial.println("*********");
      Serial.print("Characteristic Uuid ");
      std::string id = pCharacteristic->getUUID().toString();
      for (int i = 0; i < id.length(); i++)
        Serial.print(id[i]);

      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);

      Serial.println();
      Serial.println("*********");
      DynamicJsonDocument doc(512);
      deserializeJson(doc, rxValue);

      const char *message_type = doc["message_type"];
      String message_type_s = String(message_type);

      if (message_type_s = "read")
      {
        readAddress = doc["address"];
        interval = doc["interval"];
        mode = read_rfid;
      }
      if (message_type_s = "read_kyber")
      {
        readAddress = doc["address"];
        interval = doc["interval"];
        mode = read_kyber;
      }
      if (message_type_s = "write")
      {
        interval = doc["interval"];
        writeData = doc["data"];
        mode = write_rfid;
      }
      if (message_type_s = "write_kyber")
      {
        interval = doc["interval"];
        writeData = doc["data"];
        mode = write_kyber;
      }
      if (message_type_s = "disable_rfid")
      {
        mode = disable_rfid;
      }
    }
  }
};
void setColor(byte r, byte g, byte b)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}
void deviceConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  ledDiscoveryNeeded = true;
}
void startOTA()
{
  // upload with
  //  pio run -t upload --upload-port myesp32.local
  //  or set config https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("kyber");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      ESP_LOGI(TAG,"Start updating %s", type); })
      .onEnd([]()
             { ESP_LOGI(TAG, "\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { ESP_LOGI(TAG, "Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      ESP_LOGI(TAG,"Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) ESP_LOGI(TAG,"Auth Failed");
      else if (error == OTA_BEGIN_ERROR) ESP_LOGI(TAG,"Begin Failed");
      else if (error == OTA_CONNECT_ERROR) ESP_LOGI(TAG,"Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) ESP_LOGI(TAG,"Receive Failed");
      else if (error == OTA_END_ERROR) ESP_LOGI(TAG,"End Failed"); });
  ArduinoOTA.begin();
}
void startBle()
{
    BLEDevice::init("Long name works now");
  BLEServer *pServer = BLEDevice::createServer();
   pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(CONTROL_SERVICE_UUID);
  RxCharacteristic = pService->createCharacteristic(
                                         RX_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  TxtifyCharacteristic = pService->createCharacteristic(
      TX_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_INDICATE);
        BLEDescriptor *NotifyDescripton = new BLE2902(); // Characteristic User Description
  TxtifyCharacteristic->addDescriptor(NotifyDescripton);
  RxCharacteristic->setValue("");
   RxCharacteristic->setCallbacks(new RxCallback());

  //TxtifyCharacteristic->setValue("");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(CONTROL_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

//   BLEDevice::init(DEVICE_NAME);

//   ESP_LOGI(TAG, "Starting BLE");
//   pServer = BLEDevice::createServer();
//   ESP_LOGI(TAG, "Created BLE server");

//   pService = pServer->createService(CONTROL_SERVICE_UUID);
//   ESP_LOGI(TAG, "Created BLE service");

//   ESP_LOGI(TAG, "set server callbacks");

//   RxCharacteristic = pService->createCharacteristic(
//       RX_CHARACTERISTIC_UUID,
//       BLECharacteristic::PROPERTY_READ |
//           BLECharacteristic::PROPERTY_WRITE);

//   TxtifyCharacteristic = pService->createCharacteristic(
//       TX_CHARACTERISTIC_UUID,
//       BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_INDICATE);
//   BLEDescriptor *RxDescription = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
//   RxDescription->setValue("WriteMe");

//   BLEDescriptor *NotifyDescripton = new BLE2902(); // Characteristic User Description
//  TxtifyCharacteristic->addDescriptor(NotifyDescripton);
//   ESP_LOGI(TAG, "created charaateristics");

//   pService->start();
//   ESP_LOGI(TAG, "servuce started");

//   // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
//   pAdvertising = BLEDevice::getAdvertising();
//   pAdvertising->addServiceUUID(CONTROL_SERVICE_UUID);
//   //pAdvertising->setScanResponse(true);
//   //pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
//   //pAdvertising->setMinPreferred(0x12);
//   BLEDevice::startAdvertising();

//   ESP_LOGI(TAG, "BLE started");
 }
void setWifILights(byte r, byte g, byte b)
{
  for (auto &light : LightsController.DiscoveredLights())
  {
    setWifILights(r, g, b);
  }
}
void updateLightsById(uint32_t id)
{

  if (id == 0x18003000) // white ashoka
  {
    setWifILights(255, 255, 255);
    setColor(255, 255, 255);
  }
  if (id == 14403000) // Chirutt
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
  if (id == 0x00000C02) //?
  {
    setWifILights(255, 255, 0);
    setColor(255, 255, 0);
  }
  if (id == 0x7B003000) // temple gaurd
  {
    setWifILights(255, 255, 0);
    setColor(255, 255, 0);
  }
  if (id == 0x0C803000) // green
  {
    setWifILights(0, 255, 0);
    setColor(0, 255, 0);
  }
  if (id == 0x00000C05) //?
  {
    setWifILights(0, 0, 255);
    setColor(0, 0, 255);
  }
  if (id == 0x29803000) // 
  {
    setWifILights(0, 0, 255);
    setColor(0, 0, 255);
  }
  if (id == 0x6F803000) // 
  {
    setWifILights(106, 13, 173);
    setColor(106, 13, 173);
  }
  if (id == 0x14403000) // 
  {
    setWifILights(255, 255, 255);
    setColor(255, 255, 255);
  }
  if (id == 0x52403000)
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
  if (id == 0x31403000)
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
  if (id == 0x77403000)
  {
    setWifILights(255, 255, 0);
    setColor(255, 255, 0);
  }
  if (id == 0x00C03000)
  {
    setWifILights(0, 255, 0);
    setColor(0, 255, 0);
  }
  if (id == 0x46C03000)
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
  if (id == 0x25C03000)
  {
    setWifILights(0, 0, 255);
    setColor(0, 0, 255);
  }
  if (id == 0x63C03000)
  {
    setWifILights(106, 13, 173);
    setColor(106, 13, 173);
  }
  if (id == 0x00000C22)//?
  {
    setWifILights(255, 255, 255);
    setColor(255, 255, 255);
  }
  if (id == 0x00000C30)//?
  {
    setWifILights(255, 255, 255);
    setColor(255, 255, 255);
  }
  if (id == 0x3E183000)
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
  if (id == 0x5D183000)
  {
    setWifILights(0, 255, 0);
    setColor(0, 255, 0);
  }
  if (id == 0x00000C33)//?
  {
    setWifILights(255, 0, 0);
    setColor(255, 0, 0);
  }
}

void handleLightDiscovery()
{
  if (millis() > lastDiscovery + lightDiscovreyInterval)
  {
    ESP_LOGI(TAG, "%i,%i", lastDiscovery + lightDiscovreyInterval, millis());
    LightsController.DiscoverLights();
    lastDiscovery = millis();
  }
}
void sendBleResponse(const char * message){
  if(bleDeviceConnected){
    ESP_LOGI(TAG,"INDICATING BLE");
    TxtifyCharacteristic->setValue(message);
    TxtifyCharacteristic->indicate();
  }
}
void readRfid()
{
    ESP_LOGI(TAG,"read rifd");

  uint32_t data = 0;
  Rfid.ReadTag(readAddress);
  if (data != 0)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "read";
    doc["data"] = data;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    //sendBleResponse(message.c_str());
    sendBleResponse("{test:test,:Tester:tester,asdfasdfgsdf:adfasdfasff}");
  }
}
void readKyber()
{
    //ESP_LOGI(TAG,"read kyber");

  uint32_t data = 0;
  data = Rfid.ReadTag(readAddress);

  if (data != 0)
  {
    ESP_LOGI(TAG,"read value: %x",data);

    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "read";
    doc["data"] = data;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    sendBleResponse(message.c_str());
    updateLightsById(data);
  }
  
}
void writeKyber()
{
  uint32_t dataCheck =Rfid.ReadTag(0x06);
  uint32_t headerCheck =Rfid.ReadTag(0x05);
  for(int i=0;i<3;i++){
    if(dataCheck!=writeData||headerCheck!=0x1FF){
      Rfid.EM4xWriteWord( 0x06,writeData,0,0);
      delay(10);
      Rfid.EM4xWriteWord( 0x05,0x1FF,0,0);
      delay(10);
      dataCheck =Rfid.ReadTag(0x06);
      headerCheck =Rfid.ReadTag(0x05);
    }else{
      break;
    }
  }
  if (dataCheck==writeData&&headerCheck==0x1FF)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write_kyber";
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    sendBleResponse(message.c_str());
  }else{
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write";
    doc["error"] = true;
    String message;
    serializeJson(doc, message);
    sendBleResponse(message.c_str());
  }
}
void writeRfid()
{
  uint32_t result =Rfid.ReadTag(writeAddress);
  for(int i=0;i<3;i++){
    if(result!=writeData){
      Rfid.EM4xWriteWord( writeAddress,writeData,0,0);
      delay(10);
      result =Rfid.ReadTag(writeAddress);
    }else{
      break;
    }
  }
  if (result == writeData)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write";
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    sendBleResponse(message.c_str());
  }else{
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write";
    doc["error"] = true;
    String message;
    serializeJson(doc, message);
    sendBleResponse(message.c_str());
  }
}
void disableRfid()
{
  Rfid.Disable();
}

void handleRfid()
{
  //if(millis()-rfid_interval_tracker_ms>(interval*1000)){
//    ESP_LOGI(TAG,"handle rfid");
  switch (mode)
  {
  case read_rfid:
    readRfid();
    break;
  case read_kyber:
    readKyber();
    break;
  case write_kyber:
    writeKyber();
    break;
  case write_rfid:
    writeRfid();
    break;
  case disable_rfid:
    disableRfid();
    break;
  }
  if(interval==0){
    interval=42949; // zero is run once set it big after it runs 
  }
  interval;
 // rfid_interval_tracker_ms=millis();

  }

//}
void setup()
{

  Rfid.Init();

  startBle();
  Serial.begin(115200);
  // ESP_LOGI(TAG, "Setting AP (%s)…", ssid);
  //  if (WiFi.softAP(ssid, password))
  //  {
  //    ESP_LOGI(TAG, "Setup AP %s.", ssid);
  //    IPAddress IP = WiFi.softAPIP();
  //    ESP_LOGI(TAG, "AP IP %s.", IP.toString());
  //    startOTA();
  //    LightsController.Init();
  //    delay(10000);
  //    WiFi.onEvent(deviceConnected,ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
  //    //LightsController.DiscoverLights();

  // }
  // else
  // {
  //   ESP_LOGE(TAG, "Failed To Setup AP (%s)!", ssid);
  //   delay(5000);
  //   ESP.restart();
  // }
  Serial.begin(115200);
  Rfid.Enable();
}
void loop()
{
  delay(50);

    handleRfid();
    //ArduinoOTA.handle();
    if(ledDiscoveryNeeded){
    LightsController.DiscoverLights();
    ledDiscoveryNeeded=false;
    }
}
