#include "LightMessageTools.h"

std::vector<uint8_t> LightMessageTools::SetColorMessage(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    std::vector<uint8_t> message{0x31, red, green, blue, brightness, 0, 0, 0};
    message.push_back(GetHash(message));

  //  Serial.println("Message");
    //Serial.printf("Message %d %d %d %d %d %d %d %d %d \n", message[0], message[1], message[2], message[3], message[4], message[5], message[6], message[7], message[8]);
    return message;
};
uint8_t LightMessageTools::GetHash(std::vector<uint8_t> message)
{
    uint8_t hash = 0;
    for (int i = 0; i < message.size(); i++)
    {
        hash += message.at(i);
    }
    hash &= 0xff;
    return hash;
}
