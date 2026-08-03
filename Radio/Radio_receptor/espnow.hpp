#ifndef ESPNOW_HPP
#define ESPNOW_HPP

#include <Arduino.h>

// Ponlo a 1 solo para depurar.
// Para audio real por UART es mejor dejarlo en 0 para no mezclar texto con datos binarios.
#define DEBUG_SERIAL 0

#define ESPNOW_AUDIO_CHUNK 200
#define ESPNOW_PACKET_HEADER_LEN 11
#define ESPNOW_PACKET_MAX_LEN (ESPNOW_PACKET_HEADER_LEN + ESPNOW_AUDIO_CHUNK)

#define RADIO_CONTROL_MAGIC_1          0xE5
#define RADIO_CONTROL_MAGIC_2          0x5F
#define RADIO_CONTROL_COMMAND_BOX_PTT  0x01
#define RADIO_CONTROL_PACKET_LEN       4

/* Reparto justo del loop entre PC -> piloto y piloto -> PC. */
#define MAX_ESPNOW_TO_PC_PACKETS_PER_PASS  2
#define MAX_PC_TO_ESPNOW_PACKETS_PER_PASS  2

// Formato del paquete:
// 0: magic1      0xE5
// 1: magic2      0x5E
// 2: frame_seq
// 3: chunk_index
// 4: chunk_total
// 5-6: frame_len   little endian
// 7-8: offset      little endian
// 9-10: chunk_len  little endian
// 11... audio signed 8-bit

typedef struct __attribute__((packed)) {
    uint8_t magic1;       // 0xE5
    uint8_t magic2;       // 0x5E

    uint8_t frame_seq;
    uint8_t chunk_index;
    uint8_t chunk_total;

    uint16_t frame_len;
    uint16_t offset;
    uint16_t chunk_len;

    int8_t audio[ESPNOW_AUDIO_CHUNK];
} AudioNowPacket;

bool init_espnow_puente_bidir();
void procesar_uart_y_enviar_espnow();
void procesar_espnow_y_enviar_pc();

#endif
