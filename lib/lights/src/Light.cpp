#include "Light.h"
#include <Arduino.h>
#include <WiFi.h>
#include "LightMessageTools.h"
const uint16_t tcpPort = 5577;
const char* LIGHT_TAG = "LIGHT";
Light::Light(std::string ip){
    ipAddress=ip;
}
void Light::SetColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    ESP_LOGI(LIGHT_TAG, "Set Light at %s to (%i,%i,%i,%i)", ipAddress.c_str(),red,green,blue,brightness);
    SendMessage(LightMessageTools::SetColorMessage(red,green,blue,brightness));
}
std::string Light::GetIpAddress()
{
    return ipAddress;
}
void Light::SendMessage(const std::vector<uint8_t> message)
{
    for (size_t i = 0; i < 3; i++)
    {

        WiFiClient client;

        if (!client.connect(ipAddress.c_str(), tcpPort))
        {
            ESP_LOGE(LIGHT_TAG,"Connection failed. Could not send message");
            delay(5000);
            return;
        }
        client.write(message.data(), message.size());
        client.stop();
        delay(20);
    }
}
