/**
 * @file led_strip.cpp
 * @author Raúl Arcos Herrera
 * @brief This file contains the implementation of the LED Strip class for Link G4+ ECU.
 */ 

#include "../include/led_strip.hpp"

void LedStrip::display_rpm(int rpm){
    if(!launch){
        if (rpm >= RPM_MAX) {
            uint32_t ahora=millis();

            if(ahora-last_parpadeo>T_PARPADEO){
                last_parpadeo=ahora;
                estado_parpadeo=!estado_parpadeo;
            }

            if(estado_parpadeo){
                for (int i = 0; i < NUM_PIXELS; i++) {
                _ws2812b.setPixelColor(i, _ws2812b.Color(255, 0, 0));
                }
                _ws2812b.show();
            } else {
                _ws2812b.clear();
                _ws2812b.show();
            }

        } else {
            int numLeds = map(rpm, RPM_MIN, RPM_MAX, 0, NUM_PIXELS);
            int sector1=NUM_PIXELS/3;
            int sector2=2*sector1;

            for (int i = 0; i < NUM_PIXELS; i++) {
                if (i < numLeds) {
                    if (i <sector1) {
                        _ws2812b.setPixelColor(i, _ws2812b.Color(0, 255, 0));
                    } else if (i < sector2) {
                        _ws2812b.setPixelColor(i, _ws2812b.Color(255, 255, 0));
                    } else {
                        _ws2812b.setPixelColor(i, _ws2812b.Color(255, 0, 0));
                    }
                } else {
                    _ws2812b.setPixelColor(i, 0);
                }
            }
            _ws2812b.show();
        }
    }
}

void LedStrip::display_startup(){
    _ws2812b.setBrightness(255);
    uint32_t colors[3] = { _ws2812b.Color(255, 0, 0), _ws2812b.Color(0, 255, 0), _ws2812b.Color(0, 0, 255) };

    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i <= NUM_PIXELS - 4; i++) {
            _ws2812b.clear();
            _ws2812b.show();
            for (int j = 0; j < 4; j++) {
                _ws2812b.setPixelColor(i + j, colors[pass]);
            }
            _ws2812b.show();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        for (int i = NUM_PIXELS - 4; i >= 0; i--) {
            _ws2812b.clear();
            _ws2812b.show();
            for (int j = 0; j < 4; j++) {
                _ws2812b.setPixelColor(i + j, colors[pass]);
            }
            _ws2812b.show();
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }
    }
    _ws2812b.clear();
    _ws2812b.show();
}

void LedStrip::launch_control(bool estado){

    if (launch==estado) return;

    launch=estado;

    if(launch){
        for(int i=0;i<NUM_PIXELS;i++){
            _ws2812b.setPixelColor(i, _ws2812b.Color(255, 255, 0));
        }

        _ws2812b.show();
    } else {
        _ws2812b.clear();
        _ws2812b.show();
    }


}