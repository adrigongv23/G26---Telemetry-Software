#include "led.hpp"
#include "configuracion.hpp"
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel tira(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

void led_startup() {

    tira.begin();
    tira.setBrightness(100);
    tira.clear();  
    tira.show();

}


void led_show_rpm(int rpmh, int rpml) {

    int rpm=(rpmh * 256) + rpml;

    tira.clear();

    // Por debajo de las RPM mínimas, todos apagados
    if (rpm < RPM_MIN_LED) {
        tira.show();
        return;
    }

    // Por encima del máximo, todos azules
    if (rpm > RPM_MAX_LED) {
        tira.fill(tira.Color(0, 0, 255));
        tira.show();
        return;
    }

    // Calcula cuántos LED deben encenderse
    int leds_encendidos = map(rpm, RPM_MIN_LED, RPM_MAX_LED, 1, NUM_LEDS);
    leds_encendidos = constrain(leds_encendidos, 0, NUM_LEDS);

    int primer_tercio = NUM_LEDS / 3;
    int segundo_tercio = (NUM_LEDS * 2) / 3;

    for (int i = 0; i < leds_encendidos; i++) {

        if (i < primer_tercio) {
            // Primer tercio: verde
            tira.setPixelColor(i, tira.Color(0, 255, 0));

        } else if (i < segundo_tercio) {
            // Segundo tercio: amarillo
            tira.setPixelColor(i, tira.Color(255, 255, 0));

        } else {
            // Tercer tercio: rojo
            tira.setPixelColor(i, tira.Color(255, 0, 0));
        }
    }

    tira.show();
}