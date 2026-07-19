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

      //Obtenemos el dato actual
      int tempActual = dataProcessor.current_ect_value; 
      int rpmActual = dataProcessor.current_rpm_value;
      float battActual = dataProcessor.current_vbatt_value;

      //Pasamos el dato actual a mensaje para enviarlo 
      String mensaje = String(tempActual) + "|" + String(rpmActual) + "|" + String(battActual, 1);

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