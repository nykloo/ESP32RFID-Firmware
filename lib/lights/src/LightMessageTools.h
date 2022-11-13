#ifndef LIGHT_MESSAGE_TOOLS_H
#define LIGHT_MESSAGE_TOOLS_H
#include <vector>
#include <stdint.h>

class LightMessageTools
{
public:
    static std::vector<uint8_t> SetColorMessage(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
    static uint8_t GetHash(std::vector<uint8_t> message);
};
#endif