#include "include/data_processor.hpp"
#include "include/can.hpp"
#include "include/common_libraries.hpp"

DataProcessor dataProcessor;
CAN canController;

// SD
SPIClass spiSD(HSPI);
SdFat sd;
SdFile logFile;

// UDP
WiFiUDP udp;

// --- Tarea UDP (Nucleo 0) ---
void TaskUdpSender(void *pvParameters) {
    Serial.println("Iniciando tarea de envio UDP...");

    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            int tempActual = dataProcessor.current_ect_value;
            int rpmActual = dataProcessor.current_rpm_value;
            float battActual = dataProcessor.current_vbatt_value;
            float tpsActual = dataProcessor.current_tps_value;
            float frenoDelActual = dataProcessor.current_freno_del_value;
            float pcombActual = dataProcessor.current_pcomb_value;
            float taceiteActual = dataProcessor.current_taceite_value;
            float paceiteActual = dataProcessor.current_paceite_value;
            float mapActual = dataProcessor.current_map_value;
            float lambdaActual = dataProcessor.current_lambda_value;
            float lambdaObjActual = dataProcessor.current_lambda_obj_value;

            char mensaje[256];
            snprintf(mensaje, sizeof(mensaje),
                     "ect=%d;rpm=%d;vbatt=%.2f;tps=%.1f;freno_del=%.1f;"
                     "pcomb=%.2f;taceite=%.1f;paceite=%.2f;map=%.1f;lambda=%.3f;lambda_obj=%.3f",
                     tempActual, rpmActual, battActual, tpsActual, frenoDelActual,
                     pcombActual, taceiteActual, paceiteActual, mapActual, lambdaActual, lambdaObjActual);

            udp.beginPacket(IPAddress(255, 255, 255, 255), UDP_PORT);
            udp.print(mensaje);
            udp.endPacket();
        } else {
            Serial.println("[WIFI] Desconectado...");
            WiFi.disconnect();
            WiFi.reconnect();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- G26 TELEMETRY: INICIO DE SISTEMA ---");

    // 1. INICIALIZACION SD
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!sd.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(1), &spiSD))) {
        Serial.println("[FALLO] SD no detectada. El sistema continuara sin Datalogging.");
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
            logFile.println("Time,ECT,RPM,TPS,VBATT,FRENO_DEL,PCOMB,TACEITE,PACEITE,MAP,LAMBDA,LAMBDA_OBJ");
            logFile.sync();
        } else {
            Serial.println("[ERROR] No se pudo abrir archivo de sesion");
        }
    }

    dataProcessor.setLogSystem(&sd, &logFile);

    // 2. INICIAR CAN
    canController.set_data_proccessor(&dataProcessor);
    canController.start();
    canController.start_listening_task();

    // 3. INICIAR WIFI
    Serial.println("--- CONECTANDO WIFI ---");

    IPAddress local_IP(192, 168, 0, 50);
    IPAddress gateway(192, 168, 0, 254);
    IPAddress subnet(255, 255, 255, 0);

    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("[ERR] Fallo al configurar IP estatica");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        delay(500);
        Serial.print(".");
        intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[OK] WiFi Conectado.");
    } else {
        Serial.println("\n[ERR] No se pudo conectar WiFi (Continuando offline).");
    }

    // 4. TAREA UDP
    xTaskCreatePinnedToCore(
        TaskUdpSender,
        "UdpSender",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    Serial.println("[OK] Sistema ONLINE (CAN + SD + WiFi)");
}

void loop() {
    vTaskDelay(5 / portTICK_PERIOD_MS);
}
