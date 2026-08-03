#include <WiFi.h>
#include <esp_now.h>

#include "configuracion.hpp"
#include "servicios_pruebas.hpp"
#include "acceleration.hpp"
#include "skidpad.hpp"
#include "autox_endurance.hpp"

paquete_datos datos;

tipo_prueba prueba_activa = PRUEBA_NINGUNA;
volatile tipo_prueba prueba_pendiente = PRUEBA_NINGUNA;
volatile bool cambio_prueba_pendiente = false;
volatile bool reinicio_pendiente = false;

static bool sensorState = HIGH;
static bool lastSensorState = HIGH;
static unsigned long lastActivationTime = 0;
static const unsigned long DEBOUNCE_DELAY = 50;

void enviar_datos(uint8_t estado, uint16_t vuelta, unsigned long tiempo, unsigned long derecha, unsigned long izquierda, unsigned long media) {
  datos.prueba = prueba_activa;
  datos.estado = estado;
  datos.vuelta = vuelta;
  datos.tiempo = tiempo;
  datos.laptime_derecha = derecha;
  datos.laptime_izquierda = izquierda;
  datos.average = media;

  esp_err_t resultado = esp_now_send(ESPNOW_MAC_RECEPTOR_BOX, (uint8_t*)&datos, sizeof(datos));
  if (resultado != ESP_OK) Serial.println("Error enviando datos al receptor del box");
}

static int pin_sensor_prueba(tipo_prueba prueba) {
  switch (prueba) {
    case PRUEBA_ACCELERATION: return SENSOR_ACCELERATION;
    case PRUEBA_SKIDPAD: return SENSOR_SKIDPAD;
    case PRUEBA_AUTOX_ENDURANCE: return SENSOR_AUTOX_ENDURANCE;
    default: return SENSOR_SKIDPAD;
  }
}

bool detectar_activacion_sensor(int sensorPin, unsigned long minInterval) {
  sensorState = digitalRead(sensorPin);
  bool activado = false;

  if (lastSensorState == HIGH && sensorState == LOW) {
    unsigned long now = millis();

    if (now - lastActivationTime >= minInterval) {
      lastActivationTime = now;
      activado = true;
    }

    delay(DEBOUNCE_DELAY);
  }

  lastSensorState = sensorState;
  return activado;
}

static void reiniciar_variables_comunes() {
  lastActivationTime = 0;
  sensorState = digitalRead(pin_sensor_prueba(prueba_activa));
  lastSensorState = sensorState;
}

static void reiniciar_todas_las_pruebas() {
  reiniciar_acceleration();
  reiniciar_skidpad();
  reiniciar_autox_endurance();
}

static void reiniciar_prueba_actual() {
  switch (prueba_activa) {
    case PRUEBA_ACCELERATION: reiniciar_acceleration(); break;
    case PRUEBA_SKIDPAD: reiniciar_skidpad(); break;
    case PRUEBA_AUTOX_ENDURANCE: reiniciar_autox_endurance(); break;
    default: break;
  }

  memset(&datos, 0, sizeof(datos));
  reiniciar_variables_comunes();

  Serial.println("Prueba reiniciada desde el receptor del box");
  enviar_datos(101);
}

static void seleccionar_prueba(tipo_prueba nueva_prueba) {
  prueba_activa = nueva_prueba;

  reiniciar_todas_las_pruebas();
  memset(&datos, 0, sizeof(datos));
  reiniciar_variables_comunes();

  Serial.println();

  switch (prueba_activa) {
    case PRUEBA_ACCELERATION: Serial.println("PRUEBA SELECCIONADA: ACCELERATION"); break;
    case PRUEBA_SKIDPAD: Serial.println("PRUEBA SELECCIONADA: SKIDPAD"); break;
    case PRUEBA_AUTOX_ENDURANCE: Serial.println("PRUEBA SELECCIONADA: AUTOX / ENDURANCE"); break;
    default: Serial.println("NINGUNA PRUEBA SELECCIONADA"); break;
  }

  enviar_datos(100);
}

static void recibir_espnow(const esp_now_recv_info_t *info, const uint8_t *datos_recibidos, int longitud) {
  if (memcmp(info->src_addr, ESPNOW_MAC_RECEPTOR_BOX, 6) == 0 && longitud == sizeof(paquete_control)) {
    paquete_control control;
    memcpy(&control, datos_recibidos, sizeof(control));

    if (control.comando == COMANDO_SELECCIONAR_PRUEBA && control.prueba >= PRUEBA_ACCELERATION && control.prueba <= PRUEBA_AUTOX_ENDURANCE) {
      prueba_pendiente = (tipo_prueba)control.prueba;
      cambio_prueba_pendiente = true;
    } else if (control.comando == COMANDO_REINICIAR_PRUEBA) {
      reinicio_pendiente = true;
    }

    return;
  }

  if (memcmp(info->src_addr, ESPNOW_MAC_FINAL_ACCELERATION, 6) == 0 && longitud == sizeof(paquete_final_acceleration)) {
    paquete_final_acceleration mensaje;
    memcpy(&mensaje, datos_recibidos, sizeof(mensaje));
    recibir_paquete_acceleration(mensaje);
  }
}

static bool anadir_peer(const uint8_t *mac) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void setup() {
  Serial.begin(115200);

  pinMode(SENSOR_ACCELERATION, INPUT_PULLUP);
  pinMode(SENSOR_SKIDPAD, INPUT_PULLUP);
  pinMode(SENSOR_AUTOX_ENDURANCE, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(recibir_espnow);

  if (!anadir_peer(ESPNOW_MAC_RECEPTOR_BOX)) Serial.println("Error anadiendo el receptor del box");
  if (!anadir_peer(ESPNOW_MAC_FINAL_ACCELERATION)) Serial.println("Error anadiendo el sensor final de Acceleration");

  Serial.println("Emisor de pruebas listo");
  Serial.println("Esperando que el receptor del box seleccione una prueba");
}

void loop() {
  if (cambio_prueba_pendiente) {
    cambio_prueba_pendiente = false;
    seleccionar_prueba(prueba_pendiente);
  }

  if (reinicio_pendiente) {
    reinicio_pendiente = false;
    reiniciar_prueba_actual();
  }

  switch (prueba_activa) {
    case PRUEBA_ACCELERATION: prueba_acceleration(); break;
    case PRUEBA_SKIDPAD: prueba_skidpad(); break;
    case PRUEBA_AUTOX_ENDURANCE: prueba_autox_endurance(); break;
    default: delay(10); break;
  }
}
