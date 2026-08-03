#include "bridge.hpp"
#include "config.hpp"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct EspNowRxItem {
    uint32_t epoch;
    uint16_t len;
    uint8_t data[RADIO_PACKET_MAX_LEN];
};


static HardwareSerial PilotUart(2);
static QueueHandle_t box_audio_queue = nullptr;

static portMUX_TYPE control_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool box_ptt_active = false;
static volatile bool box_stop_pending = false;
static volatile uint32_t box_stream_epoch = 0;
static volatile uint32_t box_control_version = 0;

static volatile bool espnow_send_busy = false;
static volatile bool espnow_last_send_ok = false;
static volatile uint32_t espnow_rx_dropped = 0;

/* Diagnóstico permanente: no modifica el protocolo ni las MAC. */
static volatile uint32_t debug_uart_rx_bytes = 0;
static volatile uint32_t debug_pilot_audio_packets = 0;
static volatile uint32_t debug_pilot_invalid_packets = 0;
static volatile uint32_t debug_espnow_api_ok = 0;
static volatile uint32_t debug_espnow_api_fail = 0;
static volatile uint32_t debug_espnow_ack_ok = 0;
static volatile uint32_t debug_espnow_ack_fail = 0;
static volatile uint32_t debug_espnow_forward_fail = 0;
static volatile uint32_t debug_espnow_rx_raw = 0;
static volatile uint32_t debug_espnow_rx_from_box = 0;
static volatile uint32_t debug_espnow_rx_foreign_mac = 0;
static volatile uint32_t debug_box_audio_packets = 0;
static volatile uint32_t debug_box_control_packets = 0;
static volatile uint32_t debug_box_invalid_packets = 0;
static volatile uint32_t debug_uart_tx_bytes = 0;
static volatile uint32_t debug_uart_tx_fail = 0;
static uint32_t debug_last_report_ms = 0;

static uint32_t last_control_version_sent = 0;
static uint32_t last_control_epoch_sent = 0;
static bool last_control_state_sent = false;
static uint32_t last_control_sent_ms = 0;
static volatile uint32_t debug_control_heartbeats = 0;

static uint16_t read_le_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool mac_is_zero(const uint8_t *mac)
{
    static const uint8_t zero[6] = {};
    return mac == nullptr || memcmp(mac, zero, sizeof(zero)) == 0;
}

static bool audio_packet_valid(const uint8_t *data, int len)
{
    if (data == nullptr || len < RADIO_PACKET_HEADER_LEN || len > RADIO_PACKET_MAX_LEN) {
        return false;
    }

    if (data[0] != RADIO_AUDIO_MAGIC_1 || data[1] != RADIO_AUDIO_MAGIC_2) {
        return false;
    }

    const uint8_t chunk_index = data[3];
    const uint8_t chunk_total = data[4];
    const uint16_t frame_len = read_le_u16(&data[5]);
    const uint16_t offset = read_le_u16(&data[7]);
    const uint16_t chunk_len = read_le_u16(&data[9]);

    if (chunk_total == 0 || chunk_total > 16 || chunk_index >= chunk_total) {
        return false;
    }

    if (frame_len == 0 || frame_len > 2000) {
        return false;
    }

    if (chunk_len == 0 || chunk_len > RADIO_AUDIO_CHUNK) {
        return false;
    }

    if ((uint32_t)offset + chunk_len > frame_len) {
        return false;
    }

    return len == RADIO_PACKET_HEADER_LEN + chunk_len;
}

static bool control_packet_valid(const uint8_t *data, int len)
{
    return data != nullptr &&
           len == RADIO_CONTROL_PACKET_LEN &&
           data[0] == RADIO_CONTROL_MAGIC_1 &&
           data[1] == RADIO_CONTROL_MAGIC_2 &&
           data[2] == RADIO_CONTROL_COMMAND_BOX_PTT &&
           (data[3] == 0 || data[3] == 1);
}

static void snapshot_control(
    bool *active,
    bool *stop_pending,
    uint32_t *epoch,
    uint32_t *version)
{
    portENTER_CRITICAL(&control_mux);
    if (active != nullptr) {
        *active = box_ptt_active;
    }
    if (stop_pending != nullptr) {
        *stop_pending = box_stop_pending;
    }
    if (epoch != nullptr) {
        *epoch = box_stream_epoch;
    }
    if (version != nullptr) {
        *version = box_control_version;
    }
    portEXIT_CRITICAL(&control_mux);
}

static bool ensure_peer_registered(const uint8_t *mac)
{
    if (mac_is_zero(mac)) {
        return false;
    }

    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    return esp_now_add_peer(&peer) == ESP_OK;
}

static void on_espnow_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;

    espnow_last_send_ok = (status == ESP_NOW_SEND_SUCCESS);

    if (espnow_last_send_ok) {
        ++debug_espnow_ack_ok;
    } else {
        ++debug_espnow_ack_fail;
    }

    espnow_send_busy = false;
}

static void on_espnow_recv(
    const esp_now_recv_info_t *info,
    const uint8_t *data,
    int len)
{
    ++debug_espnow_rx_raw;

    if (info == nullptr || info->src_addr == nullptr || data == nullptr) {
        ++debug_box_invalid_packets;
        return;
    }

    if (memcmp(info->src_addr, BOX_RECEIVER_MAC, 6) != 0) {
        ++debug_espnow_rx_foreign_mac;
        return;
    }

    ++debug_espnow_rx_from_box;

    if (control_packet_valid(data, len)) {
        ++debug_box_control_packets;
        const bool active = data[3] != 0;

        portENTER_CRITICAL(&control_mux);
        if (active) {
            // Un START nuevo cancela un cierre pendiente. Si el stream ya estaba
            // activo no se cambia el epoch, por lo que la cola anterior termina
            // y la nueva pulsación continúa sin cortes.
            if (!box_ptt_active) {
                box_ptt_active = true;
                ++box_stream_epoch;
                ++box_control_version;
            }
            box_stop_pending = false;
        } else if (box_ptt_active) {
            // STOP no se reenvía todavía: primero se vacía la cola de audio que
            // ya llegó por ESP-NOW para no cortar el final del mensaje.
            box_stop_pending = true;
        }
        portEXIT_CRITICAL(&control_mux);
        return;
    }

    if (!audio_packet_valid(data, len) || box_audio_queue == nullptr) {
        ++debug_box_invalid_packets;
        return;
    }

    ++debug_box_audio_packets;

    bool active = false;
    bool stop_pending = false;
    uint32_t epoch = 0;
    snapshot_control(&active, &stop_pending, &epoch, nullptr);

    if (!active || stop_pending) {
        return;
    }

    EspNowRxItem item = {};
    item.epoch = epoch;
    item.len = (uint16_t)len;
    memcpy(item.data, data, len);

    if (xQueueSend(box_audio_queue, &item, 0) != pdTRUE) {
        EspNowRxItem discarded = {};
        xQueueReceive(box_audio_queue, &discarded, 0);
        if (xQueueSend(box_audio_queue, &item, 0) != pdTRUE) {
            ++espnow_rx_dropped;
        }
    }
}

static bool uart_write_all(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0) {
        return false;
    }

    size_t sent = 0;
    const uint32_t started = millis();

    while (sent < len) {
        sent += PilotUart.write(data + sent, len - sent);

        if (sent >= len) {
            debug_uart_tx_bytes += (uint32_t)len;
            return true;
        }

        if ((uint32_t)(millis() - started) >= PILOT_UART_WRITE_TIMEOUT_MS) {
            ++debug_uart_tx_fail;
            return false;
        }

        delay(0);
    }

    return true;
}

static bool send_pending_control_to_pilot()
{
    bool active = false;
    uint32_t epoch = 0;
    uint32_t version = 0;
    snapshot_control(&active, nullptr, &epoch, &version);

    const bool heartbeat_due =
        active &&
        (uint32_t)(millis() - last_control_sent_ms) >= BOX_PTT_HEARTBEAT_MS;

    if (version == last_control_version_sent && !heartbeat_due) {
        return true;
    }

    const uint8_t packet[RADIO_CONTROL_PACKET_LEN] = {
        RADIO_CONTROL_MAGIC_1,
        RADIO_CONTROL_MAGIC_2,
        RADIO_CONTROL_COMMAND_BOX_PTT,
        active ? (uint8_t)1 : (uint8_t)0
    };

    if (!uart_write_all(packet, sizeof(packet))) {
        return false;
    }

    if (version == last_control_version_sent && heartbeat_due) {
        ++debug_control_heartbeats;
    }

    last_control_version_sent = version;
    last_control_epoch_sent = epoch;
    last_control_state_sent = active;
    last_control_sent_ms = millis();

    return true;
}

static bool finalize_box_stop_if_drained()
{
    if (box_audio_queue == nullptr || uxQueueMessagesWaiting(box_audio_queue) != 0) {
        return false;
    }

    bool changed = false;

    portENTER_CRITICAL(&control_mux);
    if (box_stop_pending && box_ptt_active) {
        box_stop_pending = false;
        box_ptt_active = false;
        ++box_stream_epoch;
        ++box_control_version;
        changed = true;
    }
    portEXIT_CRITICAL(&control_mux);

    return changed;
}

static void forward_box_audio_to_pilot()
{
    if (!send_pending_control_to_pilot() || box_audio_queue == nullptr) {
        return;
    }

    EspNowRxItem item = {};
    uint8_t processed = 0;

    // Limitar cada pasada evita que una cola grande bloquee el sentido contrario.
    while (processed < MAX_BOX_TO_PILOT_PACKETS_PER_PASS &&
           xQueueReceive(box_audio_queue, &item, 0) == pdTRUE) {
        ++processed;

        bool active = false;
        uint32_t epoch = 0;
        uint32_t version = 0;
        snapshot_control(&active, nullptr, &epoch, &version);

        if (!active || item.epoch != epoch) {
            continue;
        }

        if (last_control_version_sent != version ||
            last_control_epoch_sent != epoch ||
            !last_control_state_sent) {
            if (!send_pending_control_to_pilot()) {
                return;
            }
        }

        if (!uart_write_all(item.data, item.len)) {
            Serial.println("ERROR: no se pudo enviar audio al Audio Kit por UART");
            return;
        }
    }

    // El STOP solo viaja después del último paquete que ya estaba en la cola.
    if (finalize_box_stop_if_drained()) {
        send_pending_control_to_pilot();
    }
}

bool bridge_send_espnow(const byte *data, size_t len, byte intentos)
{
    if (data == nullptr || len == 0 || len > 250 || intentos == 0) {
        return false;
    }

    if (!ensure_peer_registered(BOX_RECEIVER_MAC)) {
        return false;
    }

    for (byte attempt = 0; attempt < intentos; ++attempt) {
        const uint32_t wait_start = millis();
        while (espnow_send_busy &&
               (uint32_t)(millis() - wait_start) < ESPNOW_SEND_TIMEOUT_MS) {
            delay(1);
        }

        if (espnow_send_busy) {
            espnow_send_busy = false;
        }

        espnow_last_send_ok = false;
        espnow_send_busy = true;

        const esp_err_t err = esp_now_send(BOX_RECEIVER_MAC, data, len);
        if (err != ESP_OK) {
            ++debug_espnow_api_fail;
            espnow_send_busy = false;
            delay(1);
            continue;
        }

        ++debug_espnow_api_ok;

        const uint32_t ack_start = millis();
        while (espnow_send_busy &&
               (uint32_t)(millis() - ack_start) < ESPNOW_SEND_TIMEOUT_MS) {
            delay(1);
        }

        if (!espnow_send_busy && espnow_last_send_ok) {
            return true;
        }

        delay(1);
    }

    ++debug_espnow_forward_fail;
    return false;
}

static bool send_audio_packet(const byte *data, size_t len)
{
    if (!audio_packet_valid(data, (int)len)) {
        return false;
    }

    return bridge_send_espnow(data, len, ESPNOW_SEND_RETRIES);
}

static void process_pilot_uart_to_espnow()
{
    enum ParserState {
        WAIT_MAGIC_1,
        WAIT_MAGIC_2,
        READ_AUDIO_HEADER,
        READ_AUDIO_BODY,
        SKIP_CONTROL
    };

    static ParserState state = WAIT_MAGIC_1;
    static uint8_t packet[RADIO_PACKET_MAX_LEN] = {};
    static size_t index = 0;
    static uint16_t expected_audio_len = 0;
    static uint8_t control_bytes = 0;
    static uint32_t last_byte_ms = 0;
    uint8_t processed_packets = 0;

    auto reset_parser = [&]() {
        state = WAIT_MAGIC_1;
        index = 0;
        expected_audio_len = 0;
        control_bytes = 0;
    };

    while (PilotUart.available() > 0) {
        const int value = PilotUart.read();
        if (value < 0) {
            break;
        }

        const uint8_t b = (uint8_t)value;
        ++debug_uart_rx_bytes;

        const uint32_t now = millis();

        if (state != WAIT_MAGIC_1 && (uint32_t)(now - last_byte_ms) > 120) {
            ++debug_pilot_invalid_packets;
            reset_parser();
        }
        last_byte_ms = now;

        switch (state) {
            case WAIT_MAGIC_1:
                if (b == RADIO_AUDIO_MAGIC_1) {
                    packet[0] = b;
                    index = 1;
                    state = WAIT_MAGIC_2;
                }
                break;

            case WAIT_MAGIC_2:
                if (b == RADIO_AUDIO_MAGIC_2) {
                    packet[1] = b;
                    index = 2;
                    state = READ_AUDIO_HEADER;
                } else if (b == RADIO_CONTROL_MAGIC_2) {
                    // El Audio Kit no necesita mandar controles al box, pero se
                    // consume un posible paquete para mantener sincronía.
                    control_bytes = 2;
                    state = SKIP_CONTROL;
                } else if (b == RADIO_AUDIO_MAGIC_1) {
                    packet[0] = b;
                    index = 1;
                } else {
                    reset_parser();
                }
                break;

            case SKIP_CONTROL:
                ++control_bytes;
                if (control_bytes >= RADIO_CONTROL_PACKET_LEN) {
                    reset_parser();
                }
                break;

            case READ_AUDIO_HEADER:
                if (index >= sizeof(packet)) {
                    reset_parser();
                    break;
                }

                packet[index++] = b;
                if (index >= RADIO_PACKET_HEADER_LEN) {
                    const uint16_t frame_len = read_le_u16(&packet[5]);
                    const uint16_t offset = read_le_u16(&packet[7]);
                    expected_audio_len = read_le_u16(&packet[9]);

                    if (packet[4] == 0 || packet[4] > 16 ||
                        packet[3] >= packet[4] ||
                        frame_len == 0 || frame_len > 2000 ||
                        expected_audio_len == 0 || expected_audio_len > RADIO_AUDIO_CHUNK ||
                        (uint32_t)offset + expected_audio_len > frame_len) {
                        ++debug_pilot_invalid_packets;
                        reset_parser();
                        break;
                    }

                    state = READ_AUDIO_BODY;
                }
                break;

            case READ_AUDIO_BODY:
                if (index >= sizeof(packet)) {
                    reset_parser();
                    break;
                }

                packet[index++] = b;
                if (index >= RADIO_PACKET_HEADER_LEN + expected_audio_len) {
                    ++debug_pilot_audio_packets;

                    send_audio_packet(packet, index);
                    reset_parser();

                    ++processed_packets;
                    if (processed_packets >= MAX_PILOT_TO_BOX_PACKETS_PER_PASS) {
                        return;
                    }
                }
                break;
        }
    }
}

static void print_permanent_debug()
{
    const uint32_t now = millis();
    if ((uint32_t)(now - debug_last_report_ms) < INTERMEDIARY_DEBUG_INTERVAL_MS) {
        return;
    }
    debug_last_report_ms = now;

    bool ptt_active = false;
    bool ptt_stop_pending = false;
    uint32_t ptt_epoch = 0;
    uint32_t ptt_version = 0;
    snapshot_control(&ptt_active, &ptt_stop_pending, &ptt_epoch, &ptt_version);

    const UBaseType_t queue_items =
        box_audio_queue != nullptr ? uxQueueMessagesWaiting(box_audio_queue) : 0;

    Serial.println();
    Serial.println("========== DEBUG INTERMEDIARIO ==========");
    Serial.printf(
        "PILOTO -> UART: bytes=%lu | paquetes_audio=%lu | invalidos=%lu\n",
        (unsigned long)debug_uart_rx_bytes,
        (unsigned long)debug_pilot_audio_packets,
        (unsigned long)debug_pilot_invalid_packets
    );
    Serial.printf(
        "INTERMEDIARIO -> BOX: API_OK=%lu | API_FALLO=%lu | ACK_OK=%lu | ACK_FALLO=%lu | paquetes_fallidos=%lu\n",
        (unsigned long)debug_espnow_api_ok,
        (unsigned long)debug_espnow_api_fail,
        (unsigned long)debug_espnow_ack_ok,
        (unsigned long)debug_espnow_ack_fail,
        (unsigned long)debug_espnow_forward_fail
    );
    Serial.printf(
        "BOX -> ESP-NOW: RAW=%lu | desde_MAC_box=%lu | audio=%lu | controles=%lu | invalidos=%lu | MAC_ajena=%lu\n",
        (unsigned long)debug_espnow_rx_raw,
        (unsigned long)debug_espnow_rx_from_box,
        (unsigned long)debug_box_audio_packets,
        (unsigned long)debug_box_control_packets,
        (unsigned long)debug_box_invalid_packets,
        (unsigned long)debug_espnow_rx_foreign_mac
    );
    Serial.printf(
        "INTERMEDIARIO -> PILOTO: bytes_UART=%lu | fallos_UART=%lu | cola=%u | descartados=%lu\n",
        (unsigned long)debug_uart_tx_bytes,
        (unsigned long)debug_uart_tx_fail,
        (unsigned int)queue_items,
        (unsigned long)espnow_rx_dropped
    );
    Serial.printf(
        "PTT BOX: %s | cierre_pendiente=%s | epoch=%lu | version=%lu | latidos_START=%lu\n",
        ptt_active ? "ACTIVO" : "INACTIVO",
        ptt_stop_pending ? "SI" : "NO",
        (unsigned long)ptt_epoch,
        (unsigned long)ptt_version,
        (unsigned long)debug_control_heartbeats
    );
    Serial.println("=========================================");
}

bool bridge_init()
{
    box_audio_queue = xQueueCreate(ESPNOW_RX_QUEUE_SIZE, sizeof(EspNowRxItem));
    if (box_audio_queue == nullptr) {
        Serial.println("ERROR creando cola ESP-NOW");
        return false;
    }

    PilotUart.setRxBufferSize(PILOT_UART_RX_BUFFER_SIZE);
    PilotUart.begin(
        PILOT_UART_BAUD,
        SERIAL_8N1,
        PILOT_UART_RX_GPIO,
        PILOT_UART_TX_GPIO
    );

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    delay(50);

    if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) {
        return false;
    }

    if (esp_wifi_set_protocol(
            WIFI_IF_STA,
            WIFI_PROTOCOL_11B |
            WIFI_PROTOCOL_11G |
            WIFI_PROTOCOL_11N |
            WIFI_PROTOCOL_LR) != ESP_OK) {
        return false;
    }

    if (esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        return false;
    }

    // El driver puede limitar el valor según chip, alimentación y normativa.
    esp_wifi_set_max_tx_power(ESPNOW_TX_POWER_QDBM);

    if (esp_now_init() != ESP_OK) {
        return false;
    }

    if (esp_now_register_send_cb(on_espnow_sent) != ESP_OK ||
        esp_now_register_recv_cb(on_espnow_recv) != ESP_OK) {
        return false;
    }

    if (!ensure_peer_registered(BOX_RECEIVER_MAC)) {
        return false;
    }

    int8_t tx_power = 0;
    esp_wifi_get_max_tx_power(&tx_power);

    Serial.println("Intermediario UART <-> ESP-NOW LR iniciado");
    Serial.print("MAC WiFi STA del intermediario: ");
    Serial.println(WiFi.macAddress());

    Serial.printf(
        "MAC fija del receptor BOX: %02X:%02X:%02X:%02X:%02X:%02X\n",
        BOX_RECEIVER_MAC[0],
        BOX_RECEIVER_MAC[1],
        BOX_RECEIVER_MAC[2],
        BOX_RECEIVER_MAC[3],
        BOX_RECEIVER_MAC[4],
        BOX_RECEIVER_MAC[5]
    );
    Serial.printf("UART: RX GPIO%d / TX GPIO%d / %d baudios\n",
                  PILOT_UART_RX_GPIO,
                  PILOT_UART_TX_GPIO,
                  PILOT_UART_BAUD);
    Serial.printf("Canal ESP-NOW: %d\n", ESPNOW_CHANNEL);
    Serial.print("Potencia configurada aprox.: ");
    Serial.print(tx_power * 0.25f);
    Serial.println(" dBm");

    return true;
}


void bridge_process()
{
    static bool box_first = true;

    // El control START/STOP siempre tiene prioridad sobre los paquetes de audio.
    send_pending_control_to_pilot();

    // Alternar la prioridad en cada pasada evita que un flujo continuo pueda
    // monopolizar el loop y cortar el audio del sentido opuesto.
    if (box_first) {
        forward_box_audio_to_pilot();
        process_pilot_uart_to_espnow();
    } else {
        process_pilot_uart_to_espnow();
        forward_box_audio_to_pilot();
    }
    box_first = !box_first;

    // Por si STOP llegó mientras se enviaba un paquete hacia boxes.
    send_pending_control_to_pilot();

    // Diagnóstico siempre activo, una vez por segundo.
    print_permanent_debug();
    delay(0);
}
