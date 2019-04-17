
/*
==================================================================
        wifi provisioning functions
==================================================================  */
void wifi_provisioning_list_networks();
void wifi_provisoining_mode_loop();


void wifi_provisoining_mode_loop()
{

// compare the previous status to the current status
        if (LAST_RF_CONNECTION_STATUS != WiFi.status())
        {
                // it has changed update the variable
                LAST_RF_CONNECTION_STATUS = WiFi.status();

                if (LAST_RF_CONNECTION_STATUS == WL_AP_CONNECTED)
                {
                        // a device has connected to the AP
                        Serial.print("Device connected to AP, MAC address: ");
                }
                else
                {
                        // a device has disconnected from the AP, and we are back in listening mode
                        Serial.println("Device disconnected from AP");
                }
        }
  
  
        WiFiClient client = server.available();   // listen for incoming clients

        if (client)
        {                             // if you get a client,
                Serial.println("new client");           // print a message out the serial port
                               // make a String to hold incoming data from the client
                String method = "";
                bool has_method = false;
                bool is_post = false;
                char display_mac[20] = {};
                sprintf(display_mac,"%02X:%02X:%02X:%02X:%02X:%02X",DEVICE_MAC[0],DEVICE_MAC[1],DEVICE_MAC[2],DEVICE_MAC[3],DEVICE_MAC[4],DEVICE_MAC[5]);
                while (client.connected())
                {       // loop while the client's connected
                        if (client.available())
                        {
                                // if there's bytes to read from the client,
                                currentLine = client.readStringUntil('\n');
                                Serial.println(currentLine);    
                                
                                if (!has_method && (currentLine.indexOf("GET") == 0 || currentLine.indexOf("POST") == 0) )
                                {
                                        has_method = true;
                                        method = currentLine;
                                }
                                
                                if (has_method && currentLine.indexOf('\r') == 0)
                                {
                                        Serial.print("\nParse Method: ");Serial.print(method);
                                        // parse request.
                                        if (method.indexOf("GET / HTTP") == 0)
                                        {
                                                Serial.print("\nis root");
                                                
                                                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                                                // and a content-type so the client knows what's coming, then a blank line:
                                                client.println("HTTP/1.1 200 OK");
                                                client.println("Content-type:text/html");
                                                client.println();
                                              
                                                // the content of the HTTP response follows the header:
                                                client.print("<!DOCTYPE html><html lang=en><head><meta charset=utf-8>");
                                                client.print("<meta http-equiv=X-UA-Compatible content=IE=edge>");
                                                client.print("<meta name=viewport content='width=device-width, initial-scale=1'>");
                                                client.print("<style type=text/css>");
                                                client.print("*{font-family:Arial;box-sizing:border-box;}body{padding:10px;}");
                                                client.print(".ac{text-align:center;vertical-align:middle;}");
                                                client.print("input{ font-size:20px;width:100%;margin:10px 0; }");
                                                client.print("label{font-weight:bold;display:block;}");
                                                client.print("button{font-size:20px;width:100%;text-align:left;margin:10px 0;}");
                                                client.print("</style><script>function hc(o){console.log(o.innerText);document.getElementById('ssid').value=o.innerText;}</script>");
                                                client.print("</head><body><div style='width:100%;max-width:350px;margin:auto;'>");
                                                client.print("<h4>Config WiFi for ");client.print(display_mac);client.print("</h4><form name=send method=POST>");
                                                client.print("<input type=text name=ssid id=ssid placeholder=SSID><input name=pass type=text id=pass placeholder=Password>");
                                                client.print("<button type=button onclick=send.submit() >Submit</button></form>");
                                                client.print("<table style='width:100%'><tr><th>SSID</th><th>RSSI</th><th>Secure</th></tr>");
                                                
                                                byte _list_count = list_ssid_count;
                                                while(_list_count-- > 0)
                                                {
                                                        client.print("<tr><td>");
                                                                client.print("<button onclick=hc(this)>");client.print(list_ssid[_list_count]);client.print("</button></td>");
                                                                client.print("<td class='ac'>");client.print(list_rssi[_list_count]);client.print("</td>");
                                                                client.print("<td class='ac'>");client.print((list_security[_list_count] >0? "Y":"N"));client.print("</td>");
                                                        client.print("</tr>");
                                                }
                                                
                                                client.print("</table></div></body></html>\r\n");
                                        }
                                        else if (method.indexOf("POST / HTTP") == 0)
                                        {
                                                bool has_content = false;
                                                while( !has_content || client.available())
                                                {
                                                        if (client.available())
                                                        {
                                                                currentLine = client.readStringUntil('\n');
                                                                
                                                                Serial.println(currentLine);
                                                                
                                                                if (currentLine.indexOf("pass") > 0)
                                                                {
                                                                        client.readStringUntil('\n');
                                                                        
                                                                        has_content = true;
                                                                        //ssid=goseeseqw&pass=Test
                                                                        
                                                                        sprintf(configs.ssid,"%s",currentLine.substring(currentLine.indexOf("=")+1, currentLine.indexOf("&")).c_str());
                                                                        sprintf(configs.pass,"%s",currentLine.substring(currentLine.indexOf("=",currentLine.indexOf("&"))+1).c_str());
                                                                        configs.valid = 1;
                                                                        configs.identified = 0xFF;
                                                                        
                                                                        flash.eraseSection(flash_config_address, sizeof(configs));
                                                                        flash.writeAnything(flash_config_address, configs);
                                                                        
                                                                        Serial.print("\nSaved SSID: ");Serial.print(configs.ssid);
                                                                        Serial.print("\nSaved PASS: ");Serial.print(configs.pass);
                                                                        
                                                                        flash.readAnything(flash_config_address, configs);
                                                                }
                                                        }
                                                }
                                                
                                                client.println("HTTP/1.1 200 OK");
                                                client.println("Content-type:text/html");
                                                client.println();
                                                
                                                client.print("<!DOCTYPE html><html lang=en><head><meta charset=utf-8>");
                                                client.print("<meta http-equiv=X-UA-Compatible content=IE=edge>");
                                                client.print("<meta name=viewport content='width=device-width, initial-scale=1'>");
                                                client.print("<style type=text/css>");
                                                client.print("*{font-family:Arial;box-sizing:border-box;}body{padding:10px;}");
                                                client.print(".ac{text-align:center;vertical-align:middle;}");
                                                client.print("</style>");
                                                client.print("</head><body><div style='width:100%;max-width:350px;margin:auto;'>");
                                                client.print("<h4>Config WiFi for ");client.print(display_mac);client.print("</h4>");
                                                client.print("connecting to:<br>SSID: <strong>");client.print(configs.ssid);client.print("</strong><br>Pass: <strong>");client.print(configs.pass);client.print("</strong></div></body></html>\r\n");
                                                
                                                client.stop();
                                                Serial.print("\nRebooting Now");
                                                delay(2000);
                                                NVIC_SystemReset();
                                        }
                                        else
                                        {
                                                client.println("HTTP/1.1 404 Not Found");
                                                client.println("Content-type:text/html");
                                                client.println();
                                        }
                                        
                                        has_method = false;
                                        method = "";
                                        
                                        break;
                                }
                                
                                else
                                {
                                        currentLine = "";
                                }
                        }
                        
                        
                        
                        
                                        
                }
                
                // close the connection:
                client.stop();
                Serial.println("client disconnected");
        }
}

void wifi_provisioning_list_networks()
{
        // scan for nearby networks:
        Serial.print("\n** Scan Networks **");
        int numSsid = WiFi.scanNetworks();
        if (numSsid == -1)
        {
                Serial.println("Couldn't get a WiFi connection");
                return;
        }
      
        // print the list of networks seen:
        Serial.print("\nnumber of available networks: ");
        Serial.print(numSsid);
      
        list_ssid_count = 0;
        // print the network number and name for each network found:
        for (int thisNet = 0; thisNet < numSsid; thisNet++)
        {
                byte bssid[6];
                WiFi.BSSID(thisNet, bssid);
                char bssid_c[20] = {}; 
                sprintf(bssid_c,"%02X:%02X:%02X:%02X:%02X:%02X",bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
                
                Serial.print("\n");
                Serial.print(bssid_c);
                Serial.print(",");
                Serial.print(WiFi.SSID(thisNet));
                Serial.print(",");
                Serial.print(WiFi.RSSI(thisNet));
                
                // capture ssid name
                // skip blank names. 
                if (sprintf(list_ssid[list_ssid_count],"%s",WiFi.SSID(thisNet)) < 3)
                {
                        continue;
                }
                
                list_rssi[list_ssid_count] = WiFi.RSSI(thisNet);
                list_security[list_ssid_count] = (WiFi.encryptionType(thisNet) == ENC_TYPE_NONE? 0:1);
                
                list_ssid_count++;
                if (list_ssid_count > 19)
                {
                        list_ssid_count = 20;
                        break;;
                }
        }

}