#include <Arduino.h>

#include "configuracion.hpp"
#include "servicios_pruebas.hpp"
#include "skidpad.hpp"

static unsigned long startTime = 0;
static unsigned long lapTimeRight = 0;
static unsigned long lapTimeLeft = 0;
static int activationCount = 0;
static int cycleCount = 0;

void reiniciar_skidpad() {
  startTime = 0;
  lapTimeRight = 0;
  lapTimeLeft = 0;
  activationCount = 0;
  cycleCount = 0;
}

void prueba_skidpad() {
  if (!detectar_activacion_sensor(SENSOR_SKIDPAD, 1500)) return;

  unsigned long now = millis();
  activationCount++;

  switch (activationCount) {
    case 1:
      cycleCount++;
      Serial.print("\n===== Ciclo ");
      Serial.print(cycleCount);
      Serial.println(" =====");
      Serial.println("Primera activacion: nada");
      enviar_datos(1, cycleCount);
      break;

    case 2:
      startTime = now;
      Serial.println("Segunda activacion: inicia vuelta DERECHA");
      enviar_datos(2, cycleCount);
      break;

    case 3:
      lapTimeRight = now - startTime;
      Serial.print("Vuelta DERECHA terminada: ");
      Serial.print(lapTimeRight / 1000.0, 3);
      Serial.println(" s");
      enviar_datos(3, cycleCount, 0, lapTimeRight);
      break;

    case 4:
      startTime = now;
      Serial.println("Cuarta activacion: inicia vuelta IZQUIERDA");
      enviar_datos(4, cycleCount, 0, lapTimeRight);
      break;

    case 5: {
      lapTimeLeft = now - startTime;
      unsigned long media = (lapTimeRight + lapTimeLeft) / 2;

      Serial.print("Vuelta IZQUIERDA terminada: ");
      Serial.print(lapTimeLeft / 1000.0, 3);
      Serial.println(" s");
      Serial.print("Media de ambas vueltas: ");
      Serial.print(media / 1000.0, 3);
      Serial.println(" s");

      enviar_datos(5, cycleCount, 0, lapTimeRight, lapTimeLeft, media);
      activationCount = 0;
      break;
    }

    default:
      activationCount = 0;
      break;
  }
}
