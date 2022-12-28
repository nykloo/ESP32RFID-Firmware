#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <MagicHome.h>
#include <Rfid.h>
#include <Adafruit_NeoPixel.h>
#define LED_PIN 14
#define LED_COUNT 14
#define DEVICE_NAME "x-wing"
#define KYBER_DATA_ADDRESS 0x06
#define KYBER_HEADER_ADDRESS 0x05
bool bleDeviceConnected = false;
bool bleOldDeviceConnected = false;
String ssid;     
String password; 
bool is_ap;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
const char *TAG = "MAIN";
const int lightDiscovreyInterval = 120 * 1000;
int lastDiscovery = 0;
volatile bool ledDiscoveryNeeded = false;
MagicHome LightsController;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Rfid Rfid;
Preferences preferences;

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
volatile uint32_t readAddress = KYBER_DATA_ADDRESS;
volatile uint32_t writeAddress = KYBER_DATA_ADDRESS;
uint rfid_interval_tracker_ms = 0;
void restoreSettings()
{
  while (!preferences.begin("rfid-reader", false))
  {
    delay(100);
    ESP_LOGI(TAG, "Could not load settings");
  }
  //preferences.clear();
  Serial.print(preferences.isKey("ssid"));
  if (preferences.isKey("ssid"))
  {
    ssid = preferences.getString("ssid");
  }
  else
  {
    ssid = "ESP32 RFID";
  }
  if (preferences.isKey("password"))
  {
    password = preferences.getString("password");
  }
  else
  {
    password = "123456789";
  }
  if (preferences.isKey("is_ap"))
  {
    is_ap = preferences.getBool("is_ap");
  }
  else
  {
    is_ap = true;
  }
  preferences.end();
  ESP_LOGI(TAG, "ssid: %s", ssid);
  // ESP_LOGI(TAG,"password:",password);
  ESP_LOGI(TAG, "is_ap: %d", is_ap);
  delay(1000);
}
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
// void startBle()
// {
//     BLEDevice::init("ESP32");
//   BLEServer *pServer = BLEDevice::createServer();
//    pServer->setCallbacks(new MyServerCallbacks());

//   BLEService *pService = pServer->createService(CONTROL_SERVICE_UUID);
//   RxCharacteristic = pService->createCharacteristic(
//                                          RX_CHARACTERISTIC_UUID,
//                                          BLECharacteristic::PROPERTY_READ |
//                                          BLECharacteristic::PROPERTY_WRITE
//                                        );
//   TxtifyCharacteristic = pService->createCharacteristic(
//       TX_CHARACTERISTIC_UUID,
//       BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_INDICATE);
//         BLEDescriptor *NotifyDescripton = new BLE2902(); // Characteristic User Description
//   TxtifyCharacteristic->addDescriptor(NotifyDescripton);
//   RxCharacteristic->setValue("");
//    RxCharacteristic->setCallbacks(new RxCallback());

//   //TxtifyCharacteristic->setValue("");
//   pService->start();
//   // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
//   BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
//   pAdvertising->addServiceUUID(CONTROL_SERVICE_UUID);
//   pAdvertising->setScanResponse(true);
//   pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
//   pAdvertising->setMinPreferred(0x12);
//   BLEDevice::startAdvertising();

//  }
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
  if (id == 0x00000C22) //?
  {
    setWifILights(255, 255, 255);
    setColor(255, 255, 255);
  }
  if (id == 0x00000C30) //?
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
  if (id == 0x00000C33) //?
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

void showError()
{
  delay(300);
  setColor(255, 10, 10);
  delay(300);
  setColor(0, 0, 0);
  delay(300);
  setColor(127, 10, 10);
}

void readRfid()
{
  //ESP_LOGI(TAG, "read rifd");

  RfidResult data =Rfid.ReadTag(readAddress);
  if (!data.error)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "read";
    doc["data"] = data.data;
    doc["address"] = readAddress;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
  }
  delay(250);//read 4 x a second
}
void readKyber()
{
  RfidResult data;
  data = Rfid.ReadTag(KYBER_DATA_ADDRESS);

  if (!data.error)
  {
    ESP_LOGI(TAG, "read value: %x", data.data);

    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "read_kyber";
    doc["data"] = data.data;
    doc["address"] = KYBER_DATA_ADDRESS;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
    // sendBleResponse(message.c_str());
    updateLightsById(data.data);
    delay(1000);
    setColor(0, 0, 0);
  }
}
void writeKyber()
{
  RfidResult dataCheck = Rfid.ReadTag(KYBER_DATA_ADDRESS);
  RfidResult headerCheck = Rfid.ReadTag(KYBER_HEADER_ADDRESS);
  for (int i = 0; i < 3; i++)
  {
    if (dataCheck.data != writeData || headerCheck.data != 0x1FF)
    {
      Rfid.EM4xWriteWord(KYBER_DATA_ADDRESS, writeData, 0, 0);
      delay(100);
      Rfid.EM4xWriteWord(KYBER_HEADER_ADDRESS, 0x1FF, 0, 0);
      delay(100);
      dataCheck = Rfid.ReadTag(KYBER_DATA_ADDRESS);
      headerCheck = Rfid.ReadTag(KYBER_HEADER_ADDRESS);
    }
    else
    {
      break;
    }
  }
  if (dataCheck.data == writeData && headerCheck.data == 0x1FF)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write_kyber";
    doc["address"] = KYBER_DATA_ADDRESS;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
    updateLightsById(writeData);
    delay(1000);
    setColor(10, 10, 10);
  }
  else
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write_kyber";
    doc["address"] = KYBER_DATA_ADDRESS;
    doc["error"] = true;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
    showError();
  }
}
void writeRfid()
{
  RfidResult result = Rfid.ReadTag(writeAddress);
  for (int i = 0; i < 3; i++)
  {
    if (result.data != writeData)
    {
      Rfid.EM4xWriteWord(writeAddress, writeData, 0, 0);
      delay(10);
      result = Rfid.ReadTag(writeAddress);
    }
    else
    {
      break;
    }
  }
  if (result.data == writeData)
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write";
    doc["address"] = writeAddress;
    doc["error"] = false;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);

    // sendBleResponse(message.c_str());
  }
  else
  {
    DynamicJsonDocument doc(1024);
    doc["message_type"] = "response";
    doc["response_of"] = "write";
    doc["address"] = writeAddress;
    doc["error"] = true;
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
    // sendBleResponse(message.c_str());
  }
}
void disableRfid()
{
  Rfid.Disable();
  delay(20);
}

void handleRfid()
{

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
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  ESP_LOGI(TAG, "got ws message");

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    DynamicJsonDocument doc(512);
    deserializeJson(doc, (char *)data);
    const char *message_type = doc["message_type"];
    String message_type_s = String(message_type);
    if (message_type_s == "read")
    {
      readAddress = doc["address"];
      ESP_LOGI(TAG, "Start read rfid");
      mode = read_rfid;
    }
    else if (message_type_s == "read_kyber")
    {
      readAddress = doc["address"];
      ESP_LOGI(TAG, "Start read kyber");
      mode = read_kyber;
    }
    else if (message_type_s == "write")
    {
      writeData = doc["data"];
      writeAddress = doc["address"];
      ESP_LOGI(TAG, "Start write rfid address %d  value %d", writeAddress, writeData);
      mode = write_rfid;
    }
    else if (message_type_s == "write_kyber")
    {
      writeData = doc["data"];
      ESP_LOGI(TAG, "Start write kyber");
      mode = write_kyber;
    }
    else if (message_type_s == "disable_rfid")
    {
      ESP_LOGI(TAG, "disable rfid");
      mode = disable_rfid;
    }
    else if (message_type_s == "configure_wifi")
    {
      preferences.begin("rfid-reader", false);
      const char * ssid = doc["ssid"];
      const char * password = doc["password"];
      bool is_ap = doc["is_ap"];
      ESP_LOGI(TAG, "configure wifi for %s is_ap=%d", ssid, is_ap);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.putBool("is_ap", is_ap);
      preferences.end();
      ESP_LOGI(TAG, "Restarting to apply wifi changes");
      delay(500);
      ESP.restart();
    }
    else if (message_type_s == "reset")
    {

      preferences.begin("rfid-reader", false);
      ESP_LOGI(TAG, "resetting");
      preferences.clear();
      preferences.end();
      ESP.restart();
    }
    else
    {
      ESP_LOGI(TAG, "unknown message");
      String message;
      serializeJson(doc, message);
      Serial.print(message);
    }
  }
}
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
    break;
  case WS_EVT_ERROR:
    break;
  }
}
// thing that require wifi to be up to work
void postWifiSetup()
{
  startOTA();
  LightsController.Init();
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}
void setupWifiAP()
{
  ESP_LOGI(TAG, "Setting AP (%s)…", ssid);
  WiFi.mode(WIFI_AP);

  if (WiFi.softAP(ssid.c_str(), password.c_str()))
  {
    ESP_LOGI(TAG, "Setup AP %s.", ssid);
    IPAddress IP = WiFi.softAPIP();
    ESP_LOGI(TAG, "AP IP %s.", IP.toString());

    delay(10000);
    // WiFi.onEvent(deviceConnected,ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    // LightsController.DiscoverLights();
  }
  else
  {
    ESP_LOGE(TAG, "Failed To Setup AP (%s)!", ssid);
    delay(5000);
    ESP.restart();
  }
}

void connectToWifi()
{
  ESP_LOGI(TAG, "Connecting to (%s)…", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    if(millis()-start>1000*60*1){
      ESP_LOGE(TAG, "Failed To Connectto wifi failing back to AP mode!");
      ssid = "ESP32 RFID";
      password = "123456789";
      setupWifiAP();
      return;
    }
  }
  ESP_LOGI(TAG, "Connected to (%s)…", ssid);
  delay(10000);
  //LightsController.DiscoverLights();
}

void setup()
{
  Serial.begin(115200);

  restoreSettings();
  // setColor(10,10,10);
  Rfid.Init();

  if (is_ap)
  {
    setupWifiAP();
  }
  else
  {
    connectToWifi();
  }
  postWifiSetup();

  Rfid.Enable();
}
void loop()
{
  delay(40);

  handleRfid();
  ArduinoOTA.handle();
  if (ledDiscoveryNeeded)
  {
    LightsController.DiscoverLights();
    ledDiscoveryNeeded = false;
  }
  ws.cleanupClients(3);
}
