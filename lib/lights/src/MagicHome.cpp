
#include "MagicHome.h"
#include "string.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
const char *udpAddress = "255.255.255.255";
const int udpPort = 48899;
const char* MAGIC_HOME_TAG = "MAGICHOME";
const uint16_t tcpPort = 5577;
char incomingPacket[256];
WiFiUDP udp;
MagicHome::MagicHome(){
}
void MagicHome::Init(){
    udp.begin(WiFi.localIP(), udpPort);
}
std::vector<Light> FindLights()
{
  // only send data when connected
  std::vector<Light> foundLights = {};
  ESP_LOGI(MAGIC_HOME_TAG,"Find Lights");

  //if (connected)
  //{
    ESP_LOGI(MAGIC_HOME_TAG,"sends udp discovery packet");

    // Send a packet
    udp.beginPacket(udpAddress, udpPort);
    udp.print("HF-A11ASSISTHREAD");
    udp.endPacket();
    for (size_t i = 0; i < 10; i++)
    {
      /* code */

      int packetSize = udp.parsePacket();
      if (packetSize)
      {
        ESP_LOGI(MAGIC_HOME_TAG,"Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
        int len = udp.read(incomingPacket, 255);
        if (len > 0)
        {
          incomingPacket[len] = '\0';
        }
        ESP_LOGI(MAGIC_HOME_TAG,"UDP packet contents: %s\n", incomingPacket);
        std::string data = std::string(incomingPacket);
        int commaIndex = data.find(',');
        std::string ip = data.substr(0, commaIndex);
        foundLights.push_back(Light(ip));
        ESP_LOGI(MAGIC_HOME_TAG,"%s",ip.c_str());
      }
      delay(10);
    }
  //}
  return foundLights;
}
std::vector<Light> MagicHome::DiscoveredLights(){
  return lights;
}
void MagicHome::DiscoverLights()
{
      ESP_LOGI(MAGIC_HOME_TAG,"Searching for Lights");
  for (size_t i = 0; i < 5; i++)
  {
    std::vector<Light> newLights = FindLights();
    ESP_LOGI(MAGIC_HOME_TAG,"found %d lights\n", newLights.size());
    for (auto &&newlight : newLights)
    {
      bool contains = false;
      for (auto &&oldLight : lights)
      {
        if (oldLight.GetIpAddress() == newlight.GetIpAddress())
        {
          contains = true;
        }
      }
      if (!contains)
      {
        lights.push_back(newlight);
      }
    }

    delay(100);
  }
  ESP_LOGI(MAGIC_HOME_TAG,"lights now contains %d lights\n", lights.size());
  ESP_LOGI(MAGIC_HOME_TAG,"Searching for Done");
}
