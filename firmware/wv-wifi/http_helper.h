bool http_helper_publish(char *data);
bool http_helper_download_firmware(char *data);


bool http_helper_publish(char *data)
{
        if (WiFi.status() != WL_CONNECTED ) { return false; }
        
        Serial.println("\nConnect ");Serial.print(HTTP_HOST);Serial.print(":");Serial.print(HTTP_PORT);
        if (http_client.connect(HTTP_IP_ADDRESS, HTTP_PORT))
        {
                //{"t":"/dom/30aea46ed338/","m":{"event":"data","addr":[0,0],"ch":0,"d":[[0,"9.19"],[0,"0.00"],[0,"-50"]]}}
                
                char payload [300] = {};
                
                int content_length = sprintf(payload,"%s",data);
                Serial.print (" - connected");
                
                Serial.print("\nContent-Length: ");Serial.print(content_length);
                Serial.print("\n");Serial.print(payload);
                
                char url[55] = {};
                sprintf(url,"POST /dom/id/%02x%02x%02x%02x%02x%02x HTTP/1.1\r\n",DEVICE_MAC[5],DEVICE_MAC[4],DEVICE_MAC[3],DEVICE_MAC[2],DEVICE_MAC[1],DEVICE_MAC[0]);
                
                //http_client.println("POST /dom HTTP/1.1");
                http_client.print(url);
                
                http_client.print("Content-type:application/json\r\n");
                http_client.print("Accept:application/json\r\n");
                http_client.print("Host:");http_client.print(HTTP_HOST);http_client.print("\r\n");
                http_client.print("Content-length:");http_client.print(content_length);http_client.print("\r\n");
                http_client.print("\r\n");
                http_client.print(payload);
                
                Serial.print("\n");
                unsigned long http_timeout = 0;
                unsigned long http_connect_timeout = millis();
                String line = "";
                String http_response = "";
                while((http_timeout < 1 || (millis() - http_timeout < 250)) && (millis() - http_connect_timeout < 10000))
                {
                        while (http_client.available())
                        {
                                line = http_client.readStringUntil('\n');
                                http_timeout = millis();
                                Serial.println(line);
                                if (line.indexOf("\"res\":") > 0)
                                {
                                        http_response = line;
                                }
                        }
                }
                
                http_client.stop();
                
                Serial.print("http_response: ");Serial.print(http_response);
                // add to cmd que.
                if (http_response.indexOf("\"cmd\":") > 0)
                {
                        //{"epoch":1538851619,"cmd":"identify","res":200}
                        if (http_response.indexOf("\"cmd\":\"identify\"") > 0)
                        {
                                CMD_QUE[CMD_INDEX++] = CMD_IDENTIFY;
                        }
                        
                        if (http_response.indexOf("\"cmd\":\"firmware\"") > 0)
                        {
                                CMD_QUE[CMD_INDEX++] = CMD_FIRMWARE;
                        }
                }
                
                if (http_response.length() > 5 && http_response.indexOf("\"epoch\":") > 0)
                {
                        Serial.print("\nepoch received");
                        rtc_new_time = strtoul(http_response.substring( http_response.indexOf("\"epoch\":")+8, http_response.indexOf("\"", http_response.indexOf("\"epoch\":")+10)).c_str(),NULL,10);
                        CMD_QUE[CMD_INDEX++] = CMD_SET_RTC;
                }
                return true;
        }
        else
        {
                Serial.print (" - failed");
                return false;
        }
        
}

bool http_helper_download_firmware(char *data)
{
        if (WiFi.status() != WL_CONNECTED ) { return false; }
        Serial.print("\nDownload Firmware");
        Serial.print("\nConnect ");Serial.print(HTTP_HOST);Serial.print(":");Serial.print(HTTP_PORT);
                        
        simple_led_helper_set(LED_WIFI,SS_LED_HTTP_SENDING);
                        
        if (http_client.connect(HTTP_IP_ADDRESS, HTTP_PORT))
        {
                Serial.print (" - connected");
                
                // GET / HTTP/1.1
                char url[55] = {};
                sprintf(url,"GET /firmware/id/%02x%02x%02x%02x%02x%02x HTTP/1.1\r\n",DEVICE_MAC[5],DEVICE_MAC[4],DEVICE_MAC[3],DEVICE_MAC[2],DEVICE_MAC[1],DEVICE_MAC[0]);
                Serial.print(url);
                http_client.print(url);
                //http_client.print("GET /firmware HTTP/1.1");http_client.print("\r\n");
                //Serial.print("\n");Serial.print("GET /firmwware HTTP/1.1");
                
                // HOST: cloud.sensorsimple.com
                http_client.print("Host:");http_client.print(HTTP_HOST);http_client.print("\r\n");
                Serial.print("Host:");Serial.print(HTTP_HOST);
                
                //mac: xxxxxxxxxxxx
                char payload[50] = {};
                sprintf(payload,"%02x%02x%02x%02x%02x%02x",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
                http_client.print("mac:");http_client.print(payload);http_client.print("\r\n");
                
                // file: abd-123
                http_client.print("model:");http_client.print(data);http_client.print("\r\n");
                
                http_client.print("Cache-Control: no-cache\r\nConnection: close\r\n\r\n");
                Serial.print("Cache-Control: no-cache\r\nConnection: close\r\n\r\n");
                
                Serial.print("\n");
                unsigned long http_timeout = millis();
                unsigned long http_connect_timeout = millis();
                String line = "";
                bool is_200_ok = false;
                bool is_content_type = false;
                long content_length = 0;
                bool header_done = false;
                while((!header_done) && (millis() - http_timeout < 500 && (millis() - http_connect_timeout < 30000)) )
                {
                        while (http_client.available())
                        {
                                line = http_client.readStringUntil('\n');
                                http_timeout = millis();
                                Serial.print("\n");Serial.print(line);
                                if (line.indexOf("HTTP/1.1") > -1)
                                {
                                        if (line.indexOf("200") > -1)
                                        {
                                                is_200_ok = true;
                                        }

                                }
                                
                                else if (line.indexOf("Content-Type:") > -1)
                                {
                                        if(line.indexOf("application/octet-stream") > 0)
                                        {
                                                is_content_type = true;
                                        }
                                }
                                
                                else if (line.indexOf("Content-Length:") > -1)
                                {
                                        content_length = strtoul(line.substring( line.indexOf(":")+1).c_str(),NULL,10);
                                }

                                else if (line.indexOf('\r') == 0)
                                {
                                        // end of headers detected;
                                        header_done = true;
                                        break;
                                }
                                
                        }
                }
                
                Serial.print("\nis_200_ok: ");Serial.print(is_200_ok);
                Serial.print("\nis_content_type: ");Serial.print(is_content_type);
                Serial.print("\ncontent_length: ");Serial.print(content_length);
                Serial.print("\nData: \n");
                
                if (is_200_ok && is_content_type && content_length > 100)
                {
                        // erase the flash
                        //flash.eraseSection(flash_firmware_address, content_length);
                        
                        long erase_pointer = 0;
                        while(erase_pointer < content_length+1024)
                        {
                                flash.eraseSection(flash_firmware_address+erase_pointer, 1024);
                                erase_pointer+=1024;
                        }
                                
                        long bytes_read = 0;
                        char b_display[3] = {};
                        bool is_green = false;
                        bool bytes_verified = true;
                        http_connect_timeout = millis();
                        http_timeout = millis();
                        while(millis() - http_timeout < 500 && (millis() - http_connect_timeout < 30000))
                        {
                                while (http_client.available())
                                {
                                        byte b = http_client.read();
                                        
                                        if (!flash.writeByte(flash_firmware_address+bytes_read, b, true) && bytes_verified)
                                        {
                                                Serial.print("\nFirst verification error at ");Serial.print(flash_firmware_address+bytes_read);
                                                bytes_verified = false;
                                        }
                                        
                                        /*    
                                        if (bytes_read % 15 == 0)
                                        {
                                                Serial.print("\n");
                                                
                                        }
                                        if (bytes_read % 511 == 0)
                                        {
                                                Serial.print("\n\n");Serial.print(bytes_read);Serial.print("\n");
                                                
                                        }
                                        
                                        
                                        //sprintf(b_display,"%02X ",b);
                                        //Serial.print(b_display);
                                        */
                                        bytes_read++;
                                        
                                        if (bytes_read % 4096 == 0)
                                        {
                                                if (is_green)
                                                {
                                                        is_green = false;
                                                        simple_led_helper_set(LED_WIFI,SS_LED_WHITE);
                                                }
                                                else
                                                {
                                                        is_green = true;
                                                        simple_led_helper_set(LED_WIFI,SS_LED_WIFI_SUCCESS);
                                                }
                                        }
                                        
                                        http_timeout = millis();
                                }
                        }
                        Serial.print("\nBytes_read: ");Serial.print(bytes_read);
                        Serial.print("\nbytes_verified: ");Serial.print(bytes_verified);
                        
                        if (bytes_read != content_length)
                        {
                                Serial.print("\nBytes read does not match content length");
                        }
                        else if (!bytes_verified)
                        {
                                Serial.print("\nBytes write failed verification");
                        }
                        else
                        {
                                configs.has_update = 0x01;
                                configs.update_length = bytes_read;
                                configs.identified = 0x00;
                                flash.eraseSection(flash_config_address, sizeof(configs));
                                flash.writeAnything(flash_config_address, configs);
                                
                                
                                // reboot after 5 seconds. 
                                //time_t wake_time = rtc.getTimeRTC() + 5;
                                
                                rtc.clearAlarm0(true); // sets alarm button high to seal on power. 
                                //rtc.setAlarm0(wake_time,true); // sets MFP low, goes high on alarm, high == power on device.
                
                                NVIC_SystemReset();
                                
                                //digitalWrite(pin_SEAL_IN,LOW);
                        }
                }

                                
        }
        
        Serial.print (" - failed");
        simple_led_helper_set(LED_WIFI,SS_LED_WHITE);delay(1000);
        simple_led_helper_set(LED_WIFI,SS_LED_HTTP_ERROR);delay(1000);
        simple_led_helper_set(LED_WIFI,SS_LED_WHITE);delay(1000);
        simple_led_helper_set(LED_WIFI,SS_LED_HTTP_ERROR);delay(1000);

        
        return false;
        
}

