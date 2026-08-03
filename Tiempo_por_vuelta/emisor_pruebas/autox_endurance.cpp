#include <Arduino.h>

#include "autox_endurance.hpp"
#include "configuracion.hpp"
#include "servicios_pruebas.hpp"

static unsigned long startTime = 0;
static int activationCount = 0;
static int lapCount = 0;
static const int WARMUP_TOTAL = 2;

void reiniciar_autox_endurance() {
  startTime = 0;
  activationCount = 0;
  lapCount = 0;
}

void prueba_autox_endurance() {
  if (!detectar_activacion_sensor(SENSOR_AUTOX_ENDURANCE, 2000)) return;

  unsigned long now = millis();
  activationCount++;

  if (activationCount <= WARMUP_TOTAL) {
    if (activationCount == 1) {
      Serial.println("Calentamiento: vuelta 1 iniciada");
    } else {
      Serial.print("Calentamiento: vuelta ");
      Serial.print(activationCount - 1);
      Serial.print(" terminada, vuelta ");
      Serial.print(activationCount);
      Serial.println(" iniciada");
    }

    enviar_datos(1, activationCount);
    return;
  }

  if (activationCount == WARMUP_TOTAL + 1) {
    startTime = now;
    lapCount = 0;
    Serial.println("Calentamiento terminado: primera vuelta oficial iniciada");
    enviar_datos(2, 1);
    return;
  }

  unsigned long lapTime = now - startTime;
  lapCount++;
  startTime = now;

  Serial.print("Vuelta ");
  Serial.print(lapCount);
  Serial.print(": ");
  Serial.print(lapTime / 1000.0, 3);
  Serial.println(" s");

  enviar_datos(3, lapCount, lapTime);
}
