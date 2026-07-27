#ifndef COMMON_LIBRARIES_HPP
#define COMMON_LIBRARIES_HPP

#include <Arduino.h>
#include "time.h"
#include <ArduinoJson.h>
#include <vector>

// --- WiFi y UDP ---
#include <WiFi.h>
#include <WiFiUdp.h>

// --- SD ---
#include <SPI.h>
#include "SdFat.h"

// CONFIGURACION WIFI
#define WIFI_SSID "FGades"
#define WIFI_PASSWORD "GadesCPE"

// CONFIGURACION UDP
#define UDP_PORT 4210

// CONFIGURACION SD (HSPI)
#define SD_CS    25
#define SD_MOSI  26
#define SD_SCK   27
#define SD_MISO  14

#endif
