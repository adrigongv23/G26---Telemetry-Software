#include "include/data_processor.hpp"
#include "include/can.hpp"
#include "include/common_libraries.hpp" 

DataProcessor dataProcessor;
CAN canController;

// Objeto UDP para manejar la conexión UDP
WiFiUDP udp;

//Envio de datos a través de UDP
void TaskUdpSender(void *pvParameters){

  Serial.println("Iniciando tarea de envío...");

  while (true){
    if(WiFi.status() == WL_CONNECTED){

      //Obtenemos los datos actuales
      int tempActual = dataProcessor.current_ect_value;
      int rpmActual = dataProcessor.current_rpm_value;
      float battActual = dataProcessor.current_vbatt_value;

      float tpsActual = dataProcessor.current_tps_value;
      float frenoDelActual = dataProcessor.current_freno_del_value;   // freno delantero (instalado)
      float pcombActual = dataProcessor.current_pcomb_value;
      float taceiteActual = dataProcessor.current_taceite_value;
      float paceiteActual = dataProcessor.current_paceite_value;
      float mapActual = dataProcessor.current_map_value;
      float lambdaActual = dataProcessor.current_lambda_value;
      float lambdaObjActual = dataProcessor.current_lambda_obj_value;
      // Pendientes de montar/configurar en el coche. Cuando se instalen, descomentar aquí
      // y añadir su campo al snprintf; mientras no lleguen, el monitor los muestra como "--".
      //float velocidadActual = dataProcessor.current_velocidad_value;
      //float frenoTraActual  = dataProcessor.current_freno_tra_value;   // freno trasero

      //Formato "clave=valor" separado por ';'
      char mensaje[256];
      snprintf(mensaje, sizeof(mensaje),
               "ect=%d;rpm=%d;vbatt=%.2f;tps=%.1f;freno_del=%.1f;"
               "pcomb=%.2f;taceite=%.1f;paceite=%.2f;map=%.1f;lambda=%.3f;lambda_obj=%.3f",
               tempActual, rpmActual, battActual, tpsActual, frenoDelActual,
               pcombActual, taceiteActual, paceiteActual, mapActual, lambdaActual, lambdaObjActual);
      // Al añadir los pendientes: sumar ";velocidad=%.0f;freno_tra=%.1f" al formato
      // y velocidadActual, frenoTraActual al final de los argumentos.

      //Enviamos el paquete por broadcast a toda la red local (no hace falta conocer la IP del portátil)
      udp.beginPacket(IPAddress(255,255,255,255), UDP_PORT);
      udp.print(mensaje);
      udp.endPacket();

      //Para comprobar que el paquete se está enviado correctamente podemos usar estos prints
      //Serial.print("UDP Enviado: ");
      //Serial.println(mensaje);
    }

    else {
        //Si no hay Wifi o no se consigue conectar, lo intentamos reconectar
        Serial.println("[WIFI] Desconectado...");
        WiFi.disconnect();
        WiFi.reconnect();
    }

    //Ponemos de velocidad de envio 50ms, es ajustable 
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}


void setup() {
    Serial.begin(115200);
    
    // 1. INICIAR CAN Y PANTALLA
    // Pasamos el puntero de dataProcessor al controlador CAN
    canController.set_data_proccessor(&dataProcessor); 
    canController.start();
    canController.start_listening_task();
    
    // 2. INICIAR WIFI
    Serial.println("--- CONECTANDO WIFI ---");

    //IP estática dentro de la red del CPE (DHCP desactivado en el CPE)
    IPAddress local_IP(192, 168, 0, 50);
    IPAddress gateway(192, 168, 0, 254);
    IPAddress subnet(255, 255, 255, 0);

    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("[ERR] Fallo al configurar IP estática");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        delay(500);
        Serial.print(".");
        intentos++;
    }

    if(WiFi.status() == WL_CONNECTED){
       Serial.println("\n[OK] WiFi Conectado.");
    } else {
       Serial.println("\n[ERR] No se pudo conectar WiFi (Continuando offline).");
    }

    //Creamos la tarea UDP para el envio de datos
    //Usaremos el núcleo 1 o 0 ya que la ESP32 es Dual Core
    xTaskCreatePinnedToCore(
      TaskUdpSender,    // Función que debe de ejecutar
      "UdpSender",      // Nombre de la tarea 
      4096,             // Stack size 
      NULL,             // Parámetros extras?
      1,                // Prioridad (1 = Baja, 10 = Alta, la ponemos 1 ya que el CAN debe de tener más prioridad)
      NULL,             // Handle
      0                 // Núcleo
    );

    Serial.println("OK CAN + UDP Sender ");
}

void loop(){ 
    vTaskDelay(5 / portTICK_PERIOD_MS);
}