#pragma once

#include <Arduino.h>

bool bridge_init();
void bridge_process();
bool bridge_send_espnow(const byte *data, size_t len, byte intentos);
