template <class T> bool eeprom_write_raw(const T& data);
template <class T> bool eeprom_write_raw(byte addr,T& data);

template <class T> bool eeprom_read_raw(T& data);
template <class T> bool eeprom_read_raw(byte addr, const T& data);

template <class T> bool eeprom_write_raw(const T& data)
{
        return eeprom_write_raw(0,data);
}
template <class T> bool eeprom_write_raw(byte addr, const T& data)
{
        if ((addr + sizeof(data)) > (addr + EEPROM.length()))
        {
                Serial.print("\n**EEPROM OUT OF RANGE**\n");
                return false;
        }

        const uint8_t* p = ((const uint8_t*)(const void*)&data);
        
        int ptr = 0;
        //Serial.print("\nEEPROM WRITE:");
        while(ptr < sizeof(data))
        {
                //Serial.print("\nwrite prt: ");
                //Serial.print(ptr);
                //Serial.print(" = ");
                //Serial.print("0x");
                //Serial.print((*p < 16? "0":""));
                //Serial.print(*p,HEX);
                EEPROM.write(addr++,*p++);
                ptr++;   
        }

        EEPROM.commit();
        
        return true;
}

template <class T> bool eeprom_read_raw(T& data)
{
        return  eeprom_read_raw(0,data);
}
template <class T> bool eeprom_read_raw(byte addr,T& data)
{
        if ((addr + sizeof(data)) > (addr + EEPROM.length()))
        {
                Serial.print("\n**EEPROM OUT OF RANGE**\n");
                return false;
        }

        uint8_t* p = (uint8_t*)(void*)&data;
        
        int ptr = 0;
        //Serial.print("\nEEPROM READ:");
        while(ptr < sizeof(data))
        {
                uint8_t _byte = EEPROM.read(addr++);
                
                //Serial.print("\nread prt: ");
                //Serial.print(ptr);
                //Serial.print(" = ");
                //Serial.print("0x");
                //Serial.print((_byte < 16? "0":""));
                //Serial.print(_byte,HEX);
                *p++ = _byte;
                ptr++;   
        }
        
        return true;
}