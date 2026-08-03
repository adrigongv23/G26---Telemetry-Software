#include <Arduino.h>

#include "skidpad.hpp"

static int ciclo = 0;

void reiniciar_receptor_skidpad() {
  ciclo = 0;
}

void procesar_skidpad(const paquete_datos& datos) {
  switch (datos.estado) {
    case 1:
      ciclo = datos.vuelta;
      Serial.print("\n===== Ciclo ");
      Serial.print(ciclo);
      Serial.println(" =====");
      Serial.println("Primera activacion: nada");
      break;

    case 2:
      Serial.println("Segunda activacion: inicia vuelta DERECHA");
      break;

    case 3:
      Serial.print("Vuelta DERECHA terminada: ");
      Serial.print(datos.laptime_derecha / 1000.0, 3);
      Serial.println(" s");
      break;

    case 4:
      Serial.println("Cuarta activacion: inicia vuelta IZQUIERDA");
      break;

    case 5:
      Serial.print("Vuelta IZQUIERDA terminada: ");
      Serial.print(datos.laptime_izquierda / 1000.0, 3);
      Serial.println(" s");
      Serial.print("Media de ambas vueltas: ");
      Serial.print(datos.average / 1000.0, 3);
      Serial.println(" s");
      break;

    default:
      Serial.println("Estado de Skidpad desconocido");
      break;
  }
}
