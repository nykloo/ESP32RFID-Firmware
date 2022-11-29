#include "Rfid.h"
const char *RF_TAG = "RFID";
volatile unsigned int clkCount = 0;
volatile bool lastEdgeWasFalling = false;

void Rfid::Enable()
{
    digitalWrite(shd, LOW);
}
void Rfid::Disable()
{
    digitalWrite(shd, HIGH);
}
void IRAM_ATTR onClk()
{
    clkCount++;
}
// void IRAM_ATTR fallDetected() {
//   lastEdgeWasFalling=true;Serial.println("down");
// }
// void IRAM_ATTR riseDetected() {
//  lastEdgeWasFalling=false;Serial.println("up");

// }
void Rfid::Init()
{
    pinMode(shd, OUTPUT);
    pinMode(mod, OUTPUT);
    pinMode(demodOut, INPUT);
    pinMode(rdyClk, INPUT);
    digitalWrite(mod, LOW);
    // todo only attach on read command dont want to affect timming while sending commands
    //attachInterrupt(rdyClk, onClk, RISING);
    // attachInterrupt(demodOut, riseDetected,RISING);
    // attachInterrupt(demodOut, fallDetected,FALLING);
}
uint32_t Rfid::ReadTag()
{
    if (scanForTag(tagData) == true)
    {
        return (tagData[1] << 24) | (tagData[2] << 16) | (tagData[3] << 8) | (tagData[4]);
    }
    else
    {
        return -1;
    }
}
void Rfid::debug()
{
    while (0==digitalRead(demodOut))
    { // sync on falling edge
    }
    while (1==digitalRead(demodOut))
    { // sync on falling edge
    }
    clkCount = 0;
    while (clkCount < 16)
        ; // we are going to read ever 32 but starting after 16 offset
    for (int i = 0; i < 512; i++)
    {   // I think this would contain
        // instead of reading the pin we infer based on interupt mantained value
        demodBuffer[i] = digitalRead(demodOut);
        clkCount = 0;
        while (clkCount < 64)
            ;
    }
    Serial.println();

    for (int i = 0; i < 512; i++)
    {   // I think this would contain
        // instead of reading the pin we infer based on interupt mantained value
        Serial.print(demodBuffer[i]);
    }
    Serial.println();
}
bool Rfid::decodeTag(unsigned char *buf)
{

    unsigned char i = 0;
    unsigned short timeCount;
    unsigned char timeOutFlag = 0;
    unsigned char row, col;
    unsigned char row_parity;
    unsigned char col_parity[5];
    unsigned char dat;
    unsigned char j;

    //unsigned int manchesterPosition = 0;
    // while (true)
    // {

    //     // if(clkCount>64+8){
    //     //     //store erron in the response stream?
    //     //     manchesterPosition=1;
    //     // }
    //     // if(clkCount>48){
    //     //     if(lastEdgeWasFalling){
    //     //         //vaalue=1
    //     //     }else{
    //     //         //value=0;
    //     //     }
    //     //     manchesterPosition=0;
    //     // }else if(manchesterPosition>0){
    //     //     //store the value bit
    //     //     manchesterPosition=0;
    //     // }else{
    //     //     manchesterPosition=1;

    //     // }
    // }

    while (1)
    {
         timeCount = 0;
        //clkCount = 0;
        while (0 == digitalRead(demodOut)) // watch for demodOut to go low
        {

            if (timeCount >= TIMEOUT) // if we pass TIMEOUT milliseconds, break out of the loop
            {
                break;
            }
            else
            {
                timeCount++;
            }
        }

        if (timeCount >= 600)
        {
            return false;
        }
        timeCount = 0;

        delayMicroseconds(DELAYVAL);
        if (digitalRead(demodOut))
        {

            for (i = 0; i < 8; i++) // 9 header bits
            {
                timeCount = 0;                     // restart counting
                while (1 == digitalRead(demodOut)) // while DEMOD out is high
                {
                    if (timeCount == TIMEOUT)
                    {
                        timeOutFlag = 1;
                        break;
                    }
                    else
                    {
                        timeCount++;
                    }
                }

                if (timeOutFlag)
                {
                    break;
                }
                else
                {
                    delayMicroseconds(DELAYVAL);
                    if (0 == digitalRead(demodOut))
                    {
                        break;
                    }
                }
            } // end for loop

            if (timeOutFlag)
            {
                timeOutFlag = 0;
                return false;
            }

            if (i == 8) // Receive the data
            {

                timeOutFlag = 0;
                timeCount = 0;
                while (1 == digitalRead(demodOut))
                {
                    if (timeCount == TIMEOUT)
                    {
                        timeOutFlag = 1;
                        break;
                    }
                    else
                    {
                        timeCount++;
                    }

                    if (timeOutFlag)
                    {
                        timeOutFlag = 0;
                        return false;
                    }
                }

                col_parity[0] = col_parity[1] = col_parity[2] = col_parity[3] = col_parity[4] = 0;
                for (row = 0; row < 11; row++)
                {
                    row_parity = 0;
                    j = row >> 1; // what ???
                    // 1,2,3,4,5,6,7,8,9,10
                    // 0,0,1,1,2,2,3,3,4,5
                    // hmm i guess whats happen is read lower 4 bits
                    // then read upper 4 bits
                    // so i guess we 4 data bits the parity bit 8 times
                    // then we get 4 more parity bits
                    for (col = 0, row_parity = 0; col < 5; col++)
                    {
                        delayMicroseconds(DELAYVAL);
                        if (digitalRead(demodOut))
                        {
                            dat = 1;
                        }
                        else
                        {
                            dat = 0;
                        }

                        if (col < 4 && row < 10)
                        {
                            buf[j] <<= 1;
                            buf[j] |= dat;
                        }

                        row_parity += dat;
                        col_parity[col] += dat;
                        timeCount = 0;
                        while (digitalRead(demodOut) == dat)
                        {
                            if (timeCount == TIMEOUT)
                            {
                                timeOutFlag = 1;
                                break;
                            }
                            else
                            {
                                timeCount++;
                            }
                        }
                        if (timeOutFlag)
                        {
                            break;
                        }
                    }

                    if (row < 10)
                    {
                        if ((row_parity & 0x01) || timeOutFlag) // Row parity
                        {
                            timeOutFlag = 1;
                            break;
                        }
                    }
                }

                if (timeOutFlag || (col_parity[0] & 0x01) || (col_parity[1] & 0x01) || (col_parity[2] & 0x01) || (col_parity[3] & 0x01)) // Column parity
                {
                    timeOutFlag = 0;
                    return false;
                }
                else
                {
                    ESP_LOGI(RF_TAG, "Row Parity: %d", row_parity);
                    ESP_LOGI(RF_TAG, "Col 0 Parity: %d", col_parity[0]);
                    ESP_LOGI(RF_TAG, "Col 1 Parity: %d", col_parity[1]);
                    ESP_LOGI(RF_TAG, "Col 2 Parity: %d", col_parity[2]);
                    ESP_LOGI(RF_TAG, "Col 3 Parity: %d", col_parity[3]);
                    ESP_LOGI(RF_TAG, "Col 4 Parity: %d", col_parity[4]);
                    ESP_LOGI(RF_TAG, "Data 0: %d", buf[0]);
                    ESP_LOGI(RF_TAG, "Data 1: %d", buf[1]);
                    ESP_LOGI(RF_TAG, "Data 2: %d", buf[2]);
                    ESP_LOGI(RF_TAG, "Data 3: %d", buf[3]);
                    ESP_LOGI(RF_TAG, "Data 4: %d", buf[4]);

                    return true;
                }

            } // end if(i==8)

            return false;

        } // if(digitalRead(demodOut))
    }     // while(1)
}
bool Rfid::compareTagData(byte *tagData1, byte *tagData2)
{
    for (int j = 0; j < 5; j++)
    {
        if (tagData1[j] != tagData2[j])
        {
            return false; // if any of the ID numbers are not the same, return a false
        }
    }

    return true; // all id numbers have been verified
}
void Rfid::transferToBuffer(byte *tagData, byte *tagDataBuffer)
{
    for (int j = 0; j < 5; j++)
    {
        tagDataBuffer[j] = tagData[j];
    }
}
bool Rfid::scanForTag(byte *tagData)
{
    static byte tagDataBuffer[5]; // A Buffer for verifying the tag data. 'static' so that the data is maintained the next time the loop is called
    static int readCount = 0;     // the number of times a tag has been read. 'static' so that the data is maintained the next time the loop is called
    boolean verifyRead = false;   // true when a tag's ID matches a previous read, false otherwise
    boolean tagCheck = false;     // true when a tag has been read, false otherwise

    tagCheck = decodeTag(tagData); // run the decodetag to check for the tag
    if (tagCheck == true)          // if 'true' is returned from the decodetag function, a tag was succesfully scanned
    {

        readCount++; // increase count since we've seen a tag

        if (readCount == 1) // if have read a tag only one time, proceed
        {
            transferToBuffer(tagData, tagDataBuffer); // place the data from the current tag read into the buffer for the next read
        }
        else if (readCount == 2) // if we see a tag a second time, proceed
        {
            verifyRead = compareTagData(tagData, tagDataBuffer); // run the checkBuffer function to compare the data in the buffer (the last read) with the data from the current read

            if (verifyRead == true) // if a 'true' is returned by compareTagData, the current read matches the last read
            {
                readCount = 0; // because a tag has been succesfully verified, reset the readCount to '0' for the next tag
                return true;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}