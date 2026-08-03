#ifndef G24WHEELBUTTONS_HPP
#define G24WHEELBUTTONS_HPP

#include "common/common_libraries.hpp"
#include "can.hpp"
#include "led_strip.hpp"
#include "data_processor.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "common/crowpanel_pins.h"

#include "can.hpp"

#define B1_PIN GPIO_NUM_2
#define B2_PIN GPIO_NUM_4
#define B3_PIN GPIO_NUM_42
#define B4_PIN GPIO_NUM_40

/*
#define B1_PIN GPIO_AVAILABLE_2
#define B2_PIN GPIO_AVAILABLE_3
#define B3_PIN GPIO_AVAILABLE_4
#define B4_PIN GPIO_AVAILABLE_5
*/
#define B1_LED_PIN GPIO_NUM_3
#define B2_LED_PIN GPIO_NUM_5
#define B3_LED_PIN GPIO_NUM_41
#define B4_LED_PIN GPIO_NUM_39

#define LEVA_IZQ_PIN GPIO_NUM_15 
#define LEVA_DER_PIN GPIO_NUM_16
/*
#define LEVA_IZQ_PIN GPIO_AVAILABLE_10
#define LEVA_DER_PIN GPIO_AVAILABLE_11
*/
#define E1_PIN_A GPIO_NUM_11
#define E1_PIN_B GPIO_NUM_10
#define E2_PIN_A GPIO_NUM_36
#define E2_PIN_B GPIO_NUM_35

#define E1_BUTTON_PIN GPIO_NUM_12
#define E2_BUTTON_PIN GPIO_NUM_34

#define PRESS_TIME 30

class G24WheelButtons {
public:
    void ejecucion();
    void begin();
    static void botonera(void *arg);
    static void updateTask(void *arg);
    void set_can(CAN *can){
        _can = can;
    }

private:
    CAN *_can=nullptr;
    void estado_boton(gpio_num_t buttonPin, volatile bool &buttonState, volatile bool &laststate, volatile unsigned long &lastPressTime, volatile unsigned long &temporizador);
    void check_boton();

    static const unsigned long debounceTime = 50; // milliseconds

    struct boton{
        volatile bool estado=false;
        volatile bool last_estado=false;
        volatile unsigned long lastPressTime=0;
        volatile unsigned long temporizador=0;
    };

    boton B1;
    boton B2;
    boton B3;
    boton B4;
    boton levaizq;
    boton levader;

    bool estado_tarea=false;
    TaskHandle_t manejador_tarea=nullptr;
};

#endif