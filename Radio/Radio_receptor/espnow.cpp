#include "espnow.hpp"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#define ESPNOW_CHANNEL 1

// Potencia en cuartos de dBm: 84 = 21 dBm teorico.
 // OJO: no todos los ESP32/modulos llegan a 21 dBm; si el driver no lo acepta, lo limita/falla.
 // Con antenas externas hay que respetar la potencia EIRP legal del evento/pais.
#define ESPNOW_TX_POWER_QDBM 84

// MAC WiFi STA FIJA del ESP32-WROOM-32U INTERMEDIARIO del piloto.
// CAMBIAR OBLIGATORIAMENTE por la MAC que imprime el intermediario.
// Ejemplo: 7C:DF:A1:12:34:56 -> {0x7C, 0xDF, 0xA1, 0x12, 0x34, 0x56}
static const uint8_t ESPNOW_PEER_INTERMEDIARIO_MAC[6] = {
    0x28, 0x05, 0xA5, 0xE1, 0xCF, 0x90
};

static volatile bool espnow_send_busy = false;
static volatile bool espnow_last_send_ok = false;

#define ESPNOW_SEND_RETRIES     3
#define ESPNOW_SEND_TIMEOUT_MS  35

/* =========================
   COLA ESP-NOW RX -> PC
   ========================= */

// IMPORTANTE:
// No conviene hacer Serial.write() dentro del callback de ESP-NOW. A 115200 baudios,
// escribir un chunk de ~211 bytes puede bloquear unos 18 ms. Eso puede hacer que se
// pierdan paquetes ESP-NOW que llegan justo despues. Por eso el callback solo copia
// el paquete a esta cola y el loop() lo manda al PC.
#define ESPNOW_RX_QUEUE_SIZE 64

static uint8_t rx_queue[ESPNOW_RX_QUEUE_SIZE][ESPNOW_PACKET_MAX_LEN];
static uint16_t rx_queue_len[ESPNOW_RX_QUEUE_SIZE];
static volatile uint8_t rx_queue_head = 0;
static volatile uint8_t rx_queue_tail = 0;
static volatile uint32_t rx_queue_dropped = 0;

static uint16_t read_le_u16(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool mac_is_zero(const uint8_t* mac)
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
    return memcmp(mac, zero, 6) == 0;
}

static bool paquete_audio_valido(const uint8_t* data, int len)
{
    if (data == nullptr || len < ESPNOW_PACKET_HEADER_LEN || len > ESPNOW_PACKET_MAX_LEN) {
        return false;
    }

    if (data[0] != 0xE5 || data[1] != 0x5E) {
        return false;
    }

    uint8_t chunk_index = data[3];
    uint8_t chunk_total = data[4];
    uint16_t frame_len = read_le_u16(&data[5]);
    uint16_t offset = read_le_u16(&data[7]);
    uint16_t chunk_len = read_le_u16(&data[9]);

    if (chunk_total == 0 || chunk_total > 16) {
        return false;
    }

    if (chunk_index >= chunk_total) {
        return false;
    }

    if (frame_len == 0 || frame_len > 2000) {
        return false;
    }

    if (chunk_len == 0 || chunk_len > ESPNOW_AUDIO_CHUNK) {
        return false;
    }

    if ((uint32_t)offset + (uint32_t)chunk_len > (uint32_t)frame_len) {
        return false;
    }

    if (len != (int)ESPNOW_PACKET_HEADER_LEN + (int)chunk_len) {
        return false;
    }

    return true;
}

static bool paquete_control_valido(const uint8_t* data, int len)
{
    return data != nullptr &&
           len == RADIO_CONTROL_PACKET_LEN &&
           data[0] == RADIO_CONTROL_MAGIC_1 &&
           data[1] == RADIO_CONTROL_MAGIC_2 &&
           data[2] == RADIO_CONTROL_COMMAND_BOX_PTT &&
           (data[3] == 0 || data[3] == 1);
}

static bool paquete_radio_valido(const uint8_t* data, int len)
{
    return paquete_audio_valido(data, len) || paquete_control_valido(data, len);
}

static bool ensure_peer_registered(const uint8_t* mac)
{
    if (mac == nullptr || mac_is_zero(mac)) {
        return false;
    }

    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;

    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

// Arduino-ESP32 3.x usa esta firma para el callback de envio.
// Antes era: const uint8_t* mac_addr.
static void on_espnow_sent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    espnow_last_send_ok = (status == ESP_NOW_SEND_SUCCESS);
    espnow_send_busy = false;
}

// ESP-NOW -> cola -> UART/USB -> PC
static void on_espnow_recv(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    if (!paquete_audio_valido(data, len)) {
        return;
    }

    // Configuracion de peer fija: ignorar paquetes que no vengan del WROOM-U intermediario.
    if (info == nullptr || info->src_addr == nullptr ||
        memcmp(info->src_addr, ESPNOW_PEER_INTERMEDIARIO_MAC, 6) != 0) {
        return;
    }

    uint8_t next_head = (uint8_t)((rx_queue_head + 1) % ESPNOW_RX_QUEUE_SIZE);

    if (next_head == rx_queue_tail) {
        rx_queue_dropped++;
        return;
    }

    memcpy(rx_queue[rx_queue_head], data, len);
    rx_queue_len[rx_queue_head] = (uint16_t)len;
    rx_queue_head = next_head;
}

static bool pop_espnow_rx_packet(uint8_t* out_packet, uint16_t* out_len)
{
    if (rx_queue_tail == rx_queue_head) {
        return false;
    }

    uint8_t index = rx_queue_tail;

    memcpy(out_packet, rx_queue[index], rx_queue_len[index]);
    *out_len = rx_queue_len[index];

    rx_queue_tail = (uint8_t)((rx_queue_tail + 1) % ESPNOW_RX_QUEUE_SIZE);
    return true;
}

static bool enviar_paquete_espnow(const uint8_t* data, size_t len)
{
    if (!paquete_radio_valido(data, (int)len)) {
        return false;
    }

    if (!ensure_peer_registered(ESPNOW_PEER_INTERMEDIARIO_MAC)) {
        return false;
    }

    // Reintentos cortos: a larga distancia algun paquete puede fallar.
    // No reintentamos infinito para no meter retraso enorme al audio.
    for (uint8_t intento = 0; intento < ESPNOW_SEND_RETRIES; intento++) {

        uint32_t inicio_busy = millis();
        while (espnow_send_busy && (millis() - inicio_busy) < ESPNOW_SEND_TIMEOUT_MS) {
            delay(1);
        }

        if (espnow_send_busy) {
            espnow_send_busy = false;
        }

        espnow_last_send_ok = false;
        espnow_send_busy = true;

        esp_err_t err = esp_now_send(ESPNOW_PEER_INTERMEDIARIO_MAC, data, len);

        if (err != ESP_OK) {
            espnow_send_busy = false;
#if DEBUG_SERIAL
            Serial.print("ERROR esp_now_send: ");
            Serial.println((int)err);
#endif
            delay(1);
            continue;
        }

        uint32_t inicio_ack = millis();
        while (espnow_send_busy && (millis() - inicio_ack) < ESPNOW_SEND_TIMEOUT_MS) {
            delay(1);
        }

        if (!espnow_send_busy && espnow_last_send_ok) {
            return true;
        }

        delay(1);
    }

    return false;
}

bool init_espnow_puente_bidir()
{
    // 1. Modo WiFi Station para poder usar ESP-NOW.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);

    // 2. Sin ahorro de energia para reducir latencia/cortes.
    esp_wifi_set_ps(WIFI_PS_NONE);

    // 3. Potencia de transmision alta.
    esp_wifi_set_max_tx_power(ESPNOW_TX_POWER_QDBM);

    // 4. Activar protocolos WiFi, incluido Long Range.
    esp_wifi_set_protocol(
        WIFI_IF_STA,
        WIFI_PROTOCOL_11B |
        WIFI_PROTOCOL_11G |
        WIFI_PROTOCOL_11N |
        WIFI_PROTOCOL_LR
    );

    // 5. Fijar canal. El otro ESP-NOW tiene que usar el mismo canal.
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // 6. Iniciar ESP-NOW.
    if (esp_now_init() != ESP_OK) {
#if DEBUG_SERIAL
        Serial.println("ERROR: no se pudo iniciar ESP-NOW");
#endif
        return false;
    }

    // 7. Callback de envio: libera la bandera espnow_send_busy.
    if (esp_now_register_send_cb(on_espnow_sent) != ESP_OK) {
#if DEBUG_SERIAL
        Serial.println("ERROR: no se pudo registrar callback de envio ESP-NOW");
#endif
        return false;
    }

    // 8. Callback de recepcion: ESP-NOW -> cola. El loop lo manda al PC.
    if (esp_now_register_recv_cb(on_espnow_recv) != ESP_OK) {
#if DEBUG_SERIAL
        Serial.println("ERROR: no se pudo registrar callback de recepcion ESP-NOW");
#endif
        return false;
    }

    // 9. Registrar la MAC fija del WROOM-U intermediario.
    if (!ensure_peer_registered(ESPNOW_PEER_INTERMEDIARIO_MAC)) {
#if DEBUG_SERIAL
        Serial.println("ERROR: configura ESPNOW_PEER_INTERMEDIARIO_MAC con la MAC real del WROOM-U");
#endif
        return false;
    }

#if DEBUG_SERIAL
    int8_t txPower = 0;
    esp_wifi_get_max_tx_power(&txPower);

    Serial.println("ESP-NOW puente bidireccional iniciado correctamente");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Canal ESP-NOW: ");
    Serial.println(ESPNOW_CHANNEL);
    Serial.print("Potencia TX configurada: ");
    Serial.print(txPower * 0.25f);
    Serial.println(" dBm aprox");
#endif

    return true;
}

// ESP-NOW -> UART/USB -> PC. Se llama desde loop(), no desde el callback.
void procesar_espnow_y_enviar_pc()
{
    static uint8_t packet[ESPNOW_PACKET_MAX_LEN];
    uint16_t len = 0;
    uint8_t processed = 0;

    while (processed < MAX_ESPNOW_TO_PC_PACKETS_PER_PASS &&
           pop_espnow_rx_packet(packet, &len)) {
        Serial.write(packet, len);
        ++processed;
    }
}

// UART/USB -> ESP-NOW
// Lee paquetes binarios que vienen del programa de Windows y los manda por ESP-NOW LR.
void procesar_uart_y_enviar_espnow()
{
    enum EstadoParser {
        ESPERANDO_MAGIC_1,
        ESPERANDO_MAGIC_2,
        LEYENDO_CABECERA,
        LEYENDO_AUDIO,
        LEYENDO_CONTROL
    };

    static EstadoParser estado = ESPERANDO_MAGIC_1;
    static uint8_t packet[ESPNOW_PACKET_MAX_LEN];
    static size_t index = 0;
    static uint16_t chunk_len = 0;
    uint8_t processed_packets = 0;

    auto reset_parser = [&]() {
        index = 0;
        chunk_len = 0;
        estado = ESPERANDO_MAGIC_1;
    };

    while (Serial.available() > 0) {
        uint8_t b = (uint8_t)Serial.read();

        switch (estado) {
            case ESPERANDO_MAGIC_1:
                if (b == RADIO_CONTROL_MAGIC_1) {
                    packet[0] = b;
                    index = 1;
                    estado = ESPERANDO_MAGIC_2;
                }
                break;

            case ESPERANDO_MAGIC_2:
                if (b == 0x5E) {
                    packet[1] = b;
                    index = 2;
                    estado = LEYENDO_CABECERA;
                } else if (b == RADIO_CONTROL_MAGIC_2) {
                    packet[1] = b;
                    index = 2;
                    estado = LEYENDO_CONTROL;
                } else if (b == RADIO_CONTROL_MAGIC_1) {
                    // Puede ser el inicio real de otro paquete.
                    packet[0] = b;
                    index = 1;
                } else {
                    reset_parser();
                }
                break;

            case LEYENDO_CONTROL:
                packet[index++] = b;

                if (index >= RADIO_CONTROL_PACKET_LEN) {
                    if (paquete_control_valido(packet, (int)index)) {
                        enviar_paquete_espnow(packet, index);
                    }
                    reset_parser();

                    ++processed_packets;
                    if (processed_packets >= MAX_PC_TO_ESPNOW_PACKETS_PER_PASS) {
                        return;
                    }
                }
                break;

            case LEYENDO_CABECERA:
                packet[index++] = b;

                if (index >= ESPNOW_PACKET_HEADER_LEN) {
                    uint16_t frame_len = read_le_u16(&packet[5]);
                    uint16_t offset = read_le_u16(&packet[7]);
                    chunk_len = read_le_u16(&packet[9]);

                    bool cabecera_ok = true;

                    if (chunk_len == 0 || chunk_len > ESPNOW_AUDIO_CHUNK) {
                        cabecera_ok = false;
                    }

                    if (frame_len == 0 || frame_len > 2000) {
                        cabecera_ok = false;
                    }

                    if ((uint32_t)offset + (uint32_t)chunk_len > (uint32_t)frame_len) {
                        cabecera_ok = false;
                    }

                    if (!cabecera_ok) {
                        reset_parser();
                        break;
                    }

                    estado = LEYENDO_AUDIO;
                }
                break;

            case LEYENDO_AUDIO:
                packet[index++] = b;

                if (index >= (size_t)ESPNOW_PACKET_HEADER_LEN + chunk_len) {
                    enviar_paquete_espnow(packet, index);
                    reset_parser();

                    ++processed_packets;
                    if (processed_packets >= MAX_PC_TO_ESPNOW_PACKETS_PER_PASS) {
                        return;
                    }
                }
                break;
        }
    }
}
