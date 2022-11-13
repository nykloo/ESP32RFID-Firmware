
#ifndef LIGHT_H
#define LIGHT_H
#include <stdint.h>
#include <string>
#include <vector>
class Light {
    public:
    void SetColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness=127);
    std::string GetIpAddress();
    Light(std::string ip);
    //GetColor
    private:
    std::string ipAddress;
    void SendMessage(const std::vector<uint8_t> message);
};

#endif