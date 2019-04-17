
/*
==================================================================
        
        water vault esp8266
        
==================================================================  */

char DEVICE_MODEL[]                             = {"wv-esp8266"};// this determines firmware too.
int DEVICE_VERSION                               = 1;

#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <Wire.h>

/* Pins */ 
#define pin_SDA         5
#define pin_SCL         4
#define seal_on         14


#define bit_mcp23008_LED_BLUE        0x00
#define bit_mcp23008_LED_GREEN       0x01
#define bit_mcp23008_LED_RED         0x02
#define bit_mcp23008_                0x03
#define bit_mcp23008_                0x04
#define bit_mcp23008_                0x05
#define bit_mcp23008_CHA             0x06
#define bit_mcp23008_CHB             0x07


#include "Adafruit_MCP23008.h"
Adafruit_MCP23008 mcp;
bool MCP23008_DI_STATE[] = {0,0,0,0,0,0,0,0};
byte mcp23008_wake_intf = 0x00;
byte mcp23008_int_cap_pol = 0x00;

#include <TimeLib.h>
#include <mcp7940.h>
mcp7940 rtc = mcp7940();
unsigned long min_time = 1529560715;

byte sram_struct_is_valid       = 0xA5;
struct SRAM_STORAGE
{
        byte is_valid                           = 0x00; // 0xA5 == valid. 
        byte mcp23008_int_state                 = 0x00;
        byte mcp23008_cap_state                 = 0xC0;
        byte mcp23008_cur_state                 = 0xC0;
        unsigned long cha_event_epoch           = 0x00;
        unsigned long chb_event_epoch           = 0x00;
};
byte sram_address = 0x00;
SRAM_STORAGE sram; 


void setup() 
{
        pinMode(seal_on,OUTPUT);
        digitalWrite(seal_on,HIGH);
        Wire.begin(pin_SDA,pin_SCL);
        Serial.begin(115200);
        Serial.print("\n\n** Start **");
        // init RTC. 
                rtc.begin();
                time_t wake_time = rtc.getTimeRTC();
                if (wake_time < min_time)
                {
                        Serial.print("\nsetting rtc to = ");Serial.print(min_time);
                        wake_time = min_time;
                        rtc.setTimeRTC(wake_time);
                }
                
                rtc.clearAlarm0(true);// == set mfp high == power off, but seal on still enabled. 
                rtc.setAlarm0(wake_time+20); // sets MFP high, goes low on alarm.= power on device
                
                rtc.read_raw(sram_address,sram);
                if (sram.is_valid != sram_struct_is_valid)
                {
                        Serial.print("\nSRAM not init");
                        SRAM_STORAGE sram_default;
                        sram_default.is_valid = sram_struct_is_valid;
                        rtc.write_raw(sram_address,sram_default);
                        rtc.read_raw(sram_address,sram);
                }
                
                Serial.print("\nSRAM is_valid: ");Serial.print(sram.is_valid,HEX);
                Serial.print("\nSRAM mcp23008_int_state: ");Serial.print(sram.mcp23008_int_state,HEX);
                Serial.print("\nSRAM mcp23008_cap_state: ");Serial.print(sram.mcp23008_cap_state,HEX);
                Serial.print("\nSRAM mcp23008_cur_state: ");Serial.print(sram.mcp23008_cur_state,HEX);
                Serial.print("\nSRAM cha_event_epoch: ");Serial.print(sram.cha_event_epoch,HEX);
                Serial.print("\nSRAM chb_event_epoch: ");Serial.print(sram.chb_event_epoch,HEX);
        
        // MCP23008 setup
                mcp23008_wake_intf = mcp.read8(MCP23008_INTF);
                mcp.begin(0);
                Serial.print("\nMCP23008 Wake Int Flag: 0x");Serial.print(mcp23008_wake_intf,HEX);
                mcp23008_int_cap_pol = mcp.read8(0x08);
                Serial.print("\nMCP23008 Wake Int Pol: 0x");Serial.print(mcp23008_int_cap_pol,HEX);
                
        // init MCP2308
                mcp.pinMode(bit_mcp23008_CHA, INPUT);//mcp.pullUp(bit_mcp23008_CHA,HIGH);
                mcp.pinMode(bit_mcp23008_CHB, INPUT);//mcp.pullUp(bit_mcp23008_CHB,HIGH);
                mcp.pinMode(bit_mcp23008_LED_BLUE, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                mcp.pinMode(bit_mcp23008_LED_GREEN, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                mcp.pinMode(bit_mcp23008_LED_RED, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                
                // enable MCP23008 interrupts.
                mcp.write8(MCP23008_GPINTEN, 0b11000000); // set interupt enable for 6,7;
                
                mcp.write8(MCP23008_INTCON, 0x00); // 0 == compare to prev value, 1 = compare to DEVAL;
                
                mcp.write8(0x05, 0b00000100); // configure int pin as active low. 
                
}
 
 
void loop()
{
        byte mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nLoop Read");
        Serial.print("\nMCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp23008_int_cap_pol = mcp.read8(0x08);
        Serial.print("\nMCP23008 Wake Int Pol: 0x");Serial.print(mcp23008_int_cap_pol,HEX);
                
        MCP23008_read();
        
        mcp.digitalWrite(bit_mcp23008_LED_BLUE, LOW);
        delay(1000);
        
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nBLUE MCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
        delay(1000);
        
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nBLUE MCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp.digitalWrite(bit_mcp23008_LED_GREEN, LOW);
        delay(1000);
        
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nGreen MCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
        delay(1000);
        
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nGreen MCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp.digitalWrite(bit_mcp23008_LED_RED, LOW);
        delay(1000);
        
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nRed MCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
        delay(1000);
        
        Serial.print("\nPre shutdown read");
        mcp23008_intf = mcp.read8(MCP23008_INTF);
        Serial.print("\nMCP23008 Int Flag: 0x");Serial.print(mcp23008_intf,HEX);
        mcp23008_int_cap_pol = mcp.read8(0x08);
        Serial.print("\nMCP23008 Wake Int Pol: 0x");Serial.print(mcp23008_int_cap_pol,HEX);
        MCP23008_read();
        
        sram.mcp23008_int_state = mcp23008_intf;
        sram.mcp23008_cap_state = mcp23008_int_cap_pol;
        sram.mcp23008_cur_state = (MCP23008_DI_STATE[bit_mcp23008_CHB] << 8) | (MCP23008_DI_STATE[bit_mcp23008_CHA] << 7);
        rtc.write_raw(sram_address,sram);
        
        delay(1000);
        
        digitalWrite(seal_on,LOW);
        int i = 0;
        Serial.print("\nShutting down");
        while(i++ < 10){ Serial.print("\n");Serial.print(i);delay(1000); }
        Serial.print("\nShutdown failed");
        digitalWrite(seal_on,HIGH);
        rtc.clearAlarm0(false); // set MFP pin low
        //ESP.restart();
        ESP.deepSleep(100); // 20e6 is 20 microseconds
}


void MCP23008_read()
{
        MCP23008_DI_STATE[bit_mcp23008_CHA] = mcp.digitalRead(bit_mcp23008_CHA);
        MCP23008_DI_STATE[bit_mcp23008_CHB] = mcp.digitalRead(bit_mcp23008_CHB);
        
        Serial.print("\nbit_mcp23008_CHA: ");Serial.print(MCP23008_DI_STATE[bit_mcp23008_CHA]);
        Serial.print("\nbit_mcp23008_CHB: ");Serial.print(MCP23008_DI_STATE[bit_mcp23008_CHB]);
        Serial.print("\n");
}