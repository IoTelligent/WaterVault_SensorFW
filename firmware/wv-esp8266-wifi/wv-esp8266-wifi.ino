#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <WiFiClientSecure.h>
#include <ESP8266httpUpdate.h>

#include <Wire.h>
#include <EEPROM.h>
#include <TimeLib.h>
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

/*
==================================================================
        channel states
==================================================================  */
#define CHANNEL_NO_EVENT                0x00
#define CHANNEL_EVENT_START             0x01
#define CHANNEL_EVENT_END               0x02
#define CHANNEL_EVENT_CONTINUE          0x03

byte cha_event_status   = CHANNEL_NO_EVENT;
byte chb_event_status   = CHANNEL_NO_EVENT;





#include "Adafruit_MCP23008.h";
Adafruit_MCP23008 mcp;
bool MCP23008_DI_STATE[] = {0,0,0,0,0,0,0,0};

#define LM75B_address                    0x48
#define HD1080_address                   0x40
byte temp_sensor_type                   = LM75B_address;


char DEVICE_MODEL[]                             = {"wv-esp8266-wifi"};// this determines firmware too.
int DEVICE_VERSION                              = 5;
byte DEVICE_MAC[6]                              = {};
float DEVICE_VBAT;
float DEVICE_TEMP;
float DEVICE_RH;
const char* endpoint_host                       = "devices.getiotelligent.com";
const int endpoint_port                         = 443;


byte struct_flag_is_valid                       = 0xA5;
struct EEPROM_CONFIGS
{
        byte is_valid                           = 0x00; // 0xA5 == valid. 
        char ssid[50]                           = {};
        char psk[50]                            = {}; 
};
EEPROM_CONFIGS ee_config;

struct SRAM_STORAGE
{
        byte is_valid                           = 0x00; // 0xA5 == valid. 
        byte cha_state                          = 0x00;
        byte chb_state                          = 0x00;
        time_t cha_event_start_epoch            = 0x00;
        time_t chb_event_start_epoch            = 0x00;
        time_t cha_event_end_epoch              = 0x00;
        time_t chb_event_end_epoch              = 0x00;
        time_t time_sync_epoch                  = 0x00;
        time_t last_checkin_epoch               = 0x00;
        byte do_update                          = 0x00;
};
byte sram_address = 0x00;
SRAM_STORAGE sram_state;
SRAM_STORAGE current_state;



const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);


mcp7940 rtc = mcp7940();
time_t min_time = 1529560715;
unsigned long active_sleep_seconds = 120;
unsigned long standard_sleep_seconds = 86400;
unsigned long max_ap_on_ms = 120000;
unsigned long max_on_ms = 20000;
unsigned long startup_ms = 0;

void device_sleep(unsigned long seconds);
void device_reset_max_timer();
void device_led_setmode(byte led_mode);

#include "eeprom_read_write.h"
/* web server */
#include "jquery.h"
#include "html_source.h"
#include "mini_css.h"
#include "web_server.h"

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
                
        // mode button
                pinMode(pin_mode,INPUT);
                
        // read battery
                // voltage divider is 100k | 12k
                // ratio is .107 of vin.
                // ADC is 10bit, max == 1023
                // ADC range is 0-1v.
                // 1023 * .107 * 1 = 109.5
                DEVICE_VBAT = (analogRead(pin_vbat) / 109.5);
                Serial.print("\nBattery Level: ");Serial.print(DEVICE_VBAT,1);
                
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
                if (wake_time < min_time)
                {
                        Serial.print("\nsetting rtc to = ");Serial.print(min_time);
                        wake_time = min_time;
                        rtc.setTimeRTC(wake_time);
                }
                
                rtc.clearAlarm0(true);// == set mfp high == power off, but seal on still enabled. 
                rtc.setAlarm0(wake_time+120); // sets MFP high, goes low on alarm.= power on device
                
                Serial.print("\nRTC wake time: ");Serial.print(wake_time);

                rtc.read_raw(sram_address,sram_state);
                if (sram_state.is_valid != struct_flag_is_valid)
                {
                        Serial.print("\nSRAM not init");
                        SRAM_STORAGE sram_default;
                        sram_default.is_valid = struct_flag_is_valid;
                        rtc.write_raw(sram_address,sram_default);
                        rtc.read_raw(sram_address,sram_state);
                }
                
                Serial.print("\nSRAM is_valid:  ");Serial.print(sram_state.is_valid,HEX);
                Serial.print("\nSRAM cha_state: ");Serial.print(sram_state.cha_state,HEX);
                Serial.print("\nSRAM chb_state: ");Serial.print(sram_state.chb_state,HEX);
                Serial.print("\nSRAM cha_event_start_epoch: ");Serial.print(sram_state.cha_event_start_epoch);
                Serial.print("\nSRAM cha_event_end_epoch:   ");Serial.print(sram_state.cha_event_end_epoch);
                Serial.print("\nSRAM chb_event_start_epoch: ");Serial.print(sram_state.chb_event_start_epoch);
                Serial.print("\nSRAM chb_event_end_epoch:   ");Serial.print(sram_state.chb_event_end_epoch);
                Serial.print("\nSRAM time_sync_epoch:   ");Serial.print(sram_state.time_sync_epoch);
                Serial.print("\nSRAM do_update:   ");Serial.print(sram_state.do_update);
                
        // init MCP2308
                mcp.begin(0);
                mcp.pinMode(bit_mcp23008_CHA, INPUT);//mcp.pullUp(bit_mcp23008_CHA,HIGH);
                mcp.pinMode(bit_mcp23008_CHB, INPUT);//mcp.pullUp(bit_mcp23008_CHB,HIGH);
                mcp.pinMode(bit_mcp23008_LED_BLUE, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                mcp.pinMode(bit_mcp23008_LED_GREEN, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_GREEN, HIGH);
                mcp.pinMode(bit_mcp23008_LED_RED, OUTPUT);mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                device_led_setmode(MODE_INIT);
                
        // error here since LEDs are init for no RTC.
                if (!has_rtc)
                {
                        while(1)
                        {
                                mcp.digitalWrite(bit_mcp23008_LED_RED, HIGH);
                                mcp.digitalWrite(bit_mcp23008_LED_BLUE, LOW);
                                delay(1000);
                                mcp.digitalWrite(bit_mcp23008_LED_BLUE, HIGH);
                                mcp.digitalWrite(bit_mcp23008_LED_RED, LOW);
                                delay(1000);
                        }
                }
                        
                
                // enable MCP23008 interrupts.
                mcp.write8(MCP23008_GPINTEN, 0b11000000); // set interupt enable for 6,7;
                mcp.write8(MCP23008_INTCON, 0x00); // 0 == compare to prev value, 1 = compare to DEVAL;
                mcp.write8(0x05, 0b00000100); // configure int pin as active low.
                
        // determine why device is awake;
                device_check_interrupt();
                
        // init eeprom
                EEPROM.begin(512);
                
        // read in configs.
                eeprom_read_raw(ee_config);
                
                Serial.print("\nee_config.is_valid: 0x");Serial.print(ee_config.is_valid,HEX);
                
                // check for mode button, or no config.
                if (ee_config.is_valid != struct_flag_is_valid || !digitalRead(pin_mode) )
                {
                        // config is not valid.
                        web_server_start_ap();
                }
                else
                {
                        // check for event start.
                        if (
                                // if there is no sram end time.
                                // && there is no do_update flag
                                (sram_state.cha_event_end_epoch < 1 && sram_state.chb_event_end_epoch < 1 && sram_state.do_update != 0x01)
                                &&      (
                                                // both channels are event start. 
                                                (cha_event_status == CHANNEL_EVENT_START && chb_event_status == CHANNEL_EVENT_START)
                                        
                                                // only cha a event start.
                                        ||      (cha_event_status == CHANNEL_EVENT_START && chb_event_status == CHANNEL_NO_EVENT)
                                                
                                                // only chb event start.
                                        ||      (cha_event_status == CHANNEL_NO_EVENT && chb_event_status == CHANNEL_EVENT_START)
                                        
                                                // checkin only, but timer has not expired
                                        ||      (cha_event_status == CHANNEL_NO_EVENT && chb_event_status == CHANNEL_NO_EVENT && (wake_time - sram_state.last_checkin_epoch < (24*60*60)) )
                                        )
                           )
                                {
                                        // this is an event start
                                        // and there are no current end events
                                        // save sram and sleep.
                                        rtc.write_raw(sram_address,sram_state);
                                        
                                        // this this is a no event for both channels,
                                        // then sleep for extended duration.
                                        if (cha_event_status == CHANNEL_NO_EVENT && chb_event_status == CHANNEL_NO_EVENT)
                                        {
                                                Serial.print("\nDetected no events, and checkin time has not expired, sleep for renaming checkin duration");
                                                if (wake_time > sram_state.last_checkin_epoch)
                                                {
                                                        device_sleep(standard_sleep_seconds - (wake_time - sram_state.last_checkin_epoch) + 1);
                                                        return;
                                                }
                                                device_sleep(standard_sleep_seconds);
                                                return;
                                        }
                                        
                                        // most events will end up here. 
                                        device_sleep(active_sleep_seconds);
                                        return;
                                }
                        
                        // any other events are a publish. 
                        else
                        {
                                WiFi.mode(WIFI_STA);
                                
                                device_led_setmode(MODE_WIFI_STA_CONNECTING);
                                
                                Serial.print("\nee_config.ssid: ");Serial.print(ee_config.ssid);
                                Serial.print("\nee_config.psk: ");Serial.print(ee_config.psk);
                                
                                WiFi.begin(ee_config.ssid, ee_config.psk);
                                
                                // Wait for connection
                                unsigned long timeout_ms = millis(); 
                                while (WiFi.status() != WL_CONNECTED)
                                {
        
                                        delay(1000);
                                        Serial.print(".");
                                        
                                        if (!digitalRead(pin_mode)) 
                                        {
                                                web_server_start_ap();
                                                break;
                                        }
                                        
                                        if (millis() - timeout_ms > 30000)
                                        {
                                                Serial.print("Connection Failed");
                                                device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                                                rtc.write_raw(sram_address,sram_state);
                                                delay(1000);
                                                device_sleep(active_sleep_seconds);
                                        }
                                }
                                
                                // ensure mode button was not pressed while starting up.
                                if (current_mode != MODE_WIFI_AP_ON)
                                {
                                        device_led_setmode(MODE_WIFI_STA_CONNECTED);
                                        
                                        Serial.print("\nIP address: ");
                                        Serial.print(WiFi.localIP());
                                        
                                        if (sram_state.do_update == 0x01)
                                        {
                                                device_do_update();
                                        }
                                        else
                                        {
                                                device_read_temps(); 
                                                device_publish();
                                        }
                                        
                                        // execution will never get here. 
                                }
                                
                                // execution only here if button pressed while trying to connect. 
                        }
                
                }
                
                device_reset_max_timer();
}

bool last_pin_mode = false;
void loop()
{
        if ((millis() - startup_ms) > max_on_ms )
        {
                Serial.print("\nshutting down due to max on duration exceeded");
                device_sleep(active_sleep_seconds);
                return;
        }
        
        if (current_mode == MODE_WIFI_AP_ON)
        {
                dnsServer.processNextRequest();
                webServer.handleClient();
                return;
        }
        
        if (last_pin_mode != digitalRead(pin_mode))
        {
                Serial.print("\nMode button state: ");Serial.print( digitalRead(pin_mode)? "Off":"On" );
                last_pin_mode = digitalRead(pin_mode);
        }
                
        
        if (!digitalRead(pin_mode)) 
        {
                web_server_start_ap();
        }
}

/*
==================================================================
        device_read_temps
==================================================================  */
void device_read_temps()
{
        Wire.beginTransmission(LM75B_address);
        byte error = Wire.endTransmission();

        if (error == 0)
        {
                Serial.print("\nFound LM758");
                int16_t data = 0;
                Wire.requestFrom(LM75B_address,2);
                if(Wire.available())
                {
                        data = (Wire.read() << 8) | Wire.read();
                }
        
                DEVICE_TEMP = ((data>>5)*0.125);
                
                return;
        }
        /*
        Wire.beginTransmission(HD1080_address);
        error = Wire.endTransmission();
      
        if (error == 0)
        {
                temp_sensor_type = HD1080_address;
                Serial.print("\nFound HD1080");
                HDC1000 hd1080(HD1080_address);
                hd1080.begin();
                DEVICE_TEMP = hd1080.getTemp();
                DEVICE_RH = hd1080.getHumi();
        }
        */
}


/*
==================================================================
        device_publish
==================================================================  */
void device_publish()
{
        //device_time_sync();
        
        WiFiClientSecure client;
        client.setInsecure();
        Serial.print("\nConnecting to host ");Serial.print(endpoint_host);
        
        if (!client.connect(endpoint_host, endpoint_port))
        {
                Serial.print(" - connection failed");
                device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                rtc.write_raw(sram_address,sram_state);
                delay(1000);
                device_sleep(active_sleep_seconds);
                return;
        }
        
        Serial.print(" - Connected");
    
        // Verify validity of server's certificate
        /*
                not doing cert verification
                it's dependentt on current time being correct
                there is no way to set the esp8266 time with out ntp time server for every request.
        
        if (!client.verifyCertChain(endpoint_host))
        {
                  Serial.print(" - certificate verification failed!");
                  device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                  rtc.write_raw(sram_address,sram_state);
                  delay(1000);
                  device_sleep(active_sleep_seconds);
                  return;
        }
        */
        time_t rtc_time = rtc.getTimeRTC();
        
        char url[55] = {};
        sprintf(url,"POST /dom/id/%02x%02x%02x%02x%02x%02x HTTP/1.1\r\n",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
                
        // header. 
        client.print(url);
        client.print("Content-type:application/json\r\n");
        client.print("Accept:application/json\r\n");
        client.print("Device-model:");client.print(DEVICE_MODEL);client.print("\r\n");
        client.print("Device-version:");client.print(DEVICE_VERSION);client.print("\r\n");
        client.print("Host:");client.print(endpoint_host);client.print("\r\n");
        client.print("Connection: close\r\n");
        
        // build payload. 
        char payload[310] = {};
        char internal_sensor_data[100] = {};
        char flow_sensor_data[200] = {};
            
        device_read_temps();
        if (temp_sensor_type == HD1080_address)
        {
                sprintf(internal_sensor_data,"\"itmp\":\"%0.2f\",\"irh\":\"%0.2f\",\"vbat\":\"%0.2f\",\"rssi\":\"%d\"",DEVICE_TEMP,DEVICE_RH,DEVICE_VBAT,WiFi.RSSI());
        }
        else
        {
                sprintf(internal_sensor_data,"\"itmp\":\"%0.2f\",\"vbat\":\"%0.2f\",\"rssi\":\"%d\"",DEVICE_TEMP,DEVICE_VBAT,WiFi.RSSI());
        }
                
        unsigned long post_publish_sleep_duration = active_sleep_seconds;
    
        // if all channels are no_event, and no event end.
        // this is a checkin only. 
        if (cha_event_status == CHANNEL_NO_EVENT && chb_event_status == CHANNEL_NO_EVENT && sram_state.cha_event_end_epoch < 1 && sram_state.chb_event_end_epoch < 1)
        {
                // publish checkin.
                
                // no additional data needed.
                
                post_publish_sleep_duration = standard_sleep_seconds;
        }
        else
        {
                sprintf(flow_sensor_data,",\"d\":[{\"id\":\"a\",\"start_epoch\":\"%d\",\"end_epoch\":\"%d\",\"sns\":%d},{\"id\":\"b\",\"start_epoch\":\"%d\",\"end_epoch\":\"%d\",\"sns\":%d}]",
                        sram_state.cha_event_start_epoch,
                        (cha_event_status == CHANNEL_EVENT_CONTINUE? rtc_time:sram_state.cha_event_end_epoch),
                        sram_state.cha_state,
                        sram_state.chb_event_start_epoch,
                        (chb_event_status == CHANNEL_EVENT_CONTINUE? rtc_time:sram_state.chb_event_end_epoch),
                        sram_state.chb_state);
                
                // clear events if this is an end.
                if (cha_event_status == CHANNEL_EVENT_END)
                {
                        sram_state.cha_event_start_epoch = 0x00;
                        sram_state.cha_event_end_epoch = 0x00;
                }
                
                if (chb_event_status == CHANNEL_EVENT_END)
                {
                        sram_state.chb_event_start_epoch = 0x00;
                        sram_state.chb_event_end_epoch = 0x00;
                }
                
                if ( (cha_event_status == CHANNEL_NO_EVENT || cha_event_status == CHANNEL_EVENT_END) && (chb_event_status == CHANNEL_NO_EVENT || chb_event_status == CHANNEL_EVENT_END) )
                {
                        post_publish_sleep_duration = standard_sleep_seconds;
                }
        }
        
        int content_length = sprintf(payload,"{%s%s}",internal_sensor_data,flow_sensor_data);
        
        client.print("Content-length:");client.print(content_length);client.print("\r\n");
        client.print("\r\n");
        client.print(payload);
        
        // get reply from server.
        unsigned long reply_timeout_ms = millis();
        while (!client.available() && (millis() - reply_timeout_ms < 20000))
        {
                delay(100);
        }
        
        String server_result = "";
        if (client.available())
        {
                reply_timeout_ms = millis();
                client.setTimeout(100);
                while((millis() - reply_timeout_ms < 100) && server_result.length() < 1)
                {
                        if (client.available())
                        {
                                server_result = client.readStringUntil('\n');
                                if (server_result.indexOf("epoch\":") > -1)
                                {
                                        break; 
                                }
                                else
                                {
                                        server_result = "";
                                        reply_timeout_ms = millis();
                                }
                        }
                        else
                        {
                                Serial.print(".");
                                delay(10);
                        }
                }
        }
                   
        Serial.print("\nResult: ");Serial.print(server_result);
        if (server_result.indexOf("epoch\":") > -1 )
        {
                time_t server_time = atol(server_result.substring((server_result.indexOf("epoch\":")+7), server_result.indexOf(",",(server_result.indexOf("epoch\":")+7)) ).c_str());
                time_t rtc_time = rtc.getTimeRTC();
                Serial.print("\nServer Time: ");Serial.print(server_time);
         
                if (server_time > min_time)
                {
                        if (((server_time > rtc_time) && server_time - rtc_time > 10) || ((rtc_time > server_time) && rtc_time - server_time > 10))
                        {
                                rtc.setTimeRTC(server_time+1);
                                Serial.print(" updating rtc time to server time");
                        }
                        else
                        {
                                Serial.print(" server time delta less then 10 no action");
                        }
                }
                else
                {
                        Serial.print(" server time is not valid");
                }
        }
                
        
        // last check in time. 
        sram_state.last_checkin_epoch = rtc_time;
        
        rtc.write_raw(sram_address,sram_state);
        
        // check for update command.
        if (server_result.indexOf("do_update") > 0)
        {
                // last check in time. 
                sram_state.do_update = 0x01;
                rtc.write_raw(sram_address,sram_state);
                
                device_sleep(10);
                // reboot now. 
        }
        
        // check for mode button.
        if (!digitalRead(pin_mode)) 
        {
                web_server_start_ap();
        }
        
        // double check interrupts before sleeping.
        if (device_check_interrupt())
        {
                // interrupt detected.
                Serial.print("\nInterrupt detected after publish.");
                
                device_sleep(0);
                return; 
        }
        
        device_sleep(post_publish_sleep_duration);
}

void device_do_update()
{
        device_led_setmode(MODE_FIRMWARE_UPDATE_IP);
        
        sram_state.do_update = 0x00;
        sram_state.last_checkin_epoch = 0x00;
        rtc.write_raw(sram_address,sram_state);
                
        WiFiClientSecure client;
        client.setInsecure();
        
        Serial.print("\nConnecting to host for update ");Serial.print(endpoint_host);
  
        if (!client.connect(endpoint_host, endpoint_port))
        {
                Serial.print(" - connection failed");
                device_led_setmode(MODE_WIFI_STA_CONNECTION_FAILED);
                rtc.write_raw(sram_address,sram_state);
                delay(1000);
                device_sleep(10);
                return;
        }
        
        Serial.print(" - Connected");
        

        Serial.print("\nUpdate result: ");
        char url[100] = {};
        sprintf(url,"/firmware/model/esp8266/id/%02x%02x%02x%02x%02x%02x/file/ota",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
        
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

/*
void device_time_sync()
{
        time_t rtc_time = rtc.getTimeRTC();
        if (sram_state.time_sync_epoch < 1 || (rtc_time < sram_state.time_sync_epoch) || (rtc_time - sram_state.time_sync_epoch > (24 * 60 * 60)) || rtc_time < 1)
        {
                Serial.print("\nSetting time using SNTP");
                configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
                time_t now_t = time(nullptr);
                unsigned long sync_ms = millis();
                while (now_t < 8 * 3600 * 2)
                {
                        delay(500);
                        Serial.print(".");
                        now_t = time(nullptr);
                        
                        if (millis() - sync_ms > 30000)
                        {
                                Serial.print("\nTime NTP sync failed");
                                return;
                        }
                }
                Serial.print("\nNew epoch time: ");Serial.print(now_t);
                
                rtc.setTimeRTC(now_t);
                sram_state.time_sync_epoch = now_t;
                
                time_t rtc_time = rtc.getTimeRTC();
                time_t pnow = time(nullptr);
                
                Serial.print("\nPost Sync RTC time: ");Serial.print(rtc_time);
                Serial.print("\nPost Sync now: ");Serial.print(pnow);
        }
        else
        {
                Serial.print("\nDevice time sync. Last sync < 24 hours @ ");Serial.print(rtc_time - sram_state.time_sync_epoch);Serial.print(" seconds old");
        }
}
*/

bool device_read_input_state()
{
        current_state.cha_state = 0;
        current_state.chb_state = 0;
        
        // debounce input. 
        byte i = 0;
        while(i++ < 10)
        {
                current_state.cha_state += mcp.digitalRead(bit_mcp23008_CHA);
                current_state.chb_state += mcp.digitalRead(bit_mcp23008_CHB);
                delay(10);
        }
        
        current_state.cha_state = current_state.cha_state > 5? 1:0;
        current_state.chb_state = current_state.chb_state > 5? 1:0;
        
        Serial.print("\ncurrent_state.cha_state: ");Serial.print(current_state.cha_state);
        Serial.print("\ncurrent_state.chb_state: ");Serial.print(current_state.chb_state);
}

bool device_check_interrupt()
{
        bool return_interrupt_state = false;
        
        device_read_input_state();
        
        time_t now_epoch = rtc.getTimeRTC();
        
        /*
        ==================================================================
                ch a events
        ==================================================================  */
        // detect event start. 
        if (current_state.cha_state != sram_state.cha_state && current_state.cha_state < 1)
        {
                // event start.
                Serial.print("\nCHA event start");
                cha_event_status = CHANNEL_EVENT_START;
                sram_state.cha_event_start_epoch = now_epoch;
                sram_state.cha_event_end_epoch = 0x00;
                sram_state.cha_state = current_state.cha_state;
                
                return_interrupt_state = true;
        }
        
        // detect event end. 
        else if (current_state.cha_state != sram_state.cha_state && current_state.cha_state > 0)
        {
                Serial.print("\nCHA event end");
                cha_event_status = CHANNEL_EVENT_END;
                sram_state.cha_state = current_state.cha_state;
                sram_state.cha_event_end_epoch = now_epoch;
                
                return_interrupt_state = true;
        }
        
        // detect event continue. 
        else if (current_state.cha_state == sram_state.cha_state && current_state.cha_state < 1)
        {
                Serial.print("\nCHA event duration");
                cha_event_status = CHANNEL_EVENT_CONTINUE;
        }
        
        // detect no event.
        else
        {
                Serial.print("\nCHA no event");
                cha_event_status = CHANNEL_NO_EVENT;
        }
        
        /*
        ==================================================================
                chb events
        ==================================================================  */
        // detect event start. 
        if (current_state.chb_state != sram_state.chb_state && current_state.chb_state < 1)
        {
                // event start.
                Serial.print("\nCHB event start");
                chb_event_status = CHANNEL_EVENT_START;
                sram_state.chb_event_start_epoch = now_epoch;
                sram_state.chb_event_end_epoch = 0x00;
                sram_state.chb_state = current_state.chb_state;
                
                return_interrupt_state = true;
        }
        
        // detect event end. 
        else if (current_state.chb_state != sram_state.chb_state && current_state.chb_state > 0)
        {
                Serial.print("\nCHB event end");
                chb_event_status = CHANNEL_EVENT_END;
                sram_state.chb_state = current_state.chb_state;
                sram_state.chb_event_end_epoch = now_epoch;
                
                return_interrupt_state = true;
        }
        
        // detect event continue. 
        else if (current_state.chb_state == sram_state.chb_state && current_state.chb_state < 1)
        {
                Serial.print("\nCHB event duration");
                chb_event_status = CHANNEL_EVENT_CONTINUE;
        }
        
        // detect no event.
        else
        {
                Serial.print("\nCHB no event");
                chb_event_status = CHANNEL_NO_EVENT;
        }
        
        return return_interrupt_state;

}
        

void device_reset_max_timer()
{
        startup_ms = millis();
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

void device_sleep()
{
        device_sleep(standard_sleep_seconds);
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



