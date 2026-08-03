#pragma once

#include <Arduino.h>

/* ============================================================
   AUDIO DE LA RADIO

   El protocolo de radio sigue usando audio mono de 8 bits a 8 kHz.
   El intercom negocia normalmente mSBC a 16 kHz; audio.cpp hace la
   conversión sencilla 16 kHz <-> 8 kHz sin aplicar filtros.
   ============================================================ */

#define AUDIO_SAMPLE_RATE             8000
#define AUDIO_BLOCK_MS                200
#define AUDIO_SAMPLES_PER_BLOCK       ((AUDIO_SAMPLE_RATE * AUDIO_BLOCK_MS) / 1000)
#define AUDIO_MAX_PAYLOAD             AUDIO_SAMPLES_PER_BLOCK

/* ============================================================
   INTERCOM BLUETOOTH HFP

   ESP32-A1S = Audio Gateway (como un teléfono)
   V6 Pro+   = manos libres HFP
   ============================================================ */

#define INTERCOM_BT_NAME              "FORMULA_GADES_HFP"
#define INTERCOM_BT_MAC               {0x10, 0xDC, 0xB6, 0x74, 0x2D, 0x23}
#define INTERCOM_SLC_RETRY_MS          5000
#define INTERCOM_AUDIO_RETRY_MS        3000
#define INTERCOM_CALL_DELAY_MS         1200
#define INTERCOM_AUDIO_OPEN_DELAY_MS   600
#define INTERCOM_MIC_BUFFER_BYTES      6400
#define INTERCOM_SPEAKER_BUFFER_BYTES  32768
#define INTERCOM_CVSD_TRIGGER_US       4000
#define INTERCOM_MSBC_TRIGGER_US       7500
#define INTERCOM_MIC_FRAME_TIMEOUT_MS  500

/* ============================================================
   PTT CON EL BOTON KEY2 DE LA ESP32-AUDIO-KIT

   Switch 1 (IO13-KEY2): ON
   Switch 2 (IO13-DATA3): OFF
   Switch 4 (IO13-MTCK): OFF
   ============================================================ */

#define PTT_BUTTON_GPIO               GPIO_NUM_13
#define PTT_BUTTON_ACTIVE_LEVEL       LOW
#define PTT_DEBOUNCE_MS               25
#define PTT_BUTTON_DEBUG              0

/* ============================================================
   UART CON EL ESP32-WROOM-32U INTERMEDIARIO

   Audio Kit GPIO23 (TX) -> WROOM-U GPIO16 (RX)
   Audio Kit GPIO22 (RX) <- WROOM-U GPIO17 (TX)
   GND Audio Kit         <-> GND WROOM-U
   ============================================================ */

#define PILOT_UART_PORT               2
#define PILOT_UART_BAUD               460800
#define PILOT_UART_RX_GPIO            22
#define PILOT_UART_TX_GPIO            23
#define PILOT_UART_RX_BUFFER_SIZE     8192
#define PILOT_UART_PACKET_QUEUE_SIZE  32
#define PILOT_UART_TASK_STACK         4096
#define PILOT_UART_TASK_PRIORITY      4
#define PILOT_UART_TASK_CORE          1
#define PILOT_UART_PARSER_TIMEOUT_MS  120
#define PILOT_UART_WRITE_TIMEOUT_MS   120

#define PILOT_TX_TASK_STACK           6144
#define PILOT_TX_TASK_PRIORITY        3
#define PILOT_TX_TASK_CORE            1

#define RADIO_AUDIO_CHUNK             200
#define RADIO_PACKET_HEADER_LEN       11
#define RADIO_PACKET_MAX_LEN          (RADIO_PACKET_HEADER_LEN + RADIO_AUDIO_CHUNK)
#define RADIO_REASSEMBLY_TIMEOUT_MS   600
#define BOX_AUDIO_AUTOSTART_GUARD_MS  150

#define RADIO_AUDIO_MAGIC_1           0xE5
#define RADIO_AUDIO_MAGIC_2           0x5E
#define RADIO_CONTROL_MAGIC_1         0xE5
#define RADIO_CONTROL_MAGIC_2         0x5F
#define RADIO_CONTROL_COMMAND_BOX_PTT 0x01
#define RADIO_CONTROL_PACKET_LEN      4

#define PILOT_DEBUG_SERIAL            1
