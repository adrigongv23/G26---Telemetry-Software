#include <Arduino.h>
#include <SPI.h>
#include "SdFat.h"

#include "include/can.hpp"
#include "include/data_processor.hpp"


// --- CONFIGURACIÓN SD (HSPI, pines de la PCB validados por esquemático) ---
#define SD_CS    25
#define SD_MOSI  26
#define SD_SCK   27
#define SD_MISO  14

SPIClass spiSD(HSPI);
#define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(1), &spiSD)

SdFat sd;
SdFile logFile;

// Instancias
CAN can_interface;
DataProcessor processor;

void setup() {
    Serial.begin(2000000);
    delay(1000);
    Serial.println("\n--- G26 TELEMETRY: INICIO DE SISTEMA ---");

    // 1. INICIALIZACIÓN SD
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!sd.begin(SD_CONFIG)) {
        Serial.println("[FALLO] SD no detectada. El sistema continuará sin Datalogging.");
    } else {
        char filename[24];
        int session = 1;
        bool opened = false;

        while (!opened && session < 100000) {
            snprintf(filename, sizeof(filename), "G26-%d.csv", session);
            if (logFile.open(filename, O_RDWR | O_CREAT | O_EXCL)) {
                opened = true;
                Serial.printf("[OK] Nueva sesion: %s\n", filename);
            } else {
                session++;
            }
        }

        if (opened) {
            logFile.println("Time,ECT,RPM,TPS,VBATT");
            logFile.sync();
        } else {
            Serial.println("[ERROR] No se pudo abrir archivo de sesion");
        }
    }

    // 2. VINCULACIÓN
    processor.setLogSystem(&sd, &logFile);
    can_interface.set_data_proccessor(&processor);

    // 3. INICIO CAN
    can_interface.start();
    can_interface.start_listening_task();

    Serial.println("[OK] Sistema ONLINE.");
}

void loop() {
    delay(100);
}