#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <MagicHome.h>
#include <Rfid.h>
#include <Adafruit_NeoPixel.h>
#define LED_PIN 14
#define LED_COUNT 3
const char *ssid = "Kyber Nexus";
const char *password = "123456789";
const char *TAG = "MAIN";
const int lightDiscovreyInterval = 120 * 1000;
int lastDiscovery = 0;
volatile bool ledDiscoveryNeeded=false;
MagicHome LightsController;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
void setColor(byte r,byte g, byte b){
  for(int i=0; i<LED_COUNT;i++){
  pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
    pixels.show();

}
Rfid Rfid;
void deviceConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    ledDiscoveryNeeded=true;
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
void updateLightsById(uint32_t id)
{

  for (auto &light : LightsController.DiscoveredLights())
  {
    if (id == 0x00000C00) // white ashoka
    {
      light.SetColor(255, 255, 255);
      setColor(255,255,255);
    }
    if (id == 0x00000C01) // white
    {
      light.SetColor(255, 0, 0);
      setColor(255,0,0);
    }
    if (id == 0x00000C02) // green
    {
      light.SetColor(255, 255, 0);
      setColor(255,255,0);

    }
    if (id == 0x00000C03) // white
    {
      light.SetColor(255, 255, 0);
      setColor(255,255,0);
    }
    if (id == 0x00000C04) // green
    {
      light.SetColor(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C05) // white
    {
      light.SetColor(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C06) // green
    {
      light.SetColor(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C07) // white
    {
      light.SetColor(106, 13, 173);
      setColor(106,13,173);

    }
    if (id == 0x00000C08) // green
    {
      light.SetColor(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C09)
    {
      light.SetColor(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0A)
    {
      light.SetColor(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0B)
    {
      light.SetColor(255, 255, 0);
      setColor(255,255,0);

    }
    if (id == 0x00000C0C)
    {
      light.SetColor(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C0D)
    {
      light.SetColor(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0E)
    {
      light.SetColor(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C0F)
    {
      light.SetColor(106, 13, 173);
      setColor(106,13,173);

    }
    if (id == 0x00000C22)
    {
      light.SetColor(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C30)
    {
      light.SetColor(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C31)
    {
      light.SetColor(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C32)
    {
      light.SetColor(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C33)
    {
      light.SetColor(255, 0, 0);
            setColor(255,0,0);
    }
  }
}

void handleLightDiscovery()
{
  if (millis()>lastDiscovery + lightDiscovreyInterval)
  {
    ESP_LOGI(TAG,"%i,%i",lastDiscovery+lightDiscovreyInterval,millis());
    LightsController.DiscoverLights();
    lastDiscovery = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  ESP_LOGI(TAG, "Setting AP (%s)…", ssid);
  if (WiFi.softAP(ssid, password))
  {
    ESP_LOGI(TAG, "Setup AP %s.", ssid);
    IPAddress IP = WiFi.softAPIP();
    ESP_LOGI(TAG, "AP IP %s.", IP.toString());
    startOTA();
    LightsController.Init();
    delay(10000);
    WiFi.onEvent(deviceConnected,ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    LightsController.DiscoverLights();

  LightsController.DiscoverLights();
  }
  else
  {
    ESP_LOGE(TAG, "Failed To Setup AP (%s)!", ssid);
    delay(5000);
    ESP.restart();
  }
      Rfid.Init();
  delay(100);
  Rfid.Enable();
}
uint32_t lastTag = 0;
void loop()
{
  ArduinoOTA.handle();
  if(ledDiscoveryNeeded){
  LightsController.DiscoverLights();
  ledDiscoveryNeeded=false;
  }

  uint32_t tag = Rfid.ReadTag();

  if (tag != -1)
  {
    Serial.println(tag, HEX);
    if (tag != lastTag)
    {
      lastTag = tag;
      updateLightsById(tag);
    }
  }
}