#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <Adafruit_NeoPixel.h>

#define PIN_WS2812B 6
#define NUM_PIXELS 18 

#define STOP_CAR_WARNING 1

#define RPM_MIN 6000
#define RPM_MAX 11000

class LedStrip{
public:
    LedStrip(): _ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800), _warning(0), _brightness(255)  {}

    static void updateTask(void *arg) {
        LedStrip *ledStrip = static_cast<LedStrip*>(arg);
        ledStrip->update();
        vTaskDelete(NULL);
    }

    void update();

    void set_rpm(int rpm);

    void begin(){
        _ws2812b.begin();
    }   

    void set_brightness(uint8_t brightness) {
        _ws2812b.setBrightness(brightness);
        _ws2812b.show();
    }

    void set_mutex(SemaphoreHandle_t mutex){
        _mutex = mutex;
    }

    void display_warning(int warning);
    void display_rpm(int rpm);
    void display_startup();

private:
    Adafruit_NeoPixel _ws2812b;
    SemaphoreHandle_t _mutex;
    int _rpm;
    int _warning;
    int _brightness;
};

#endif