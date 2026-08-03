#include <Arduino.h>
#include "espnow.hpp"

void setup()
{
    // UART USB con el PC.
    // IMPORTANTE: tiene que coincidir con el baudrate del programa de Windows.
    // Buffer grande para que no se pierdan bytes cuando el PC envia audio por UART.
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    delay(1000);

#if DEBUG_SERIAL
    Serial.println();
    Serial.println("================================");
    Serial.println(" Puente UART <-> ESP-NOW LR");
    Serial.println("================================");
#endif

    if (!init_espnow_puente_bidir()) {
#if DEBUG_SERIAL
        Serial.println("FALLO: no se pudo iniciar ESP-NOW bidireccional");
#endif
        while (true) {
            delay(1000);
        }
    }

#if DEBUG_SERIAL
    Serial.println("Puente listo: PC <-> UART <-> ESP-NOW LR");
#endif
}

void loop()
{
    static bool pilot_first = true;

    // Cada funcion procesa un numero limitado de paquetes. Ademas se alterna
    // el orden para que ninguno de los dos sentidos monopolice el loop.
    if (pilot_first) {
        procesar_espnow_y_enviar_pc();
        procesar_uart_y_enviar_espnow();
    } else {
        procesar_uart_y_enviar_espnow();
        procesar_espnow_y_enviar_pc();
    }

    pilot_first = !pilot_first;
    delay(0);
}
