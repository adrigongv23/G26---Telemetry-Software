
#include <Arduino.h>
#include <SPI.h>
#include "SdFat.h"
#include "include/can.hpp"
#include "include/data_processor.hpp"

// --- CONFIGURACIÓN SD (VSPI) ---
#define SD_CS_PIN 5
#define SPI_CLOCK SD_SCK_MHZ(20)
SdFat sd;
SdFile logFile;

// Instancias
CAN can_interface;
DataProcessor processor;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- G26 TELEMETRY: INICIO DE SISTEMA ---");

    // 1. INICIALIZACIÓN SD
    if (!sd.begin(SD_CS_PIN, SPI_CLOCK)) {
        Serial.println("[FALLO] SD no detectada. El sistema continuará sin Datalogging.");
    } else {
        Serial.println("[OK] SD Montada.");
        if (!logFile.exists("G26.csv")) {
             if (logFile.open("G26.csv", O_RDWR | O_CREAT | O_AT_END)) {
                 // SOLO Cabeceras de lo que vamos a grabar ahora
                 logFile.println("Time,ECT"); 
                 logFile.close();
             }
        }
    }

    // 2. VINCULACIÓN
    // Le damos al procesador acceso a la SD para que guarde cuando lleguen datos
    processor.setLogSystem(&sd, &logFile);
    can_interface.set_data_proccessor(&processor);

    // 3. INICIO CAN
    can_interface.start();
    can_interface.start_listening_task(); // Escucha en Core 1 (Background)
    
    Serial.println("[OK] Sistema ONLINE.");
}

void loop() {
    
    delay(100); 
}