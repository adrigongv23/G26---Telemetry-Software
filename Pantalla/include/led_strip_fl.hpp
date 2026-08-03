// #ifndef LED_STRIP_HPP
// #define LED_STRIP_HPP

// #include <FastLED.h>

// #define PIN_WS2812B 6
// #define NUM_PIXELS 18 

// #define STOP_CAR_WARNING 1

// #define RPM_MIN 6000
// #define RPM_MAX 11000

// class LedStrip{
// public:
//     LedStrip(): _warning(0), _brightness(255)  {}

//     static void updateTask(void *arg) {
//         LedStrip *ledStrip = static_cast<LedStrip*>(arg);
//         ledStrip->update();
//         vTaskDelete(NULL);
//     }

//     void update();

//     void set_rpm(int rpm);

//     void begin(){
//         FastLED.addLeds<WS2812B, PIN_WS2812B, GRB>(_leds, NUM_PIXELS);
//         FastLED.setBrightness(_brightness);
//     }   

//     void set_brightness(uint8_t brightness) {
//         _brightness = brightness;
//         FastLED.setBrightness(_brightness);
//         FastLED.show();
//     }

//     void set_mutex(SemaphoreHandle_t mutex){
//         _mutex = mutex;
//     }

//     void display_warning(int warning);
//     void display_rpm(int rpm);
//     void display_startup();

// private:
//     CRGB _leds[NUM_PIXELS];
//     SemaphoreHandle_t _mutex;
//     int _rpm;
//     int _warning;
//     int _brightness;
// };

// #endif
