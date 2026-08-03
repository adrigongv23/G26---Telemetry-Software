#pragma once

#include <Arduino.h>

void enviar_datos(uint8_t estado, uint16_t vuelta = 0, unsigned long tiempo = 0, unsigned long derecha = 0, unsigned long izquierda = 0, unsigned long media = 0);
bool detectar_activacion_sensor(int sensorPin, unsigned long minInterval);
