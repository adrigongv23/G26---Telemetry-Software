#include <Arduino.h>

#include "autox_endurance.hpp"

unsigned long tiempo_acumulado=0;
int vueltas_total=0;

void reiniciar_receptor_autox_endurance() {

  if(tiempo_acumulado!=0 || vueltas_total!=0){

    Serial.print("Tiempo total: ");
    Serial.print(tiempo_acumulado/1000.0, 3);
    Serial.println(" s");

    unsigned long media_vueltas=tiempo_acumulado/vueltas_total;

    Serial.print("Media de vueltas: ");
    Serial.print(media_vueltas/1000.0, 3);
    Serial.println(" s");

    tiempo_acumulado=0;
    vueltas_total=0;

  }

}

void procesar_autox_endurance(const paquete_datos& datos) {
  switch (datos.estado) {
    case 1:
      if (datos.vuelta == 1) {
        Serial.println("Calentamiento: vuelta 1 iniciada");
      } else {
        Serial.print("Calentamiento: vuelta ");
        Serial.print(datos.vuelta - 1);
        Serial.print(" terminada, vuelta ");
        Serial.print(datos.vuelta);
        Serial.println(" iniciada");
      }
      break;

    case 2:
      Serial.println("Calentamiento terminado: primera vuelta oficial iniciada");
      break;

    case 3:
      Serial.print("Vuelta ");
      Serial.print(datos.vuelta);
      Serial.print(": ");
      Serial.print(datos.tiempo / 1000.0, 3);
      tiempo_acumulado+=datos.tiempo;
      vueltas_total=datos.vuelta;
      Serial.println(" s");
      break;

    default:
      Serial.println("Estado de Autocross / Endurance desconocido");
      break;
  }
}
