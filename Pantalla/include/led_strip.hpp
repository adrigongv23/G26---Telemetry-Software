#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <Adafruit_NeoPixel.h>
#include "common/crowpanel_pins.h"

#define PIN_WS2812B 6
//#define PIN_WS2812B GPIO_AVAILABLE_1
#define NUM_PIXELS 18 
#define T_PARPADEO 50

#define STOP_CAR_WARNING 1

#define RPM_MIN 8000
#define RPM_MAX 12500

class LedStrip{
public:
    LedStrip(): _ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800), _warning(0), _brightness(255)  {}

    void begin(){
        _ws2812b.begin();
    }   

    void display_rpm(int rpm);
    void display_startup();
    void launch_control(bool estado);

private:
    Adafruit_NeoPixel _ws2812b;
    SemaphoreHandle_t _mutex;
    int _rpm;
    int _warning;
    int _brightness;
    bool launch=false;
    uint32_t last_parpadeo=0;
    bool estado_parpadeo=false;
};

#endif