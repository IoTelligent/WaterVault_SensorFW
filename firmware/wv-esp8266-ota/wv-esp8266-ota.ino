#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266httpUpdate.h>

#include <Wire.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <mcp7940.h>
#include <Arduino.h>

/* Pins */ 
#define pin_SDA         5
#define pin_SCL         4
#define pin_seal_on     14
#define pin_vbat        A0
#define pin_mode        2

#define bit_mcp23008_LED_BLUE        0x00
#define bit_mcp23008_LED_GREEN       0x01
#define bit_mcp23008_LED_RED         0x02
#define bit_mcp23008_                0x03
#define bit_mcp23008_                0x04
#define bit_mcp23008_                0x05
#define bit_mcp23008_CHA             0x06
#define bit_mcp23008_CHB             0x07

/*
==================================================================
        device modes
==================================================================  */
#define MODE_INIT                       0x00
#define MODE_WIFI_AP_ON                 0x01
#define MODE_WIFI_STA_CONNECTING        0x02
#define MODE_WIFI_STA_CONNECTED         0x03
#define MODE_WIFI_STA_CONNECTION_FAILED 0x04
#define MODE_OFF                        0x05
#define MODE_NORMAL_RUN                 0x06
#define MODE_FIRMWARE_UPDATE_IP         0x07
byte current_mode = MODE_NORMAL_RUN;


#include "Adafruit_MCP23008.h";
Adafruit_MCP23008 mcp;


byte DEVICE_MAC[6]                              = {};
const char* endpoint_host                       = "devices.getiotelligent.com";
const int endpoint_port                         = 443;

mcp7940 rtc = mcp7940();

unsigned long active_sleep_seconds = 120;


void device_sleep(unsigned long seconds);
void device_reset_max_timer();
void device_led_setmode(byte led_mode);

void setup()
{
        // seal on power
                pinMode(pin_seal_on,OUTPUT);
                digitalWrite(pin_seal_on,HIGH);
                
        // init serial.
                Serial.begin(115200);
                Serial.print("\nStarting ");
        
        // start I2C
                Wire.begin(pin_SDA,pin_SCL);
                
        // get MAC address.
                WiFi.macAddress(DEVICE_MAC);
                
        // init RTC.
                bool has_rtc = true;
                if (rtc.begin() != 0x00)
                {
                        Serial.print("\nNo RTC found");        
                        has_rtc = false;
                }
                delay(100);
                time_t wake_time = rtc.getTimeRTC();
                rtc.clearAlarm0(true);// == set mfp high == power off, but seal on still enabled. 
                rtc.setAlarm0(wake_time+120); // sets MFP high, goes low on alarm.= power on device

                
        // init MCP2308
                mcp.begin(0);
                mcp.pinMode(bit_mcp23008_CHA, INPUT);//mcp.pullUp(bit_mcp23008_CHA,HIGH);
                mcp.pinMode(bit_mcp23008_CHB, INPUT);//mcp.pullUp(bit_mcp23008_CHB,HIGH);
                mcp.pinMode(bit_mcp23008_LED_BLUE, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                mcp.pinMode(bit_mcp23008_LED_GREEN, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                mcp.pinMode(bit_mcp23008_LED_RED, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                device_led_setmode(MODE_INIT);
                
                // enable MCP23008 interrupts.
                mcp.write8(MCP23008_GPINTEN, 0b11000000); // set interupt enable for 6,7;
                mcp.write8(MCP23008_INTCON, 0x00); // 0 == compare to prev value, 1 = compare to DEVAL;
                mcp.write8(0x05, 0b00000100); // configure int pin as active low.
                
                
                WiFi.mode(WIFI_STA);
                
                device_led_setmode(MODE_WIFI_STA_CONNECTING);
                
        // init eeprom
                EEPROM.begin(512);
                
                WiFi.begin();
                
                // Wait for connection
                unsigned long timeout_ms = millis(); 
                while (WiFi.status() != WL_CONNECTED)
                {

                        delay(1000);
                        Serial.print(".");
                        
                        if (millis() - timeout_ms > 30000)
                        {
                                Serial.print("Connection Failed");
                                device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                                delay(1000);
                                device_sleep(active_sleep_seconds);
                        }
                }
                

                device_led_setmode(MODE_WIFI_STA_CONNECTED);
                delay(1000);
                
                Serial.print("\nIP address: ");
                Serial.print(WiFi.localIP());
                
                device_do_update();
}

void loop()
{

}


void device_do_update()
{
        device_led_setmode(MODE_FIRMWARE_UPDATE_IP);
                
        WiFiClientSecure client;
        client.setInsecure();
        
        Serial.print("\nConnecting to host for update ");Serial.print(endpoint_host);
  
        if (!client.connect(endpoint_host, endpoint_port))
        {
                Serial.print(" - connection failed");
                device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                delay(1000);
                device_sleep(10);
                return;
        }
        
        Serial.print(" - Connected");
        
        Serial.print("\nUpdate result: ");
        char url[100] = {};
        sprintf(url,"/firmware/model/esp8266/id/%02x%02x%02x%02x%02x%02x/file/prod",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
        
        byte ret = ESPhttpUpdate.update(client, endpoint_host, endpoint_port,url);
        switch(ret)
        {
                case HTTP_UPDATE_FAILED:
                    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    break;
    
                case HTTP_UPDATE_NO_UPDATES:
                    Serial.println("HTTP_UPDATE_NO_UPDATES");
                    break;
    
                case HTTP_UPDATE_OK:
                    Serial.println("HTTP_UPDATE_OK");
                    break;
                default:
                {
                        Serial.print("\nun known return: ");Serial.print(ret);
                }
        }
        
        device_sleep(10);
}

void device_led_setmode(byte led_mode)
{
        switch(led_mode)
        {
                case MODE_WIFI_AP_ON:
                {
                        mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, LOW);
                }break;
                
                case MODE_WIFI_STA_CONNECTING:
                {
                        Serial.print("\nMODE_WIFI_STA_CONNECTING");
                        mcp.digitalWrite(bit_mcp23008_LED_RED, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                }break;
                
                case MODE_WIFI_STA_CONNECTED:
                {
                        Serial.print("\nMODE_WIFI_STA_CONNECTED");
                        mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                }break;
                
                case MODE_FIRMWARE_UPDATE_IP:
                {
                        Serial.print("\nMODE_FIRMWARE_UPDATE_IP");
                        mcp.digitalWrite(bit_mcp23008_LED_RED, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, LOW);
                }break;
                
                case MODE_WIFI_STA_CONNECTION_FAILED:
                {
                        Serial.print("\nMODE_WIFI_STA_CONNECTION_FAILED");
                        mcp.digitalWrite(bit_mcp23008_LED_RED, LOW);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                }break;
                
                case MODE_OFF:
                case MODE_INIT:
                {
                        mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                        mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                }break;
        }
}

void device_sleep(unsigned long seconds)
{
        Serial.print("\nShutting down for ");Serial.print(seconds);Serial.print(" seconds");
        device_led_setmode(MODE_OFF);
        if (seconds > 0)
        {
                int i = 0;

                // set wake time.
                time_t wake_time = rtc.getTimeRTC();
                rtc.clearAlarm0(true);// == set mfp high == power off, but seal on still enabled. 
                rtc.setAlarm0(wake_time+seconds); // sets MFP high, goes low on alarm.= power on device
                delay(100);
                
                // power down
                digitalWrite(pin_seal_on,LOW);
                
                // delay waiting for power off. 
                while(i++ < 10){ Serial.print("\n");Serial.print(i);delay(1000); }
                
                // execution should only get here if there was an interrupt while
                // trying to shut down.
                
                // turn sealon back on
                digitalWrite(pin_seal_on,HIGH);
                
                Serial.print("\nShutdown failed");
        }
        // reset the RTC, pulling MFP pin low to ensure no brownout 
        rtc.clearAlarm0(false); // set MFP pin low
        
        // sleep to wake up from scratch. 
        ESP.deepSleep(100); // microseconds.
}



