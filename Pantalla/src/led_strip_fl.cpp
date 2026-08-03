// #include "../include/led_strip.hpp"

// void LedStrip::display_warning(int warning){
//     if(warning == STOP_CAR_WARNING){
//         for(int i = 0; i < NUM_PIXELS; i++){
//             _leds[i] = CRGB::Red;
//         }
//         FastLED.show();
//     }
// }

// void LedStrip::set_rpm(int rpm){
//     if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
//         display_rpm(rpm);
//         xSemaphoreGive(_mutex);
//     }
// }

// void LedStrip::display_rpm(int rpm){
//     if (rpm > RPM_MAX) {
//         for (int i = 0; i < NUM_PIXELS; i++) {
//             _leds[i] = CRGB::Red;
//         }
//         FastLED.show();
//         delay(50);
//         for (int i = 0; i < NUM_PIXELS; i++) {
//             _leds[i] = CRGB::Black;
//         }
//         FastLED.show();
//         delay(50);
//     } else {
//         int numLeds = map(rpm, RPM_MIN, RPM_MAX, 0, NUM_PIXELS);

//         for (int i = 0; i < NUM_PIXELS; i++) {
//             if (i < numLeds) {
//                 if (i < NUM_PIXELS / 3) {
//                     _leds[i] = CRGB::Green;
//                 } else if (i < 2 * NUM_PIXELS / 3) {
//                     _leds[i] = CRGB::Yellow;
//                 } else {
//                     _leds[i] = CRGB::Red;
//                 }
//             } else {
//                 _leds[i] = CRGB::Black;
//             }
//         }
//         FastLED.show();
//     }
// }

// void LedStrip::display_startup(){
//     FastLED.setBrightness(255);
//     CRGB colors[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };

//     for (int pass = 0; pass < 3; pass++) {
//         for (int i = 0; i <= NUM_PIXELS - 4; i++) {
//             fill_solid(_leds, NUM_PIXELS, CRGB::Black);
//             FastLED.show();
//             for (int j = 0; j < 4; j++) {
//                 _leds[i + j] = colors[pass];
//             }
//             FastLED.show();
//             delay(10);
//         }

//         for (int i = NUM_PIXELS - 4; i >= 0; i--) {
//             fill_solid(_leds, NUM_PIXELS, CRGB::Black);
//             FastLED.show();
//             for (int j = 0; j < 4; j++) {
//                 _leds[i + j] = colors[pass];
//             }
//             FastLED.show();
//             delay(10); 
//         }
//     }
//     fill_solid(_leds, NUM_PIXELS, CRGB::Black);
//     FastLED.show();
// }

// void LedStrip::update(){
//     display_rpm(_rpm);
// }
