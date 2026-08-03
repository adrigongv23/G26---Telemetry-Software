#include <Arduino.h>
#include "bridge.hpp"
#include "config.hpp"

void setup()
{
    Serial.begin(115200);
    delay(700);

    Serial.println();
    Serial.println("============================================");
    Serial.println(" WROOM-32U intermediario UART / ESP-NOW LR");
    Serial.println(" DEBUG PERMANENTE ACTIVADO");
    Serial.println("============================================");

    if (!bridge_init()) {
        Serial.println("FALLO iniciando el intermediario");

        while (true) {
            Serial.println("El intermediario sigue detenido por un error de inicio.");
            delay(2000);
        }
    }
}

void loop()
{
    bridge_process();
}
