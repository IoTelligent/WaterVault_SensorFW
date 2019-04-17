/*
==================================================================
        Section Name
==================================================================  */
int internal_vbat();
int internal_temp();
void internal_temp_init();

/*
==================================================================
        read_internal
==================================================================  */
int internal_vbat()
{
        char _vbat[10] = {}; 
        sprintf(_vbat,"%0.2f", (float(analogRead(pin_Vbat_Sense))/4095.0)*16.5*100);
        //Serial.print("\n_vbat: ");Serial.print(_vbat);Serial.print(" ");Serial.print(analogRead(pin_Vbat_Sense));
        return atoi(_vbat);
}

int internal_temp()
{
        int16_t data = 0;
        char _temp[10] = {}; 
        Wire.requestFrom(LM75B_address,2);
        if(Wire.available())
        {
                data = (Wire.read() << 8) | Wire.read();
        }

        // read temp, and leave in C. 
        sprintf(_temp,"%0.2f",((data>>5)*0.125)*100);
        //Serial.print("\n_temp: ");Serial.print(_temp); 
        return atoi(_temp);
}

/*
==================================================================
        internal_temp_init
==================================================================  */
void internal_temp_init()
{
        Wire.beginTransmission(LM75B_address);
        Wire.write(0x00);
        Wire.endTransmission();
}