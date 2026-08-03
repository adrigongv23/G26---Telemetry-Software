#include "uart_audio.hpp"
#include "audio.hpp"
#include "config.hpp"

#include <HardwareSerial.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct __attribute__((packed)) AudioWirePacket {
    uint8_t magic1;
    uint8_t magic2;
    uint8_t frame_seq;
    uint8_t chunk_index;
    uint8_t chunk_total;
    uint16_t frame_len;
    uint16_t offset;
    uint16_t chunk_len;
    int8_t audio[RADIO_AUDIO_CHUNK];
};

struct UartRxItem {
    uint32_t epoch;
    uint16_t packet_len;
    uint8_t packet[RADIO_PACKET_MAX_LEN];
};

static constexpr size_t AUDIO_HEADER_SIZE = offsetof(AudioWirePacket, audio);
static constexpr uint8_t MAX_AUDIO_CHUNKS =
    (AUDIO_MAX_PAYLOAD + RADIO_AUDIO_CHUNK - 1) / RADIO_AUDIO_CHUNK;

static HardwareSerial PilotUart(PILOT_UART_PORT);
static QueueHandle_t rx_packet_queue = nullptr;
static TaskHandle_t rx_task_handle = nullptr;

static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool box_ptt_active = false;
static volatile bool box_stop_pending = false;
static volatile uint32_t box_stream_epoch = 0;
static volatile uint32_t box_control_version = 0;
static volatile uint32_t dropped_uart_packets = 0;
static volatile uint32_t box_last_stop_ms = 0;

static volatile uint32_t debug_control_start = 0;
static volatile uint32_t debug_control_stop = 0;
static volatile uint32_t debug_audio_packets = 0;
static volatile uint32_t debug_reassembled_frames = 0;
static volatile uint32_t debug_audio_autostarts = 0;
static volatile uint32_t debug_guard_drops = 0;

static int8_t reassembly_buffer[AUDIO_MAX_PAYLOAD];
static bool chunk_received[MAX_AUDIO_CHUNKS];
static bool reassembly_active = false;
static uint8_t reassembly_seq = 0;
static uint8_t reassembly_total_chunks = 0;
static uint8_t reassembly_received_chunks = 0;
static uint16_t reassembly_frame_len = 0;
static uint32_t reassembly_epoch = 0;
static uint32_t reassembly_started_ms = 0;

static uint16_t read_le_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool audio_packet_valid(const uint8_t *data, uint16_t len)
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

    if (chunk_total == 0 || chunk_total > MAX_AUDIO_CHUNKS) {
        return false;
    }

    if (chunk_index >= chunk_total) {
        return false;
    }

    if (frame_len == 0 || frame_len > AUDIO_MAX_PAYLOAD) {
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

static bool control_packet_valid(const uint8_t *data, uint16_t len)
{
    return data != nullptr &&
           len == RADIO_CONTROL_PACKET_LEN &&
           data[0] == RADIO_CONTROL_MAGIC_1 &&
           data[1] == RADIO_CONTROL_MAGIC_2 &&
           data[2] == RADIO_CONTROL_COMMAND_BOX_PTT &&
           (data[3] == 0 || data[3] == 1);
}

static void snapshot_control_state(
    bool *active,
    bool *stop_pending,
    uint32_t *epoch,
    uint32_t *version)
{
    portENTER_CRITICAL(&state_mux);
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
    portEXIT_CRITICAL(&state_mux);
}

static void handle_control_packet(const uint8_t *packet)
{
    const bool start_requested = packet[3] != 0;
    const uint32_t now = millis();
    bool started = false;
    bool stop_registered = false;

    portENTER_CRITICAL(&state_mux);

    if (start_requested) {
        // Los START periódicos son latidos. No cambian el epoch si el stream ya
        // está activo. También cancelan un cierre pendiente si vuelve a pulsarse.
        if (!box_ptt_active) {
            box_ptt_active = true;
            ++box_stream_epoch;
            ++box_control_version;
            started = true;
        }
        box_stop_pending = false;
    } else {
        box_last_stop_ms = now;
        if (box_ptt_active && !box_stop_pending) {
            // No cortar todavía: todos los paquetes anteriores al STOP ya están
            // en la cola UART y deben reconstruirse/reproducirse primero.
            box_stop_pending = true;
            stop_registered = true;
        }
    }

    portEXIT_CRITICAL(&state_mux);

    if (started) {
        ++debug_control_start;
        audio_resume_playback();
    }

    if (stop_registered) {
        ++debug_control_stop;
    }
}

static void enqueue_audio_packet(const uint8_t *packet, uint16_t len)
{
    if (rx_packet_queue == nullptr || !audio_packet_valid(packet, len)) {
        return;
    }

    bool active = false;
    bool stop_pending = false;
    uint32_t epoch = 0;
    snapshot_control_state(&active, &stop_pending, &epoch, nullptr);

    if (stop_pending) {
        ++debug_guard_drops;
        return;
    }

    if (!active) {
        const uint32_t now = millis();
        uint32_t last_stop = 0;

        portENTER_CRITICAL(&state_mux);
        last_stop = box_last_stop_ms;
        portEXIT_CRITICAL(&state_mux);

        // Si acaba de llegar STOP, estos pueden ser fragmentos atrasados y se
        // descartan. Pasado el margen, un paquete de audio válido puede recuperar
        // por sí solo un START perdido o un reinicio del Audio Kit.
        if ((uint32_t)(now - last_stop) < BOX_AUDIO_AUTOSTART_GUARD_MS) {
            ++debug_guard_drops;
            return;
        }

        portENTER_CRITICAL(&state_mux);
        if (!box_ptt_active) {
            box_ptt_active = true;
            box_stop_pending = false;
            ++box_stream_epoch;
            ++box_control_version;
            ++debug_audio_autostarts;
        }
        active = box_ptt_active;
        epoch = box_stream_epoch;
        portEXIT_CRITICAL(&state_mux);

        if (active) {
            audio_resume_playback();
        }
    }

    ++debug_audio_packets;

    UartRxItem item = {};
    item.epoch = epoch;
    item.packet_len = len;
    memcpy(item.packet, packet, len);

    if (xQueueSend(rx_packet_queue, &item, 0) != pdTRUE) {
        // Mantener audio reciente es preferible a acumular retraso. Si la cola
        // se llena, eliminamos el paquete más antiguo e insertamos el nuevo.
        UartRxItem discarded = {};
        xQueueReceive(rx_packet_queue, &discarded, 0);

        if (xQueueSend(rx_packet_queue, &item, 0) != pdTRUE) {
            ++dropped_uart_packets;
        }
    }
}

static void uart_rx_task(void *parameter)
{
    (void)parameter;

    enum ParserState {
        WAIT_MAGIC_1,
        WAIT_MAGIC_2,
        READ_AUDIO_HEADER,
        READ_AUDIO_BODY,
        READ_CONTROL
    };

    ParserState state = WAIT_MAGIC_1;
    uint8_t packet[RADIO_PACKET_MAX_LEN] = {};
    size_t index = 0;
    uint16_t expected_audio_len = 0;
    uint32_t last_byte_ms = millis();

    auto reset_parser = [&]() {
        state = WAIT_MAGIC_1;
        index = 0;
        expected_audio_len = 0;
    };

    for (;;) {
        bool read_anything = false;

        while (PilotUart.available() > 0) {
            const int value = PilotUart.read();
            if (value < 0) {
                break;
            }

            read_anything = true;
            const uint8_t b = (uint8_t)value;
            const uint32_t now = millis();

            if (state != WAIT_MAGIC_1 &&
                (uint32_t)(now - last_byte_ms) > PILOT_UART_PARSER_TIMEOUT_MS) {
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
                        packet[1] = b;
                        index = 2;
                        state = READ_CONTROL;
                    } else if (b == RADIO_AUDIO_MAGIC_1) {
                        packet[0] = b;
                        index = 1;
                    } else {
                        reset_parser();
                    }
                    break;

                case READ_CONTROL:
                    if (index >= sizeof(packet)) {
                        reset_parser();
                        break;
                    }

                    packet[index++] = b;
                    if (index >= RADIO_CONTROL_PACKET_LEN) {
                        if (control_packet_valid(packet, (uint16_t)index)) {
                            handle_control_packet(packet);
                        }
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

                        if (packet[4] == 0 || packet[4] > MAX_AUDIO_CHUNKS ||
                            packet[3] >= packet[4] ||
                            frame_len == 0 || frame_len > AUDIO_MAX_PAYLOAD ||
                            expected_audio_len == 0 || expected_audio_len > RADIO_AUDIO_CHUNK ||
                            (uint32_t)offset + expected_audio_len > frame_len) {
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
                        enqueue_audio_packet(packet, (uint16_t)index);
                        reset_parser();
                    }
                    break;
            }
        }

        if (!read_anything) {
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            taskYIELD();
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
        const size_t written = PilotUart.write(data + sent, len - sent);
        sent += written;

        if (sent >= len) {
            return true;
        }

        if ((uint32_t)(millis() - started) >= PILOT_UART_WRITE_TIMEOUT_MS) {
            return false;
        }

        delay(0);
    }

    return true;
}

static void reset_reassembly(uint8_t seq, uint8_t total_chunks, uint16_t frame_len, uint32_t epoch)
{
    reassembly_active = true;
    reassembly_seq = seq;
    reassembly_total_chunks = total_chunks;
    reassembly_received_chunks = 0;
    reassembly_frame_len = frame_len;
    reassembly_epoch = epoch;
    reassembly_started_ms = millis();
    memset(chunk_received, 0, sizeof(chunk_received));
}

bool uart_audio_init()
{
    rx_packet_queue = xQueueCreate(
        PILOT_UART_PACKET_QUEUE_SIZE,
        sizeof(UartRxItem)
    );

    if (rx_packet_queue == nullptr) {
#if PILOT_DEBUG_SERIAL
        Serial.println("ERROR: no se pudo crear la cola UART de audio");
#endif
        return false;
    }

    // Debe configurarse antes de begin(), según la API HardwareSerial del ESP32.
    PilotUart.setRxBufferSize(PILOT_UART_RX_BUFFER_SIZE);
    PilotUart.begin(
        PILOT_UART_BAUD,
        SERIAL_8N1,
        PILOT_UART_RX_GPIO,
        PILOT_UART_TX_GPIO
    );

    const BaseType_t task_result = xTaskCreatePinnedToCore(
        uart_rx_task,
        "pilot_uart_rx",
        PILOT_UART_TASK_STACK,
        nullptr,
        PILOT_UART_TASK_PRIORITY,
        &rx_task_handle,
        PILOT_UART_TASK_CORE
    );

    if (task_result != pdPASS) {
#if PILOT_DEBUG_SERIAL
        Serial.println("ERROR: no se pudo crear la tarea UART");
#endif
        rx_task_handle = nullptr;
        return false;
    }

#if PILOT_DEBUG_SERIAL
    Serial.println("UART con intermediario iniciada");
    Serial.printf("Baudios: %d | RX GPIO%d | TX GPIO%d\n",
                  PILOT_UART_BAUD,
                  PILOT_UART_RX_GPIO,
                  PILOT_UART_TX_GPIO);
#endif

    return true;
}

bool uart_send_audio(uint8_t frame_seq, const int8_t *audio_data, uint16_t audio_len)
{
    if (audio_data == nullptr || audio_len == 0 || audio_len > AUDIO_MAX_PAYLOAD) {
        return false;
    }

    const uint8_t total_chunks =
        (uint8_t)((audio_len + RADIO_AUDIO_CHUNK - 1) / RADIO_AUDIO_CHUNK);

    for (uint8_t chunk_index = 0; chunk_index < total_chunks; ++chunk_index) {
        const uint16_t offset = (uint16_t)chunk_index * RADIO_AUDIO_CHUNK;
        const uint16_t remaining = (uint16_t)(audio_len - offset);
        const uint16_t chunk_len = remaining > RADIO_AUDIO_CHUNK
            ? RADIO_AUDIO_CHUNK
            : remaining;

        AudioWirePacket packet = {};
        packet.magic1 = RADIO_AUDIO_MAGIC_1;
        packet.magic2 = RADIO_AUDIO_MAGIC_2;
        packet.frame_seq = frame_seq;
        packet.chunk_index = chunk_index;
        packet.chunk_total = total_chunks;
        packet.frame_len = audio_len;
        packet.offset = offset;
        packet.chunk_len = chunk_len;
        memcpy(packet.audio, audio_data + offset, chunk_len);

        const size_t packet_size = AUDIO_HEADER_SIZE + chunk_len;
        if (!uart_write_all(reinterpret_cast<const uint8_t *>(&packet), packet_size)) {
#if PILOT_DEBUG_SERIAL
            Serial.printf("ERROR UART enviando fragmento %u/%u\n",
                          (unsigned)chunk_index,
                          (unsigned)total_chunks);
#endif
            return false;
        }
    }

    return true;
}

void uart_discard_received_audio()
{
    if (rx_packet_queue != nullptr) {
        UartRxItem discarded = {};
        while (xQueueReceive(rx_packet_queue, &discarded, 0) == pdTRUE) {
        }
    }

    reassembly_active = false;
    reassembly_received_chunks = 0;
    memset(chunk_received, 0, sizeof(chunk_received));
}

static bool finalize_box_stop_if_drained()
{
    if (rx_packet_queue == nullptr || uxQueueMessagesWaiting(rx_packet_queue) != 0 || reassembly_active) {
        return false;
    }

    bool changed = false;

    portENTER_CRITICAL(&state_mux);
    if (box_stop_pending && box_ptt_active) {
        box_stop_pending = false;
        box_ptt_active = false;
        ++box_stream_epoch;
        ++box_control_version;
        changed = true;
    }
    portEXIT_CRITICAL(&state_mux);

    return changed;
}

bool uart_box_ptt_active()
{
    bool active = false;
    snapshot_control_state(&active, nullptr, nullptr, nullptr);
    return active;
}

void uart_process_received_audio(bool reproduce)
{
    static uint32_t processed_control_version = 0;

    bool active = false;
    uint32_t current_epoch = 0;
    uint32_t current_version = 0;
    snapshot_control_state(&active, nullptr, &current_epoch, &current_version);

    if (current_version != processed_control_version) {
        processed_control_version = current_version;
        reassembly_active = false;
        reassembly_received_chunks = 0;
        memset(chunk_received, 0, sizeof(chunk_received));

        if (!active) {
            uart_discard_received_audio();
            // El último frame ya se escribió al I2S. Se solicita el cierre sin
            // vaciarlo, y el silencio final se añade tras el timeout normal.
            audio_request_playback_stop();
        } else if (reproduce) {
            audio_resume_playback();
        }
    }

    if (!reproduce || !active) {
        uart_discard_received_audio();
        return;
    }

    if (reassembly_active &&
        (uint32_t)(millis() - reassembly_started_ms) > RADIO_REASSEMBLY_TIMEOUT_MS) {
        reassembly_active = false;
    }

    UartRxItem item = {};
    while (rx_packet_queue != nullptr &&
           xQueueReceive(rx_packet_queue, &item, 0) == pdTRUE) {

        snapshot_control_state(&active, nullptr, &current_epoch, nullptr);

        if (!active || item.epoch != current_epoch) {
            continue;
        }

        if (!audio_packet_valid(item.packet, item.packet_len)) {
            continue;
        }

        AudioWirePacket packet = {};
        memcpy(&packet, item.packet, item.packet_len);

        if (!reassembly_active ||
            reassembly_epoch != item.epoch ||
            packet.frame_seq != reassembly_seq ||
            packet.chunk_total != reassembly_total_chunks ||
            packet.frame_len != reassembly_frame_len) {
            reset_reassembly(
                packet.frame_seq,
                packet.chunk_total,
                packet.frame_len,
                item.epoch
            );
        }

        if (!chunk_received[packet.chunk_index]) {
            memcpy(
                reassembly_buffer + packet.offset,
                packet.audio,
                packet.chunk_len
            );
            chunk_received[packet.chunk_index] = true;
            ++reassembly_received_chunks;
        }

        if (reassembly_received_chunks >= reassembly_total_chunks) {
            ++debug_reassembled_frames;
            audio_play_frame_s8(reassembly_buffer, reassembly_frame_len);
            reassembly_active = false;
        }
    }

    // STOP se aplica únicamente cuando ya no queda ningún paquete ni un frame
    // incompleto. Así los auriculares reproducen el final completo del mensaje.
    if (finalize_box_stop_if_drained()) {
        audio_request_playback_stop();
    }
}

void uart_audio_debug_report()
{
#if PILOT_DEBUG_SERIAL
    static uint32_t last_report_ms = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - last_report_ms) < 1000) {
        return;
    }
    last_report_ms = now;

    bool active = false;
    bool stop_pending = false;
    uint32_t epoch = 0;
    uint32_t version = 0;
    snapshot_control_state(&active, &stop_pending, &epoch, &version);

    const UBaseType_t queued =
        rx_packet_queue != nullptr ? uxQueueMessagesWaiting(rx_packet_queue) : 0;

    Serial.printf(
        "PILOTO BOX->INTERCOM: PTT=%s cierre_pendiente=%s ctrlON=%lu ctrlOFF=%lu paquetes=%lu frames=%lu autoSTART=%lu guard=%lu cola=%u drop=%lu epoch=%lu version=%lu\n",
        active ? "ON" : "OFF",
        stop_pending ? "SI" : "NO",
        (unsigned long)debug_control_start,
        (unsigned long)debug_control_stop,
        (unsigned long)debug_audio_packets,
        (unsigned long)debug_reassembled_frames,
        (unsigned long)debug_audio_autostarts,
        (unsigned long)debug_guard_drops,
        (unsigned int)queued,
        (unsigned long)dropped_uart_packets,
        (unsigned long)epoch,
        (unsigned long)version
    );
#endif
}
