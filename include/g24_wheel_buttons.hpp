#ifndef G24WHEELBUTTONS_HPP
#define G24WHEELBUTTONS_HPP

#include "common/common_libraries.hpp"
#include "led_strip.hpp"
#include "data_processor.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "can.hpp"

#define B1_PIN GPIO_NUM_2
#define B2_PIN GPIO_NUM_4
#define B3_PIN GPIO_NUM_42
#define B4_PIN GPIO_NUM_40

#define B1_LED_PIN GPIO_NUM_3
#define B2_LED_PIN GPIO_NUM_5
#define B3_LED_PIN GPIO_NUM_41
#define B4_LED_PIN GPIO_NUM_39

#define LEVA_IZQ_PIN GPIO_NUM_15 
#define LEVA_DER_PIN GPIO_NUM_16

#define E1_PIN_A GPIO_NUM_11
#define E1_PIN_B GPIO_NUM_10
#define E2_PIN_A GPIO_NUM_36
#define E2_PIN_B GPIO_NUM_35

#define E1_BUTTON_PIN GPIO_NUM_12
#define E2_BUTTON_PIN GPIO_NUM_34

class G24WheelButtons {
public:
    G24WheelButtons();
    void begin();
    void update();
    static void updateTask(void *arg);
    void set_can_controller(CAN *canController);
    void set_led_strip(LedStrip *ledStrip);
    void set_data_processor(DataProcessor *dataProcessor);

private:
    LedStrip *_led_strip;
    DataProcessor *_data_processor;
    void handleButtonPress(gpio_num_t buttonPin);
    void handleButtonRelease(gpio_num_t buttonPin);
    static void IRAM_ATTR handleEncoderInterrupt(void* arg);
    void handleClockWise(gpio_num_t encoderPin);
    void handleCounterClockWise(gpio_num_t encoderPin);
    void checkButtonState(gpio_num_t buttonPin, volatile bool &buttonState, volatile unsigned long &lastPressTime, int ledPin);

    static const unsigned long debounceTime = 50; // milliseconds
    volatile unsigned long lastPressTimeB1;
    volatile unsigned long lastPressTimeB2;
    volatile unsigned long lastPressTimeB3;
    volatile unsigned long lastPressTimeB4;
    volatile unsigned long lastPressTimeLevaIzq;
    volatile unsigned long lastPressTimeLevaDer;
    volatile unsigned long lastPressTimeE1;
    volatile unsigned long lastPressTimeE2;
    volatile unsigned long lastTurnTimeE1;
    volatile unsigned long lastTurnTimeE2;

    volatile bool buttonStateB1;
    volatile bool buttonStateB2;
    volatile bool buttonStateB3;
    volatile bool buttonStateB4;
    volatile bool buttonStateLevaIzq;
    volatile bool buttonStateLevaDer;
    volatile int encoderCounterE1;
    volatile int encoderCounterE2;
    volatile bool buttonStateE1;
    volatile bool buttonStateE2;

    volatile int displayCounter;
    volatile int lastDispayCounter;

    volatile int brightnessCounter;
    volatile int lastBrightnessCounter;

    volatile int lastPin_A_StateE1;
    volatile int lastPin_A_StateE2;

    CAN* canController;
};

#endif