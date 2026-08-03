#include <Arduino.h>

#include "acceleration.hpp"

void reiniciar_receptor_acceleration() {
  // Acceleration no necesita variables persistentes en el receptor.
}

void procesar_acceleration(const paquete_datos& datos) {
  switch (datos.estado) {
    case 1:
      Serial.println("Sensor final de Acceleration conectado. Sistema listo");
      break;

    case 2:
      Serial.println("Acceleration iniciada");
      break;

    case 3:
      Serial.print("Acceleration terminada: ");
      Serial.print(datos.tiempo / 1000.0, 3);
      Serial.println(" s");
      break;

    case 4:
      Serial.println("ERROR: el sensor final de Acceleration no esta conectado");
      break;

    default:
      Serial.println("Estado de Acceleration desconocido");
      break;
  }
}
