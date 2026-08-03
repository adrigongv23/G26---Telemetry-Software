#include <WiFi.h>
#include <esp_now.h>

#include "configuracion.hpp"
#include "acceleration.hpp"
#include "skidpad.hpp"
#include "autox_endurance.hpp"

paquete_datos datos;
volatile bool datos_nuevos = false;
tipo_prueba prueba_activa = PRUEBA_NINGUNA;

static void mostrar_menu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("       SELECCION DE PRUEBA - BOX");
  Serial.println("========================================");
  Serial.println("1 -> Acceleration");
  Serial.println("2 -> Skidpad");
  Serial.println("3 -> Autocross / Endurance");
  Serial.println("0 -> Reiniciar prueba actual");
  Serial.println("m -> Mostrar este menu");
  Serial.println("========================================");
}

static void enviar_control(uint8_t comando, tipo_prueba prueba) {
  paquete_control control;
  control.comando = comando;
  control.prueba = prueba;

  esp_err_t resultado = esp_now_send(ESPNOW_MAC_EMISOR_PRUEBAS, (uint8_t*)&control, sizeof(control));
  if (resultado != ESP_OK) Serial.println("Error enviando el comando al emisor");
}

static void reiniciar_procesadores() {
  reiniciar_receptor_acceleration();
  reiniciar_receptor_skidpad();

  if(prueba_activa==PRUEBA_AUTOX_ENDURANCE) reiniciar_receptor_autox_endurance();

}

static void seleccionar_prueba(tipo_prueba nueva_prueba) {
  prueba_activa = nueva_prueba;

  memset(&datos, 0, sizeof(datos));
  datos_nuevos = false;
  reiniciar_procesadores();

  Serial.println();

  switch (prueba_activa) {
    case PRUEBA_ACCELERATION: Serial.println("Seleccionando ACCELERATION..."); break;
    case PRUEBA_SKIDPAD: Serial.println("Seleccionando SKIDPAD..."); break;
    case PRUEBA_AUTOX_ENDURANCE: Serial.println("Seleccionando AUTOX / ENDURANCE..."); break;
    default: return;
  }

  enviar_control(COMANDO_SELECCIONAR_PRUEBA, prueba_activa);
}

static void reiniciar_prueba() {
  if (prueba_activa == PRUEBA_NINGUNA) {
    Serial.println("Primero selecciona una prueba");
    return;
  }

  memset(&datos, 0, sizeof(datos));
  datos_nuevos = false;
  reiniciar_procesadores();

  enviar_control(COMANDO_REINICIAR_PRUEBA, prueba_activa);
  Serial.println("Orden de reinicio enviada al emisor");
}

static void procesar_datos() {
  if (datos.estado == 100) {
    Serial.println("Prueba confirmada por el emisor");
    return;
  }

  if (datos.estado == 101) {
    Serial.println("El emisor ha reiniciado la prueba");
    return;
  }

  switch ((tipo_prueba)datos.prueba) {
    case PRUEBA_ACCELERATION: procesar_acceleration(datos); break;
    case PRUEBA_SKIDPAD: procesar_skidpad(datos); break;
    case PRUEBA_AUTOX_ENDURANCE: procesar_autox_endurance(datos); break;
    default: Serial.println("Paquete recibido sin una prueba valida"); break;
  }
}

static void recibir_datos(const esp_now_recv_info_t *info, const uint8_t *datos_emitidos, int longitud) {
  if (memcmp(info->src_addr, ESPNOW_MAC_EMISOR_PRUEBAS, 6) != 0) return;
  if (longitud != sizeof(paquete_datos)) return;

  memcpy(&datos, datos_emitidos, sizeof(datos));
  datos_nuevos = true;
}

static void leer_monitor_serie() {
  while (Serial.available() > 0) {
    char comando = Serial.read();

    switch (comando) {
      case '1': seleccionar_prueba(PRUEBA_ACCELERATION); break;
      case '2': seleccionar_prueba(PRUEBA_SKIDPAD); break;
      case '3': seleccionar_prueba(PRUEBA_AUTOX_ENDURANCE); break;
      case '0': reiniciar_prueba(); break;
      case 'm':
      case 'M': mostrar_menu(); break;
      case '\n':
      case '\r': break;
      default: Serial.println("Comando no valido. Escribe m para ver el menu"); break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }

  esp_now_peer_info_t emisor = {};
  memcpy(emisor.peer_addr, ESPNOW_MAC_EMISOR_PRUEBAS, 6);
  emisor.channel = 0;
  emisor.encrypt = false;

  esp_now_register_recv_cb(recibir_datos);

  if (esp_now_add_peer(&emisor) != ESP_OK) {
    Serial.println("Error anadiendo el emisor de pruebas");
    return;
  }

  Serial.println("Receptor de pruebas del box listo");
  mostrar_menu();
}

void loop() {
  leer_monitor_serie();

  if (datos_nuevos) {
    datos_nuevos = false;
    procesar_datos();
  }
}
