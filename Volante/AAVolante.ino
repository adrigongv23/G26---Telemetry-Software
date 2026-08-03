#include "can.hpp"
#include "led.hpp"
#include "configuracion.hpp"

CAN can;

void setup(){

    Serial.begin(115200);

    led_startup();

    can.start();
    can.start_listening_task();

}

void loop(){

    

}