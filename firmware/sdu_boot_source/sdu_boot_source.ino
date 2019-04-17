
/*
==================================================================
        copy this to the sketch folder with
        at sdu_boot_source.ino.bin | xxd -i > ~/uC/Client\ Projects/WaterVault/firmware/wv-wifi/sdu_boot_source.h
==================================================================  */

#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <SPI.h>
#include <SPIMemory.h>
#include <FlashStorage.h>
#include <Adafruit_NeoPixel.h>

#define SDU_START    0x2000
#define SDU_SIZE     0x4000
#define SKETCH_START (uint32_t*)(SDU_START + SDU_SIZE)

#define pin_RGB                         11
#define pin_SEAL_IN                     A2
#define pin_FLASH_SS                    13

struct config_struct
{
        byte valid;          
        char ssid[40];          
        char pass[40];           
        int sleep_s = 10;            
        byte identified = 0xFF;
        byte has_update;
        long update_length;
};
config_struct configs;

uint32_t flash_firmware_address = 0; // firmware is max 256KB
uint32_t flash_config_address = (260*1024); // this is 260KB


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(2, pin_RGB, NEO_GRB + NEO_KHZ800);
SPIFlash external_flash(pin_FLASH_SS);
FlashClass flash;

// Initialize C library
extern "C" void __libc_init_array(void);

int main()
{
        init();
        
        __libc_init_array();
        
        delay(1);
        
        external_flash.begin();
        external_flash.readAnything(flash_config_address, configs);
        
        if (configs.has_update == 0x01 && configs.update_length > SDU_SIZE)
        {
                pixels.begin();
                
                pixels.setPixelColor(1, pixels.Color(0,0,0));
                pixels.setPixelColor(0, pixels.Color(0,0,0));
                pixels.show();
                
                long read_pointer = 0;
                uint32_t flashAddress = (uint32_t)SKETCH_START;
                
                // calcualte firmware size less sdu boot. 
                uint32_t update_size = configs.update_length - SDU_SIZE;
                
                // erase the current firmware. 
                flash.erase((void*)flashAddress, update_size);
                bool is_toggle = false;
                bool sdu_skip = false;
                byte buffer[512] = {};
                
                while(read_pointer < configs.update_length+sizeof(buffer))
                {
                        //b = external_flash.readByte(read_pointer + flash_firmware_address,false);
                        external_flash.readByteArray(read_pointer + flash_firmware_address, buffer, sizeof(buffer));
                        
                        if (read_pointer % 4096 == 0)
                        {
                                if (is_toggle)
                                {
                                        is_toggle = false;
                                        pixels.setPixelColor(0, pixels.Color(25,25,25));
                                }
                                else
                                {
                                        is_toggle = true;
                                        pixels.setPixelColor(0, pixels.Color(0,0,50));
                                }
                                pixels.show();
                                delay(100);
                        }
                        
                        read_pointer += sizeof(buffer);
                        
                        // skip SDU. 
                        if (read_pointer <= SDU_SIZE){ continue;}
                        
                        /*
                        if (!sdu_skip)
                        {
                                pixels.setPixelColor(1, pixels.Color(0,50,0));
                                pixels.show();
                                sdu_skip = true;
                        }
                        */
                        
                        flash.write((void*)flashAddress, buffer, sizeof(buffer));
                        flashAddress += sizeof(buffer);
                }
                
                configs.has_update = 0x00;
                configs.update_length = 0x00;
                configs.identified = 0x00;
                external_flash.eraseSection(flash_config_address, sizeof(configs));
                external_flash.writeAnything(flash_config_address, configs);
                
                pixels.setPixelColor(0, pixels.Color(0,50,0));
                pixels.show();
                
                NVIC_SystemReset();
        }
        
        // jump to the sketch
        __set_MSP(*SKETCH_START);
        
        //Reset vector table address
        SCB->VTOR = ((uint32_t)(SKETCH_START) & SCB_VTOR_TBLOFF_Msk);
        
        // address of Reset_Handler is written by the linker at the beginning of the .text section (see linker script)
        uint32_t resetHandlerAddress = (uint32_t) * (SKETCH_START + 1);
        // jump to reset handler
        asm("bx %0"::"r"(resetHandlerAddress));
}

