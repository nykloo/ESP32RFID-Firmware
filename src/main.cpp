#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid     = "Kyber Nexus";
const char* password = "123456789";
const char* TAG = "MAIN";
void startOTA(){
  //upload with
  // pio run -t upload --upload-port myesp32.local
  // or set config https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update

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
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      ESP_LOGI(TAG,"Start updating %s", type);
    })
    .onEnd([]() {
      ESP_LOGI(TAG,"\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      ESP_LOGI(TAG,"Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) ESP_LOGI(TAG,"Auth Failed");
      else if (error == OTA_BEGIN_ERROR) ESP_LOGI(TAG,"Begin Failed");
      else if (error == OTA_CONNECT_ERROR) ESP_LOGI(TAG,"Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) ESP_LOGI(TAG,"Receive Failed");
      else if (error == OTA_END_ERROR) ESP_LOGI(TAG,"End Failed");
    });
    ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  ESP_LOGI(TAG,"Setting AP (%s)…",ssid);
 if(WiFi.softAP(ssid, password)){
    ESP_LOGI(TAG,"Setup AP %s.",ssid);
    IPAddress IP = WiFi.softAPIP();
    ESP_LOGI(TAG,"AP IP %s.",IP.toString());

    startOTA();
 }else{
    ESP_LOGE(TAG,"Failed To Setup AP (%s)!",ssid);
    delay(5000);
    ESP.restart();
 }
}

void loop() {
    ArduinoOTA.handle();
}