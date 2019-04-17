
/*
==================================================================
        wifi provisioning functions
==================================================================  */

void ascii_string_to_char(String str,char rtn_c[]);
void web_server_start_ap();
void web_server_404();
void web_server_root();
void web_server_jquery();
void web_server_css();
void web_server_get_ssid();
void web_server_get_info();
void web_server_save_ssid();

void web_server_start_ap()
{
        current_mode = MODE_WIFI_AP_ON;
        device_led_setmode(MODE_WIFI_AP_ON);
        device_reset_max_timer();
        max_on_ms = max_ap_on_ms;
        
        char ap_name[50] = {};
        sprintf(ap_name,"watervault-%02X:%02X:%02X:%02X:%02X:%02X",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
        Serial.print("\nStaring AP: ");Serial.print(ap_name);
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(ap_name);
        // if DNSServer is started with "*" for domain name, it will reply with
        // provided IP to all DNS request
        dnsServer.start(DNS_PORT, "*", apIP);

        webServer.on ( "/", web_server_root );
        webServer.on ( "/provision/jquery.js", web_server_jquery );
        webServer.on ( "/provision/mini-default.css", web_server_css );
        webServer.on ( "/favicon.ico", web_server_404 );
        webServer.on ( "/provision/ajax/get_ssid", web_server_get_ssid );
        webServer.on ( "/provision/ajax/get_info", web_server_get_info );
        webServer.on ( "/provision/ajax/save_ssid", web_server_save_ssid );
        
        webServer.onNotFound (web_server_root);
        
        webServer.begin();
}

void web_server_404()
{
        Serial.print("\nweb_server_404");
        webServer.send ( 404, "text/html", "file not found");
        Serial.print(" - sent");
}

void web_server_root()
{
        Serial.print("\nweb_server_root");
        webServer.sendContent_P(html_index, sizeof(html_index));
        Serial.print(" - sent");
        device_reset_max_timer();
}

void web_server_jquery()
{
        //HTTP/1.1 200 OK\r\nContent-Encoding:gzip\r\ntext/javascript\r\n\r\n
        Serial.print("\nweb_server_jquery");
        webServer.sendContent_P(jQuery, sizeof(jQuery));
        Serial.print(" - sent");
}

void web_server_css()
{
        //HTTP/1.1 200 OK\r\nContent-Encoding:gzip\r\nContent-Type:text/css\r\n\r\n
        Serial.print("\nweb_server_css");
        webServer.sendContent_P(mini_css, sizeof(mini_css));
        Serial.print(" - sent");
}

void web_server_get_ssid()
{
        Serial.print("\nweb_server_css");
        int scan_count = WiFi.scanNetworks();
        int json_length = 0;
        char json[100] = {};
        
        json_length = sprintf(json,"HTTP/1.1 %d OK\r\nContent-Type:application/json\r\n\r\n",200);
        webServer.sendContent_P(json, json_length);
        Serial.print("\n");
        Serial.print(json);
        
        json_length = sprintf(json,"{\"count\":%d%s",scan_count,(scan_count > 0? ",\"data\":[":"}" ));
        webServer.sendContent_P(json, json_length);
        Serial.print(json);
        
        if (scan_count > 0)
        {
                int scan_index = 0;
                while(scan_index < scan_count)
                {
                        json_length = sprintf(json,"{\"ssid\":\"%s\",\"rssi\":%d,\"is_secure\":%d},",WiFi.SSID(scan_index).c_str(),WiFi.RSSI(scan_index),(WiFi.encryptionType(scan_index) == ENC_TYPE_NONE)? 0:1);
                        webServer.sendContent_P(json, json_length);
                        Serial.print(json);
                        scan_index++;
                }
                
                json_length = sprintf(json,"{}%s}","]");
                webServer.sendContent_P(json, json_length);
                Serial.print(json);
        }
        
        Serial.print(" - sent");
        device_reset_max_timer();
}

void web_server_get_info()
{
        Serial.print("\nweb_server_get_info");
        int json_length = 0;
        char json[100] = {};
        
        json_length = sprintf(json,"HTTP/1.1 %d OK\r\nContent-Type:application/json\r\n\r\n",200);
        webServer.sendContent_P(json, json_length);
        
        json_length = sprintf(json,"{\"device_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"device_batt\":\"%0.2f\",\"ver\":\"V%d\"}",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5],DEVICE_VBAT,DEVICE_VERSION);
        webServer.sendContent_P(json, json_length);
        Serial.print(" - sent");
        device_reset_max_timer();
}

void web_server_save_ssid()
{
        //HTTP/1.1 200 OK\r\nContent-Encoding:gzip\r\nContent-Type:text/css\r\n\r\n
        Serial.print("\nweb_server_save_ssid");
        Serial.print("\nSSID: ");Serial.print(webServer.arg("ssid"));
        Serial.print("\nPSK: ");Serial.print(webServer.arg("psk"));
        
        
        String ssid_s = webServer.arg("ssid");
        ascii_string_to_char(webServer.arg("ssid"),ee_config.ssid);
        ascii_string_to_char(webServer.arg("psk"),ee_config.psk);
        
        
        Serial.print("\nSSID c: ");Serial.print(ee_config.ssid);
        Serial.print("\nPSK c: ");Serial.print(ee_config.psk);
        ee_config.is_valid = struct_flag_is_valid;
        
        // save configs. 
        eeprom_write_raw(ee_config);
        
        int json_length = 0;
        char json[100] = {};
        
        json_length = sprintf(json,"HTTP/1.1 200 OK\r\nContent-Type:application/json\r\n\r\n{\"result\":\"success\"}");
        webServer.sendContent_P(json, json_length);
        
        Serial.print(" - sent");
        
        // show connecting status led
        device_led_setmode(MODE_WIFI_STA_CONNECTING);
        
        // give the browser a chance to show result page. 
        int i = 0;
        while(i++ < 50)
        {
                Serial.print(".");
                delay(100);
        }
        
        // reboot now. 
        device_sleep(0);
}

void ascii_string_to_char(String str,char rtn_c[])
{
        int i = 0;
        int j = 0;
        char ascii_value[5] = {};
        while(i < str.length() )
        {
                sprintf(ascii_value,"%s",str.substring(i,i+2).c_str());
                //Serial.print(ascii_value);Serial.print(" ");Serial.print( strtol(ascii_value, 0, 16) );Serial.print(", ");
                rtn_c[j++] = char(strtol(ascii_value, 0, 16));
                i++;i++;
        }
        // terminate the char string. 
        rtn_c[j] = 0x00;
}