__attribute__ ((section(".sketch_boot")))
unsigned char sduBoot[0x4000] =
{
#include "sdu_boot_source.h"
};

/*
==================================================================
        
        wifi temp sensor
        
==================================================================  */

char DEVICE_MODEL[]                             = {"wv-wifi"};// this determines firmware too.
int DEVICE_VERSION                               = 3;


/*
==================================================================
        Pin Definitions
==================================================================  */
#define pin_WINC_RESET                  30
#define pin_WINC_INT                    0
#define pin_WINC_CS                     2
#define pin_WINC_EN                     5

#define pin_RGB                         11

#define pin_SEAL_IN                     A2

#define pin_FLASH_SS                    13

#define pin_Vbat_Sense                  A7

#define pin_SensorLatch                 A5
#define pin_Sensor                      26
#define pin_SensorReset                 4

#define pin_Button                      3



/*
==================================================================
        includes
==================================================================  */
#include <Wire.h>
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiSSLClient.h>

#include<SPIMemory.h>

#include <simple_sensor_mcu.h>

#include <simple_sensor_dallas_temp.h>
#include <DS2482_800.h>                 // required by simple_sensor_dallas_temp
#include <OneWire.h>                    // required by simple_sensor_dallas_temp

/* MQTT */
#define MQTT_MAX_PACKET_SIZE 255
#include <simple_mqtt.h>
#include <PubSubClient.h>               // required by simple_mqtt.

/* status LEDs */
#include <Adafruit_NeoPixel.h>


/* Cap Touch */
#include "Adafruit_FreeTouch.h"


char ap_ssid[25]                        = {"getiotelligent-connect"};
byte DEVICE_MAC[6]                      = {};

int LAST_RF_CONNECTION_STATUS = WL_IDLE_STATUS;
WiFiServer server(80);

struct config_struct
{
        byte valid;          
        char ssid[40];          
        char pass[40];           
        int sleep_s = 120;            
        byte identified = 0xFF;
        byte has_update;
        long update_length;
};
config_struct configs;

uint32_t flash_firmware_address = 0; // firmware is max 256KB
uint32_t flash_config_address = (260*1024); // this is 260KB



char list_ssid[20][32]  = {};
long list_rssi[20]      = {};
bool list_security[20]  = {};
byte list_ssid_count    = 0;
String currentLine = "";

bool is_provisioning_mode = false; 

bool IS_DEBUG = true;
int ADC_OFFSET = 0;


byte LED_WIFI   = 1;
byte LED_SENSOR   = 0;
unsigned long min_time = 1529560715;
unsigned long idle_timeout_ms = 0;


char HTTP_HOST[]                        = {"devices.getiotelligent.com"};
char HTTP_IP_ADDRESS[]                    =  {"18.216.205.1"};
int HTTP_PORT                           = 443;
byte MQTT_SERVERS_STATUS_OK             = 0x01;
byte MQTT_SERVERS_STATUS_FAIL           = 0x00;
byte MQTT_SERVERS_STATUS_UNKNOWN        = 0xFF;
byte MQTT_SERVER_STATUS                 = MQTT_SERVERS_STATUS_UNKNOWN;

bool HAS_PUBLISH = false;
bool HAS_LED_STATUS_DONE = false;
bool has_ventoried_this_cycle = false;
bool has_identified_this_cycle = false;
bool has_switch_latch = false; 

volatile byte CMD_QUE[100];
volatile byte CMD_INDEX = 0;
#define CMD_IDENTIFY            0x01
#define CMD_IVENTORY            0x02
#define CMD_PUBLISH             0x03
#define CMD_SET_RTC             0x04
#define CMD_FIRMWARE            0x05

#define SENSOR_NOT_LATCHED      0x01
#define SENSOR_LATCHED          0x00

#define SENSOR_NO_FLOW          0x01
#define SENSOR_FLOW             0x00

int sensor_latch_count = 0;
byte sensor_flow_active = 0; 

#include <TimeLib.h>
#include <mcp7940.h>
mcp7940 rtc = mcp7940();
time_t rtc_start_time;
time_t rtc_new_time;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(2, pin_RGB, NEO_GRB + NEO_KHZ800);
SPIFlash flash(pin_FLASH_SS);

WiFiSSLClient http_client;

#include "wifi_provisioning.h"
#include <simple_led_helper.h>
#include "http_helper.h"
#include "internal_sensors.h"


#define SDU_START    0x2000
#define SDU_SIZE     0x4000
#define SKETCH_START (uint32_t*)(SDU_START + SDU_SIZE)

void setup()
{
        // seal on the Q4
        pinMode(pin_SEAL_IN,OUTPUT);
        digitalWrite(pin_SEAL_IN,HIGH);
        
        currentLine.reserve(200);
        Serial.begin(115200);
        
        //delay(5000);
        // set up analog resolution.
        analogReadResolution(12);

        Wire.begin();
        
        Serial.print("\nStart");
        
        // read in configs and wifi credenials.
        flash.begin();
        
        
        flash.readAnything(flash_config_address, configs);
        if (configs.sleep_s < 5){ configs.sleep_s = 10; }
        
        pinMode(pin_SensorLatch,INPUT_PULLUP);
        pinMode(pin_Sensor,INPUT_PULLUP);
        pinMode(pin_SensorReset, OUTPUT);
        pinMode(pin_Button, INPUT_PULLUP);

        //configs.sleep_s = 10;
        /*
        configs.sleep_s = 900;            
        configs.chan_a_enabled = 0x01;
        configs.chan_b_enabled = 0x01;
        configs.has_update = 0x01;
        */
        
        //firmware_status.has_update = 0x01;
        //flash.eraseSection(flash_config_address, sizeof(configs));
        //flash.writeAnything(flash_config_address, configs);
        //flash.readAnything(flash_firmware_status_address, firmware_status);
        
        //configs.has_update = 0x01;
        //flash.writeAnything(flash_config_address, configs);
        
        /*
        ==================================================================
                
                Hard coded sleep_s to 120, change this lagter, perahps
                make it a downloadable config.
                
        ==================================================================  */
        configs.sleep_s = 120;
        
        Serial.print("\nconfigs.valid: ");Serial.print(configs.valid);
        Serial.print("\nconfigs.sleep_s: ");Serial.print(configs.sleep_s);
        Serial.print("\nconfigs.ssid: ");Serial.print(configs.ssid);
        Serial.print("\nconfigs.pass: ");Serial.print(configs.pass);
        Serial.print("\nconfigs.identified: ");Serial.print(configs.identified);
        
        
        Serial.print("\n");
        Serial.print("\nconfigs.has_update: ");Serial.print(configs.has_update, HEX);
        Serial.print("\nconfigs.update_length: ");Serial.print(configs.update_length);
        
        Serial.print("\nStart time: ");Serial.print(millis());
        Serial.print("\nModel: ");Serial.print(DEVICE_MODEL);Serial.print(" ");Serial.print(DEVICE_VERSION);
        Serial.print("\n");

        // RGB Pixels
        pixels.begin();
        simple_led_helper_set_intensity((byte)10);
        simple_led_helper_begin(LED_WIFI);
        simple_led_helper_begin(LED_SENSOR);
        simple_led_helper_set(LED_WIFI,SS_LED_OFF);
        delay(250);
        simple_led_helper_set(LED_WIFI,SS_LED_WIFI_CONNECTING);
        
        // set pins. 
        WiFi.setPins(pin_WINC_CS, pin_WINC_INT, pin_WINC_RESET, pin_WINC_EN);
        
        // check if winc present.
        if (WiFi.status() == WL_NO_SHIELD)
        {
                Serial.println("WiFi shield not present");
                // don't continue
                while (true)
                {
                        simple_led_helper_set(LED_WIFI,SS_LED_RED);
                        delay(1000);
                        simple_led_helper_set(LED_WIFI,SS_LED_BLUE);
                        delay(1000);
                }
                        
        }
        
        
        // grab mac address 
        WiFi.macAddress(DEVICE_MAC);
        
        if (!digitalRead(pin_Button))
        {
                configs.valid = 0;
        }
        
        if (configs.valid != 1)
        {
                wifi_connect(); 
        }
        
        // init RTC. 
        rtc.begin();
        rtc_start_time = rtc.getTimeRTC();
        if (rtc_start_time < min_time)
        {
                Serial.print("\nsetting rtc to = ");Serial.print(min_time);
                rtc_start_time = min_time;
                rtc.setTimeRTC(rtc_start_time);
        }
        
        // set alarm time just incase there is a brown out.
        time_t wake_time = rtc.getTimeRTC();
        rtc.clearAlarm0();// == sets mfp low == power off, but seal on still enabled. 
        rtc.setAlarm0(wake_time+60,true); // sets MFP low, goes high on alarm, high == power on device. 
        
        if (configs.identified != 0x01)
        {
                CMD_QUE[CMD_INDEX++]=CMD_IDENTIFY;
        }
        
        //
        
        internal_temp_init();
        
        // publish every cycle. 
        CMD_QUE[CMD_INDEX++]=CMD_PUBLISH;

}


unsigned long loop_t = millis(); 
void loop()
{
        if (is_provisioning_mode)
        {
                wifi_provisoining_mode_loop();
                return;
        }
        Serial.print("\nmillis: ");Serial.print(millis());
        Serial.print("\nsensor_latch_count: ");Serial.print(digitalRead(pin_SensorLatch));Serial.print(":");Serial.print(sensor_latch_count);Serial.print("\nsensor_flow_active: ");Serial.print(digitalRead(pin_Sensor));Serial.print("\n");
        // check/clear latch.
        /*
        if (digitalRead(pin_SensorLatch) == SENSOR_LATCHED)
        {
                sensor_latch_reset(); 
        }
        */
        //delay(1000);
        /*
        if (Serial.available())
        {
                while(Serial.available())
                {
                        byte c = Serial.read();
                        
                        if (c == 0x31)
                        {
                                time_t wake_time = rtc.getTimeRTC() + 10;
                                Serial.print("reset true");
                                rtc.clearAlarm0(true);// == set mfp high
                                delay(5000);
                                Serial.print("alarm true");
                                rtc.setAlarm0(wake_time,true); // sets mfp low, goes high on alarm. 
                        }
                        else if (c == 0x30)
                        {
                                time_t wake_time = rtc.getTimeRTC() + 10;
                                Serial.print("reset false");
                                rtc.clearAlarm0();// == sets mfp low
                                delay(5000);
                                Serial.print("alarm false");
                                rtc.setAlarm0(wake_time); // sets MFP high, goes low on alarm.
                        }
                        else if (c == 0x32)
                        {
                                time_t wake_time = rtc.getTimeRTC() + 10;
                                Serial.print("reset false");
                                rtc.clearAlarm0();// == sets mfp low
                                delay(5000);
                                Serial.print("alarm false");
                                rtc.setAlarm0(wake_time,true); // sets MFP low, goes high on alarm.
                        }
                }
        }
        
        //delay(500);
        //return;
        */
        
        if (CMD_INDEX > 0)
        {
                process_incoming_command();
        }
        
        if (CMD_INDEX < 1)
        {
                time_t wake_time = rtc.getTimeRTC() + configs.sleep_s;
                
                // if there is active flow, sleep breifly and recheck. 
                if (sensor_flow_active > 0)
                {
                        Serial.print("\nSleep for ");Serial.print(configs.sleep_s);
                }
                else
                {
                        // if there is no active flow, this is the end of the event, sleep for 24 hours. 
                        Serial.print("\nSleep for 24 hours");
                        wake_time = rtc.getTimeRTC() + (60 * 60 * 24);
                }
                
                simple_led_helper_set(LED_WIFI,SS_LED_OFF);
                simple_led_helper_set(LED_SENSOR,SS_LED_OFF);
                
                rtc.clearAlarm0();// == sets mfp low == power off, but seal on still enabled. 
                rtc.setAlarm0(wake_time,true); // sets MFP low, goes high on alarm, high == power on device. 
                
                // reset the latch
                digitalWrite(pin_SensorReset,HIGH);
                digitalWrite(pin_SEAL_IN,LOW);

                int i = 0;
                while( i++ < 20 )
                {
                        Serial.print("\n.");
                        delay(1000);
                }
                
                Serial.print("\nSleep Failed, reboot");
                //if execution is here for some reason the MCU did not sleep, reboot.
                NVIC_SystemReset();
        }
}

/*
void sensor_latch_reset()
{
        if (sensor_latch_count < 1)
        {
                CMD_QUE[CMD_INDEX++]=CMD_PUBLISH;
        }
        
        sensor_latch_count++;
        digitalWrite(pin_SensorReset,HIGH);
        delay(100);
        digitalWrite(pin_SensorReset,LOW);
}
*/

void process_incoming_command()
{
        byte curr_cmd_index = 0;
        while(curr_cmd_index < CMD_INDEX)
        {
                Serial.print("\nCMD_INDEX COUNT: ");Serial.print(CMD_INDEX);
                switch( CMD_QUE[curr_cmd_index] )
                {
                        case CMD_IDENTIFY:
                        {
                                Serial.print("\nCMD is Identify");
                                if (wifi_connect())
                                {
                                        if (!has_identified_this_cycle)
                                        {
                                                has_identified_this_cycle = true;
                                                
                                                IPAddress ipa= WiFi.localIP();
                                                char data[100] = {}; 
                                                sprintf(data,"{\"event\":\"identify\",\"firmware\":\"%d\",\"model\":\"%s\",\"ipa\":\"%d.%d.%d.%d\"}",DEVICE_VERSION,DEVICE_MODEL, ipa[0],ipa[1],ipa[2],ipa[3]);
                                                http_helper_publish(data);
                                                        
                                                if (configs.identified != 0x01)
                                                {
                                                        configs.identified = 0x01;
                                                        flash.eraseSection(flash_config_address, sizeof(configs));
                                                        flash.writeAnything(flash_config_address, configs);
                                                }
                                        }
                                }
                        }break;
                        
                        case CMD_PUBLISH:
                        {
                                Serial.print("\nCMD is publish");
                                if (wifi_connect() && !HAS_PUBLISH)
                                {
                                        idle_timeout_ms = millis();
                                        
                                        char data[150] = {};
                                        
                                        byte server_status = 1;
                                        sensor_flow_active = (digitalRead(pin_Sensor) == SENSOR_FLOW? 1:0);
                                        
                                        if (sensor_flow_active > 0)
                                        {
                                                simple_led_helper_set(LED_SENSOR,SS_LED_RED);
                                        }
                                        else
                                        {
                                                simple_led_helper_set(LED_SENSOR,SS_LED_GREEN);
                                        }
                                        
                                        simple_led_helper_set(LED_WIFI,SS_LED_HTTP_SENDING);
                                        sprintf(data,"{\"event\":\"data\",\"d\":{\"itmp\":\"%d\",\"vbat\":\"%d\",\"rssi\":\"%d\",\"sense_latch\":\"%d\",\"sns_flw\":\"%d\"}}",internal_temp(),internal_vbat(),WiFi.RSSI(),(digitalRead(pin_SensorLatch) == SENSOR_LATCHED? 0x01:0x00),sensor_flow_active);
                                        if (http_helper_publish(data))
                                        {
                                                simple_led_helper_set(LED_WIFI,SS_LED_WIFI_SUCCESS);
                                        }
                                        else
                                        {
                                                simple_led_helper_set(LED_WIFI,SS_LED_WIFI_ERROR);
                                                server_status = 0;
                                        }
                                        delay(1000);
                                        HAS_PUBLISH = true;
                                }
                        }break;
                        
                        case CMD_SET_RTC:
                        {
                                time_t rtc_current_time = rtc.getTimeRTC();
                                if (rtc_current_time - rtc_new_time > 30 || rtc_new_time - rtc_current_time > 30)
                                {
                                        Serial.print("\nsetting rtc to = ");Serial.print(rtc_new_time);
                                        rtc.setTimeRTC(rtc_new_time);
                                }
                                
                        }break;
                        
                        case CMD_FIRMWARE:
                        {
                                http_helper_download_firmware(DEVICE_MODEL);
                                
                        }break; 
                }
                curr_cmd_index++;
        }
        
        CMD_INDEX = 0;
}

bool wifi_connect()
{
        if (WiFi.status() == WL_CONNECTED) { return true; }
        if (configs.valid == 1)
        {
                Serial.print("\nConnecting to ");Serial.print(configs.ssid);Serial.print(" : ");Serial.print(configs.pass);
                WiFi.begin(configs.ssid, configs.pass);
                
                // wait for 10 seconds.
                unsigned long timer_s = millis();
                unsigned long pulse_s = 0;
                while (WiFi.status() != WL_CONNECTED)
                {
                        if (millis() - pulse_s > 1000)
                        {
                                Serial.print(".");
                                pulse_s = millis();
                        }
                        
                        if (millis() - timer_s > 1000)
                        {
                                break;
                        }
                }
        }
        
        if (WiFi.status() != WL_CONNECTED)
        {
                /*
                ==================================================================
                        connection failed
                ==================================================================  */
                Serial.print(" - Failed. Start connection");
                if (configs.valid == 1)
                {
                        simple_led_helper_set(LED_WIFI,SS_LED_RED);
                        delay(2500);
                }
                
                simple_led_helper_set(LED_WIFI,SS_LED_WIFI_APMODE);
                wifi_provisioning_list_networks();
                
                Serial.print("\nCreating access point named: ");
                Serial.print(ap_ssid);
        
                LAST_RF_CONNECTION_STATUS = WiFi.beginAP(ap_ssid);
                
                if ( LAST_RF_CONNECTION_STATUS != WL_AP_LISTENING)
                {
                        Serial.println("Creating access point failed");
                        // don't continue
                        while (true);
                }
        
                // wait 10 seconds for connection:
                Serial.print("\nWaiting for AP to come up");
                delay(10000);
        
                // start the web server on port 80
                server.begin();
                
                is_provisioning_mode = true;
                
                return false; 
        }
        else
        {
                simple_led_helper_set(LED_WIFI,SS_LED_WIFI_CONNECTED);
                /*
                ==================================================================
                        connection seccuess.
                ==================================================================  */
                delay(100);
                Serial.print(" - Connected");
                IPAddress ipa = WiFi.localIP();
                char ipa_c[20] = {};
                sprintf(ipa_c,"%d.%d.%d.%d",ipa[0],ipa[1],ipa[2],ipa[3]);
                Serial.print("\nIP Address: "); Serial.print(ipa_c);
                Serial.print("\nRSSI: ");Serial.print(WiFi.RSSI());
                
                return true; 
        }
}
