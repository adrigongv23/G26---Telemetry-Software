#include <Arduino.h>
#include <esp_now.h>

#include "acceleration.hpp"
#include "servicios_pruebas.hpp"

static unsigned long startTime = 0;
static bool counting = false;
static volatile bool handshakeDone = false;
static volatile bool handshakePending = false;
static volatile bool triggerPending = false;
static unsigned long lastHandshake = 0;

static void enviar_handshake() {
  paquete_final_acceleration mensaje = {};
  mensaje.handshake = true;
  mensaje.trigger = false;

  esp_err_t resultado = esp_now_send(ESPNOW_MAC_FINAL_ACCELERATION, (uint8_t*)&mensaje, sizeof(mensaje));
  if (resultado != ESP_OK) Serial.println("Error enviando handshake al sensor final de Acceleration");
}

void reiniciar_acceleration() {
  startTime = 0;
  counting = false;
  handshakeDone = false;
  handshakePending = false;
  triggerPending = false;
  lastHandshake = 0;
}

void recibir_paquete_acceleration(const paquete_final_acceleration& mensaje) {
  if (mensaje.handshake && !handshakeDone) {
    handshakeDone = true;
    handshakePending = true;
  }

  if (mensaje.trigger) triggerPending = true;
}

void prueba_acceleration() {
  if (!handshakeDone && millis() - lastHandshake >= 500) {
    lastHandshake = millis();
    enviar_handshake();
    Serial.println("Enviando handshake al sensor final de Acceleration...");
  }

  if (handshakePending) {
    handshakePending = false;
    Serial.println("Sensor final de Acceleration conectado");
    enviar_datos(1);
  }

  if (triggerPending) {
    triggerPending = false;

    if (counting) {
      unsigned long tiempo = millis() - startTime;
      counting = false;

      Serial.print("Acceleration terminada: ");
      Serial.print(tiempo / 1000.0, 3);
      Serial.println(" s");

      enviar_datos(3, 0, tiempo);
    }
  }

  if (!detectar_activacion_sensor(SENSOR_ACCELERATION, 1500)) return;

  if (!handshakeDone) {
    Serial.println("No se inicia: el sensor final de Acceleration no esta conectado");
    enviar_datos(4);
    return;
  }

  if (!counting) {
    startTime = millis();
    counting = true;
    Serial.println("Sensor de salida activado: Acceleration iniciada");
    enviar_datos(2);
  }
}
