#include <esp_now.h>
#include <WiFi.h>

const int sensorPin = 13;
bool lastState = HIGH;

// MAC de la Master
uint8_t masterAddress[] = {0x28, 0x05, 0xA5, 0xE1, 0xCF, 0x90};

typedef struct struct_message {
  bool handshake;
  bool trigger;
} struct_message;

struct_message msg;

void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len){
  struct_message incomingMsg;
  memcpy(&incomingMsg, incomingData, sizeof(incomingMsg));

  if(incomingMsg.handshake){
    Serial.println("🔄 Handshake recibido del Master, enviando confirmación");
    msg.handshake = true;
    esp_now_send(masterAddress, (uint8_t*)&msg, sizeof(msg));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){
    Serial.println("Error al inicializar ESP-NOW");
    return;
  }

  // Añadir Master como peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataReceive);

  Serial.println("Slave listo. Esperando handshake del Master...");
}

void loop() {
  bool sensorState = digitalRead(sensorPin);

  // Detectar sensor final
  if(lastState == HIGH && sensorState == LOW){
    delay(100);  // debounce / margen para paso de coche
    msg.trigger = true;
    esp_now_send(masterAddress, (uint8_t*)&msg, sizeof(msg));
    Serial.println("🏁 Sensor final activado, mensaje enviado al Master");
    msg.trigger = false;  // reset para siguiente vuelta
  }

  lastState = sensorState;
}
