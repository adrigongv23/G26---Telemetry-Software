#pragma once

#include <Arduino.h>

/* MAC del receptor del box */
static const uint8_t ESPNOW_MAC_RECEPTOR_BOX[6] = {
    0x28, 0x05, 0xA5, 0xE1, 0xCA, 0x10
};

/* MAC del ESP32 situado al final de Acceleration */
static const uint8_t ESPNOW_MAC_FINAL_ACCELERATION[6] = {
    0x28, 0x05, 0xA5, 0x0B, 0x44, 0xA8
};


/* Pines de los sensores */
static const int SENSOR_ACCELERATION = 26;
static const int SENSOR_SKIDPAD = 26;
static const int SENSOR_AUTOX_ENDURANCE = 26;

enum tipo_prueba : uint8_t {
    PRUEBA_NINGUNA = 0,
    PRUEBA_ACCELERATION = 1,
    PRUEBA_SKIDPAD = 2,
    PRUEBA_AUTOX_ENDURANCE = 3
};

enum tipo_comando : uint8_t {
    COMANDO_NINGUNO = 0,
    COMANDO_SELECCIONAR_PRUEBA = 1,
    COMANDO_REINICIAR_PRUEBA = 2
};

struct paquete_control {
    uint8_t comando;
    uint8_t prueba;
};

struct paquete_datos {
    uint8_t prueba;
    uint8_t estado;
    uint16_t vuelta;
    unsigned long tiempo;
    unsigned long laptime_derecha;
    unsigned long laptime_izquierda;
    unsigned long average;
};

/* Compatible con el acel_slave original */
struct paquete_final_acceleration {
    bool handshake;
    bool trigger;
};
