#pragma once

#include <Arduino.h>

/* UART2 entre WROOM-U y Audio Kit */
#define PILOT_UART_BAUD               460800
#define PILOT_UART_RX_GPIO            16
#define PILOT_UART_TX_GPIO            17
#define PILOT_UART_RX_BUFFER_SIZE     8192
#define PILOT_UART_WRITE_TIMEOUT_MS   120
#define BOX_PTT_HEARTBEAT_MS          250

/* Protocolo de audio/control */
#define RADIO_AUDIO_CHUNK             200
#define RADIO_PACKET_HEADER_LEN       11
#define RADIO_PACKET_MAX_LEN          (RADIO_PACKET_HEADER_LEN + RADIO_AUDIO_CHUNK)
#define RADIO_AUDIO_MAGIC_1           0xE5
#define RADIO_AUDIO_MAGIC_2           0x5E
#define RADIO_CONTROL_MAGIC_1         0xE5
#define RADIO_CONTROL_MAGIC_2         0x5F
#define RADIO_CONTROL_COMMAND_BOX_PTT 0x01
#define RADIO_CONTROL_PACKET_LEN      4

/* ESP-NOW Long Range */
#define ESPNOW_CHANNEL                1
#define ESPNOW_TX_POWER_QDBM          84
#define ESPNOW_SEND_RETRIES           3
#define ESPNOW_SEND_TIMEOUT_MS        35
#define ESPNOW_RX_QUEUE_SIZE          40

/* Reparto justo del tiempo entre los dos sentidos de audio. */
#define MAX_BOX_TO_PILOT_PACKETS_PER_PASS  2
#define MAX_PILOT_TO_BOX_PACKETS_PER_PASS  2

/*
   MAC WiFi STA del ESP32 receptor conectado al PC.
   Esta es la MAC que ya figuraba en el proyecto anterior. Cámbiala si la
   MAC real de tu receptor de boxes es distinta.
*/
static constexpr uint8_t BOX_RECEIVER_MAC[6] = {
    0x28, 0x05, 0xA5, 0xE1, 0xCA, 0x10
};

#define INTERMEDIARY_DEBUG_SERIAL       1
#define INTERMEDIARY_DEBUG_INTERVAL_MS  1000
