#include <Arduino.h>

// Evita que Arduino-ESP32 libere la memoria de Bluetooth Classic antes de setup().
#if __has_include("esp32-hal-alloc-bt-classic-mem.h")
#include "esp32-hal-alloc-bt-classic-mem.h"
#elif __has_include("esp32-hal-bt-mem.h")
#include "esp32-hal-bt-mem.h"
#else
#error "Actualiza esp32 by Espressif Systems a la version 3.3.8 o posterior."
#endif

#include "audio.hpp"
#include "config.hpp"
#include "ptt.hpp"
#include "uart_audio.hpp"

static uint8_t audio_frame_sequence = 0;
static TaskHandle_t pilot_tx_task_handle = nullptr;

static void pilot_tx_task(void *parameter)
{
    (void)parameter;
    bool microphone_enabled = false;

    for (;;) {
        const bool ptt_pressed = ptt_is_pressed();

        if (ptt_pressed != microphone_enabled) {
            microphone_enabled = ptt_pressed;
            audio_set_microphone_enabled(microphone_enabled);
        }

        if (!ptt_pressed) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const int8_t *audio = nullptr;
        uint16_t audio_len = 0;

        if (audio_capture_frame(&audio, &audio_len) && ptt_is_pressed()) {
            uart_send_audio(audio_frame_sequence++, audio, audio_len);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void stop_forever(const char *reason)
{
#if PILOT_DEBUG_SERIAL
    Serial.println(reason);
#endif
    while (true) {
        delay(1000);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

#if PILOT_DEBUG_SERIAL
    Serial.println();
    Serial.println("================================================");
    Serial.println(" Radio piloto: INTERCOM BLUETOOTH <-> UART");
    Serial.println("================================================");
#endif

    if (!ptt_init()) {
        stop_forever("FALLO: no se pudo iniciar el PTT KEY2");
    }

    if (!audio_init()) {
        stop_forever("FALLO: no se pudo iniciar Bluetooth HFP");
    }

    if (!uart_audio_init()) {
        stop_forever("FALLO: no se pudo iniciar el enlace UART");
    }

    const BaseType_t tx_task_result = xTaskCreatePinnedToCore(
        pilot_tx_task,
        "pilot_audio_tx",
        PILOT_TX_TASK_STACK,
        nullptr,
        PILOT_TX_TASK_PRIORITY,
        &pilot_tx_task_handle,
        PILOT_TX_TASK_CORE
    );

    if (tx_task_result != pdPASS) {
        stop_forever("FALLO: no se pudo iniciar la tarea de transmision del piloto");
    }

#if PILOT_DEBUG_SERIAL
    Serial.println("Radio lista. Enciende el V6 Pro+; se conectara automaticamente.");
    Serial.println("KEY2 pulsado = enviar micro; la escucha del box sigue activa.");
#endif
}

void loop()
{
    audio_process();
    uart_process_received_audio(true);
    uart_audio_debug_report();
    delay(1);
}
