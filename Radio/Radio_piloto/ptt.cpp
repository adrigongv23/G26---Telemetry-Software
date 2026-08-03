#include "ptt.hpp"
#include "config.hpp"

#include <Arduino.h>

static bool stable_pressed = false;
static bool last_raw_pressed = false;
static uint32_t last_raw_change_ms = 0;

bool ptt_init()
{
    // KEY2 no lleva resistencia pull-up externa, por lo que activamos
    // la pull-up interna. Al pulsar KEY2, GPIO13 queda conectado a GND.
    pinMode(PTT_BUTTON_GPIO, INPUT_PULLUP);
    delay(10);

    last_raw_pressed =
        (digitalRead(PTT_BUTTON_GPIO) == PTT_BUTTON_ACTIVE_LEVEL);
    stable_pressed = last_raw_pressed;
    last_raw_change_ms = millis();

#if PILOT_DEBUG_SERIAL
    Serial.println("PTT configurado en KEY2 (GPIO13, activo a nivel LOW)");
    Serial.println("DIP: 1=ON, 2=OFF y 4=OFF para usar KEY2 sin conflictos");
#endif

    return true;
}

bool ptt_is_pressed()
{
    const bool raw_pressed =
        (digitalRead(PTT_BUTTON_GPIO) == PTT_BUTTON_ACTIVE_LEVEL);

    // Reinicia el temporizador cada vez que cambia la lectura instantanea.
    if (raw_pressed != last_raw_pressed) {
        last_raw_pressed = raw_pressed;
        last_raw_change_ms = millis();
    }

    // Solo acepta el nuevo estado cuando se mantiene estable el tiempo
    // indicado. Esto elimina los rebotes mecanicos del pulsador.
    if (stable_pressed != last_raw_pressed &&
        (uint32_t)(millis() - last_raw_change_ms) >= PTT_DEBOUNCE_MS) {
        stable_pressed = last_raw_pressed;

#if PILOT_DEBUG_SERIAL
        Serial.println(stable_pressed
            ? "PTT KEY2 ACTIVADO"
            : "PTT KEY2 DESACTIVADO");
#endif
    }

#if PTT_BUTTON_DEBUG
    Serial.print("KEY2 raw=");
    Serial.print(raw_pressed ? "PULSADO" : "LIBRE");
    Serial.print(" estable=");
    Serial.println(stable_pressed ? "PULSADO" : "LIBRE");
#endif

    return stable_pressed;
}
