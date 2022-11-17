#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#define DELAYVAL 384  // 384 //standard delay for manchster decode
#define TIMEOUT 10000 // standard timeout for manchester decode at  160mhz is 1000000
class Rfid
{
public:
    void Enable();
    void Disable();
    void Init();
    uint32_t ReadTag();

private:
    // pin configuration
    int demodOut = 16;
    int shd = 4;
    int mod = 27;
    int rdyClk = 15;
    byte tagData[5];
    bool decodeTag(unsigned char *buf);
    bool compareTagData(byte *tagData1, byte *tagData2);
    void transferToBuffer(byte *tagData, byte *tagDataBuffer);
    bool scanForTag(byte *tagData);
};

#endif