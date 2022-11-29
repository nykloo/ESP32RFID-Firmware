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
#define WaitUS delayMicroseconds
const char *ssid = "Kyber Nexus";
const char *password = "123456789";
const char *TAG = "MAIN";
const int lightDiscovreyInterval = 120 * 1000;
int lastDiscovery = 0;
volatile bool ledDiscoveryNeeded=false;
MagicHome LightsController;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

//-----------------------------------
// EM4469 / EM4305 routines
//-----------------------------------
// Below given command set.
// Commands are including the even parity, binary mirrored
#define FWD_CMD_LOGIN   0xC
#define FWD_CMD_WRITE   0xA
#define FWD_CMD_READ    0x9
#define FWD_CMD_PROTECT 0x3
#define FWD_CMD_DISABLE 0x5

static uint8_t forwardLink_data[64]; //array of forwarded bits
static uint8_t *forward_ptr;  //ptr for forward message preparation
static uint8_t fwd_bit_sz; //forwardlink bit counter
static uint8_t *fwd_write_ptr;  //forwardlink bit pointer

//====================================================================
// prepares command bits
// see EM4469 spec
//====================================================================
//--------------------------------------------------------------------
//  VALUES TAKEN FROM EM4x function: SendForward
//  START_GAP = 440;       (55*8) cycles at 125kHz (8us = 1cycle)
//  WRITE_GAP = 128;       (16*8)
//  WRITE_1   = 256 32*8;  (32*8)

//  These timings work for 4469/4269/4305 (with the 55*8 above)
//  WRITE_0 = 23*8 , 9*8

static uint8_t Prepare_Cmd(uint8_t cmd) {

    *forward_ptr++ = 0; //start bit
    *forward_ptr++ = 0; //second pause for 4050 code

    *forward_ptr++ = cmd;
    cmd >>= 1;
    *forward_ptr++ = cmd;
    cmd >>= 1;
    *forward_ptr++ = cmd;
    cmd >>= 1;
    *forward_ptr++ = cmd;

    return 6; //return number of emitted bits
}

//====================================================================
// prepares address bits
// see EM4469 spec
//====================================================================
static uint8_t Prepare_Addr(uint8_t addr) {

    register uint8_t line_parity;

    uint8_t i;
    line_parity = 0;
    for (i = 0; i < 6; i++) {
        *forward_ptr++ = addr;
        line_parity ^= addr;
        addr >>= 1;
    }

    *forward_ptr++ = (line_parity & 1);

    return 7; //return number of emitted bits
}

//====================================================================
// prepares data bits intreleaved with parity bits
// see EM4469 spec
//====================================================================
static uint8_t Prepare_Data(uint16_t data_low, uint16_t data_hi) {

    register uint8_t column_parity;
    register uint8_t i, j;
    register uint16_t data;

    data = data_low;
    column_parity = 0;

    for (i = 0; i < 4; i++) {
        register uint8_t line_parity = 0;
        for (j = 0; j < 8; j++) {
            line_parity ^= data;
            column_parity ^= (data & 1) << j;
            *forward_ptr++ = data;
            data >>= 1;
        }
        *forward_ptr++ = line_parity;
        if (i == 1)
            data = data_hi;
    }

    for (j = 0; j < 8; j++) {
        *forward_ptr++ = column_parity;
        column_parity >>= 1;
    }
    *forward_ptr = 0;

    return 45; //return number of emitted bits
}
#define turn_read_lf_off(microSeconds) digitalWrite(27,HIGH); delayMicroseconds(microSeconds);
#define turn_read_lf_on(microSeconds) digitalWrite(27,LOW); delayMicroseconds(microSeconds);

//====================================================================
// Forward Link send function
// Requires: forwarLink_data filled with valid bits (1 bit per byte)
// fwd_bit_count set with number of bits to be sent
//====================================================================
static void SendForward(uint8_t fwd_bit_count, bool fast) {
// iceman,   21.3us increments for the USclock verification.
// 55FC * 8us == 440us / 21.3 === 20.65 steps.  could be too short. Go for 56FC instead
// 32FC * 8us == 256us / 21.3 ==  12.018 steps. ok
// 16FC * 8us == 128us / 21.3 ==  6.009 steps. ok
#ifndef EM_START_GAP
#define EM_START_GAP 55*8
#endif

    fwd_write_ptr = forwardLink_data;
    fwd_bit_sz = fwd_bit_count;


    // force 1st mod pulse (start gap must be longer for 4305)
    fwd_bit_sz--; //prepare next bit modulation
    fwd_write_ptr++;

    turn_read_lf_off(EM_START_GAP);
    turn_read_lf_on(18 * 8);

    // now start writing with bitbanging the antenna. (each bit should be 32*8 total length)
    while (fwd_bit_sz-- > 0) { //prepare next bit modulation
        if (((*fwd_write_ptr++) & 1) == 1) {
            turn_read_lf_on (32 * 8);
        } else {
            turn_read_lf_off(23 * 8);
            turn_read_lf_on(18 * 8);
        }
    }
}

static void EM4xLoginEx(uint32_t pwd) {
    forward_ptr = forwardLink_data;
    uint8_t len = Prepare_Cmd(FWD_CMD_LOGIN);
    len += Prepare_Data(pwd & 0xFFFF, pwd >> 16);
    SendForward(len, false);
    //WaitUS(20); // no wait for login command.
    // should receive
    // 0000 1010 ok
    // 0000 0001 fail
}


void EM4xLogin(uint32_t pwd, bool ledcontrol) {




    EM4xLoginEx(pwd);

    WaitUS(400);
}

void EM4xReadWord(uint8_t addr, uint32_t pwd, uint8_t usepwd, bool ledcontrol) {


    // clear buffer now so it does not interfere with timing later

    /* should we read answer from Logincommand?
    *
    * should receive
    * 0000 1010 ok
    * 0000 0001 fail
    **/
    if (usepwd) EM4xLoginEx(pwd);

    forward_ptr = forwardLink_data;
    uint8_t len = Prepare_Cmd(FWD_CMD_READ);
    len += Prepare_Addr(addr);

    SendForward(len, false);

    WaitUS(400);


}

void EM4xWriteWord(uint8_t addr, uint32_t data, uint32_t pwd, uint8_t usepwd, bool ledcontrol) {



    /* should we read answer from Logincommand?
    *
    * should receive
    * 0000 1010 ok.
    * 0000 0001 fail
    **/
    if (usepwd) EM4xLoginEx(pwd);

    forward_ptr = forwardLink_data;
    uint8_t len = Prepare_Cmd(FWD_CMD_WRITE);
    len += Prepare_Addr(addr);
    len += Prepare_Data(data & 0xFFFF, data >> 16);

    SendForward(len, false);
}

void EM4xProtectWord(uint32_t data, uint32_t pwd, uint8_t usepwd, bool ledcontrol) {


    /* should we read answer from Logincommand?
    *
    * should receive
    * 0000 1010 ok.
    * 0000 0001 fail
    **/
    if (usepwd) EM4xLoginEx(pwd);

    forward_ptr = forwardLink_data;
    uint8_t len = Prepare_Cmd(FWD_CMD_PROTECT);
    len += Prepare_Data(data & 0xFFFF, data >> 16);

    SendForward(len, false);

}


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
void setWifILights(byte r, byte g, byte b){
  for (auto &light : LightsController.DiscoveredLights())
  {
      setWifILights(r, g, b);
  }
}
void updateLightsById(uint32_t id)
{


    if (id == 0x00000C00) // white ashoka
    {
      setWifILights(255, 255, 255);
      setColor(255,255,255);
    }
    if (id == 0x00000C01) // white
    {
      setWifILights(255, 0, 0);
      setColor(255,0,0);
    }
    if (id == 0x00000C02) // green
    {
      setWifILights(255, 255, 0);
      setColor(255,255,0);

    }
    if (id == 0x00000C03) // white
    {
      setWifILights(255, 255, 0);
      setColor(255,255,0);
    }
    if (id == 0x00000C04) // green
    {
      setWifILights(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C05) // white
    {
      setWifILights(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C06) // green
    {
      setWifILights(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C07) // white
    {
      setWifILights(106, 13, 173);
      setColor(106,13,173);

    }
    if (id == 0x00000C08) // green
    {
      setWifILights(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C09)
    {
      setWifILights(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0A)
    {
      setWifILights(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0B)
    {
      setWifILights(255, 255, 0);
      setColor(255,255,0);

    }
    if (id == 0x00000C0C)
    {
      setWifILights(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C0D)
    {
      setWifILights(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C0E)
    {
      setWifILights(0, 0, 255);
      setColor(0,0,255);

    }
    if (id == 0x00000C0F)
    {
      setWifILights(106, 13, 173);
      setColor(106,13,173);

    }
    if (id == 0x00000C22)
    {
      setWifILights(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C30)
    {
      setWifILights(255, 255, 255);
      setColor(255,255,255);

    }
    if (id == 0x00000C31)
    {
      setWifILights(255, 0, 0);
      setColor(255,0,0);

    }
    if (id == 0x00000C32)
    {
      setWifILights(0, 255, 0);
      setColor(0,255,0);

    }
    if (id == 0x00000C33)
    {
      setWifILights(255, 0, 0);
            setColor(255,0,0);
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
      Rfid.Init();

  Serial.begin(115200);
  //ESP_LOGI(TAG, "Setting AP (%s)…", ssid);
  // if (WiFi.softAP(ssid, password))
  // {
  //   ESP_LOGI(TAG, "Setup AP %s.", ssid);
  //   IPAddress IP = WiFi.softAPIP();
  //   ESP_LOGI(TAG, "AP IP %s.", IP.toString());
  //   startOTA();
  //   LightsController.Init();
  //   delay(10000);
  //   WiFi.onEvent(deviceConnected,ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
  //   //LightsController.DiscoverLights();

  // }
  // else
  // {
  //   ESP_LOGE(TAG, "Failed To Setup AP (%s)!", ssid);
  //   delay(5000);
  //   ESP.restart();
  // }
  Serial.begin(115200);
  delay(100);
  Rfid.Enable();
}
uint32_t lastTag = 0;
bool readmode=false;
void loop()
{
//   // ArduinoOTA.handle();
//   // if(ledDiscoveryNeeded){
//   // LightsController.DiscoverLights();
//   // ledDiscoveryNeeded=false;
//   // }
//   //write bit 0 to enter command mode 0b0
// // write command 0b0101
// // write address 0b0101 is for 5
// // 2 extra 0 bytes 0b00
// // write address even parity for 5 is 0b0 becuase the count of 1s is evens
// // data for c08
// // (byte1) (byte1 even parity bit) (0b00000000) (0b0)
// // (byte1) (byte1 even parity bit) (0b00000000) (0b0)
// // (byte1) (byte1 even parity bit) (0b00001100) (0b0)
// // (byte1) (byte1 even parity bit) (0b00001000) (0b1)
// // (col parity byte) (0)           (0b00000100) (0b0)
// //end of command
// //so
// //this is sliced wrong
// // 0b 0 0101 0101 00 0 00000000 0 00000000 0 00001100 0 00001000 1 00000100 0
// //seems like this is the proper way
// // 0b 0 0101 0101 00 0 0000 0 0000 0 0000 0 0000 0 0000 0 1100 0 0000 0 1000 1 0001 0
// //uint64_t writeCommandbad = 0b001010101000000000000000000000000011000000010001000001000;
// uint64_t writeCommand      = 0b001010101000000000000000000000000000011000000001000100010;
// uint64_t readBlock5Command = 0b0100101010;
// uint64_t readConfigCommand = 0b0100101001;

// // data read
// // [ 14022][I][Rfid.cpp:192] decodeTag(): [RFID] Row Parity: 2
// // [ 14022][I][Rfid.cpp:193] decodeTag(): [RFID] Col 0 Parity: 2
// // [ 14022][I][Rfid.cpp:194] decodeTag(): [RFID] Col 1 Parity: 2
// // [ 14027][I][Rfid.cpp:195] decodeTag(): [RFID] Col 2 Parity: 2
// // [ 14033][I][Rfid.cpp:196] decodeTag(): [RFID] Col 3 Parity: 0
// // [ 14038][I][Rfid.cpp:197] decodeTag(): [RFID] Col 4 Parity: 0
// // [ 14044][I][Rfid.cpp:198] decodeTag(): [RFID] Data 0: 0
// // [ 14049][I][Rfid.cpp:199] decodeTag(): [RFID] Data 1: 0
// // [ 14054][I][Rfid.cpp:200] decodeTag(): [RFID] Data 2: 0
// // [ 14058][I][Rfid.cpp:201] decodeTag(): [RFID] Data 3: 12
// // [ 14063][I][Rfid.cpp:202] decodeTag(): [RFID] Data 4: 6
// // C06
// // 00000000 
// // do a first field stop of at least 55 rf pulses will always work
// // to send a 1 leave field on for 32 clocks
// // to send a 0 leave field on for 18 clock then off for 14

// //hmm it would probably be better to cound to clocks with an interupt but
// //we can probaly just delay for the amount of time that matches the clocks
// // a clock for 125 khz takes 8us
 if(millis()<10000){//write for first 10 seconds
// //  digitalWrite(27,LOW);
// //  delayMicroseconds(8*16);
// //  digitalWrite(27,HIGH);
// //  delayMicroseconds(8*16);
// // for(int i=56; i--;i>=0){
// //   if(1==((writeCommand>>i)&1)){
// //     digitalWrite(27,LOW);
// //     delayMicroseconds(8*32);
// //   }else{
// //     digitalWrite(27,LOW);
// //     delayMicroseconds(8*18);
// //     digitalWrite(27,HIGH);
// //     delayMicroseconds(8*14);
// //   }
// // }
// //digitalWrite(27,LOW);

//uint32_t tag = Rfid.ReadTag();
//  delay(100);
 //EM4xReadWord(5,0,0,0);
  //delayMicroseconds(300);
 // Rfid.debug();
 EM4xWriteWord(5,0x000001FF,0,0,0);
 EM4xWriteWord(6,0x3E183000,0,0,0);//vader 8ball
 }else{ 

// // //write done set back to high field
// // digitalWrite(27,LOW);
// // if(readmode==false){
// // ESP_LOGI(TAG, "READ TAG start");
// // readmode =true;
// // }

// // //delay(1000);
// //   // for(int i=0;i<100;i++){
// //   //   digitalWrite(27,LOW);
// //   //   delayMicroseconds(126);
// //   //   digitalWrite(27,HIGH);s
// //   //   delayMicroseconds(126);
// //   // }

// //   // }
// //   //delay(1000);
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
}

