#include "audio.hpp"
#include "config.hpp"

#include <Arduino.h>
#include <string.h>

#include "esp32-hal-bt.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_ag_api.h"
#include "esp_timer.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Bluetooth HFP necesita un ESP32 clasico, como el ESP32-A1S."
#endif

#if !defined(CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI) || !(CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI)
#error "Instala esp32 by Espressif Systems 3.3.8 o posterior para usar HFP por HCI."
#endif

/* ============================================================
   ESTADO BLUETOOTH
   ============================================================ */

static esp_bd_addr_t intercom_addr = INTERCOM_BT_MAC;
static char test_number[] = "100";

static volatile bool hfp_ready = false;
static volatile bool slc_connected = false;
static volatile bool slc_connecting = false;
static volatile bool audio_connected = false;
static volatile bool audio_connecting = false;
static volatile bool call_active = false;
static volatile bool codec_msbc = false;

static uint32_t last_slc_attempt_ms = 0;
static uint32_t last_audio_attempt_ms = 0;
static uint32_t call_start_at_ms = 0;
static uint32_t audio_open_at_ms = 0;

/* ============================================================
   BUFFERS DE AUDIO

   mic_buffer:
     PCM del micro ya convertido a 8 bits / 8 kHz para la radio.

   speaker_buffer:
     PCM de 16 bits a la frecuencia negociada por HFP para el intercom.
   ============================================================ */

static StreamBufferHandle_t mic_buffer = nullptr;
static StreamBufferHandle_t speaker_buffer = nullptr;
static esp_timer_handle_t hfp_audio_timer = nullptr;

static volatile bool microphone_enabled = false;
static volatile bool playback_enabled = false;
static volatile bool speaker_clear_requested = true;
static volatile bool downsample_phase = false;

static int8_t mic_frame[AUDIO_SAMPLES_PER_BLOCK];

/* ============================================================
   UTILIDADES
   ============================================================ */

static void print_esp_result(const char *name, esp_err_t err)
{
#if PILOT_DEBUG_SERIAL
    if (err == ESP_OK) {
        Serial.printf("%s: OK\n", name);
    } else {
        Serial.printf("%s: %s (0x%04X)\n", name, esp_err_to_name(err), (unsigned)err);
    }
#else
    (void)name;
    (void)err;
#endif
}

static void print_intercom_mac()
{
#if PILOT_DEBUG_SERIAL
    Serial.printf(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        intercom_addr[0], intercom_addr[1], intercom_addr[2],
        intercom_addr[3], intercom_addr[4], intercom_addr[5]
    );
#endif
}

static void drain_microphone_buffer()
{
    if (mic_buffer == nullptr) {
        return;
    }

    uint8_t discarded[128];
    while (xStreamBufferReceive(mic_buffer, discarded, sizeof(discarded), 0) > 0) {
    }
}

static void drain_speaker_buffer_from_reader(uint8_t *temporary, size_t temporary_len)
{
    if (speaker_buffer == nullptr || temporary == nullptr || temporary_len == 0) {
        return;
    }

    while (xStreamBufferReceive(speaker_buffer, temporary, temporary_len, 0) > 0) {
    }
}

/* ============================================================
   PIPELINE HFP
   ============================================================ */

static void audio_timer_callback(void *arg)
{
    (void)arg;
    if (audio_connected) {
        esp_hf_ag_outgoing_data_ready();
    }
}

static void stop_hfp_audio_timer()
{
    if (hfp_audio_timer != nullptr) {
        esp_timer_stop(hfp_audio_timer);
        esp_timer_delete(hfp_audio_timer);
        hfp_audio_timer = nullptr;
    }
}

static bool start_hfp_audio_timer()
{
    stop_hfp_audio_timer();

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = audio_timer_callback;
    timer_args.arg = nullptr;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "hfp_audio";
    timer_args.skip_unhandled_events = true;

    esp_err_t err = esp_timer_create(&timer_args, &hfp_audio_timer);
    if (err != ESP_OK) {
        print_esp_result("esp_timer_create", err);
        return false;
    }

    err = esp_timer_start_periodic(
        hfp_audio_timer,
        codec_msbc ? INTERCOM_MSBC_TRIGGER_US : INTERCOM_CVSD_TRIGGER_US
    );

    if (err != ESP_OK) {
        print_esp_result("esp_timer_start_periodic", err);
        stop_hfp_audio_timer();
        return false;
    }

    return true;
}

/*
   HFP entrega PCM mono de 16 bits.
   - CVSD: 8 kHz  -> se usa cada muestra.
   - mSBC: 16 kHz -> se toma una de cada dos muestras.

   No se aplica paso alto, paso bajo, puerta de ruido ni ganancia digital.
*/
static void incoming_audio_callback(const uint8_t *buf, uint32_t len)
{
    if (buf == nullptr || len < sizeof(int16_t) || !microphone_enabled || mic_buffer == nullptr) {
        return;
    }

    const int16_t *samples = reinterpret_cast<const int16_t *>(buf);
    const size_t sample_count = len / sizeof(int16_t);
    int8_t converted[128];
    size_t converted_count = 0;

    for (size_t i = 0; i < sample_count; ++i) {
        bool keep = true;

        if (codec_msbc) {
            keep = !downsample_phase;
            downsample_phase = !downsample_phase;
        }

        if (!keep) {
            continue;
        }

        converted[converted_count++] = (int8_t)(samples[i] / 256);

        if (converted_count == sizeof(converted)) {
            xStreamBufferSend(mic_buffer, converted, converted_count, 0);
            converted_count = 0;
        }
    }

    if (converted_count > 0) {
        xStreamBufferSend(mic_buffer, converted, converted_count, 0);
    }
}

/*
   El intercom solicita PCM mono de 16 bits.
   Si no hay audio del box disponible, se devuelve silencio manteniendo
   abierto el enlace HFP.
*/
static uint32_t outgoing_audio_callback(uint8_t *buf, uint32_t len)
{
    if (buf == nullptr || len == 0 || !audio_connected) {
        return 0;
    }

    if (speaker_clear_requested) {
        drain_speaker_buffer_from_reader(buf, len);
        speaker_clear_requested = false;
    }

    if (!playback_enabled || speaker_buffer == nullptr) {
        memset(buf, 0, len);
        return len;
    }

    const size_t received = xStreamBufferReceive(speaker_buffer, buf, len, 0);
    if (received < len) {
        memset(buf + received, 0, len - received);
    }

    return len;
}

/* ============================================================
   CONEXION HFP
   ============================================================ */

static void request_slc_connect(bool force)
{
    if (!hfp_ready || slc_connected || slc_connecting) {
        return;
    }

    const uint32_t now = millis();
    if (!force && (uint32_t)(now - last_slc_attempt_ms) < INTERCOM_SLC_RETRY_MS) {
        return;
    }

    last_slc_attempt_ms = now;
    slc_connecting = true;

#if PILOT_DEBUG_SERIAL
    Serial.print("Conectando intercom HFP: ");
    print_intercom_mac();
    Serial.println();
#endif

    const esp_err_t err = esp_hf_ag_slc_connect(intercom_addr);
    if (err != ESP_OK) {
        slc_connecting = false;
    }
    print_esp_result("esp_hf_ag_slc_connect", err);
}

static void start_fake_call()
{
    if (!slc_connected || call_active) {
        return;
    }

    call_active = true;
    const esp_err_t err = esp_hf_ag_out_call(
        intercom_addr,
        1,
        0,
        ESP_HF_CALL_STATUS_CALL_IN_PROGRESS,
        ESP_HF_CALL_SETUP_STATUS_IDLE,
        test_number,
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN
    );

    print_esp_result("esp_hf_ag_out_call", err);
    audio_open_at_ms = millis() + INTERCOM_AUDIO_OPEN_DELAY_MS;
}

static void request_audio_connect(bool force)
{
    if (!slc_connected || audio_connected) {
        return;
    }

    const uint32_t now = millis();
    if (!force && audio_connecting &&
        (uint32_t)(now - last_audio_attempt_ms) < INTERCOM_AUDIO_RETRY_MS) {
        return;
    }

    last_audio_attempt_ms = now;
    audio_connecting = true;

    const esp_err_t err = esp_hf_ag_audio_connect(intercom_addr);
    if (err != ESP_OK) {
        audio_connecting = false;
    }
    print_esp_result("esp_hf_ag_audio_connect", err);
}

/* ============================================================
   RESPUESTAS HFP BASICAS
   ============================================================ */

static void send_indicator_state(esp_bd_addr_t addr)
{
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALL,
        call_active ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS : ESP_HF_CALL_STATUS_NO_CALLS);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALLSETUP, ESP_HF_CALL_SETUP_STATUS_IDLE);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_SERVICE, ESP_HF_NETWORK_STATE_AVAILABLE);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_SIGNAL, 5);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_ROAM, ESP_HF_ROAMING_STATUS_INACTIVE);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_BATTCHG, 5);
    esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALLHELD, ESP_HF_CALL_HELD_STATUS_NONE);
}

static void answer_current_call(esp_bd_addr_t addr)
{
    call_active = true;
    print_esp_result(
        "esp_hf_ag_answer_call",
        esp_hf_ag_answer_call(
            addr,
            1,
            0,
            ESP_HF_CALL_STATUS_CALL_IN_PROGRESS,
            ESP_HF_CALL_SETUP_STATUS_IDLE,
            test_number,
            ESP_HF_CALL_ADDR_TYPE_UNKNOWN
        )
    );
    audio_open_at_ms = millis() + INTERCOM_AUDIO_OPEN_DELAY_MS;
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
#if PILOT_DEBUG_SERIAL
            Serial.println(param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS
                ? "Intercom emparejado correctamente"
                : "ERROR emparejando el intercom");
#endif
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_PIN_REQ_EVT: {
            esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
            break;
        }

        default:
            break;
    }
}

static void hfp_callback(esp_hf_cb_event_t event, esp_hf_cb_param_t *param)
{
    switch (event) {
        case ESP_HF_PROF_STATE_EVT:
            hfp_ready = (param->prof_stat.state == ESP_HF_INIT_SUCCESS ||
                         param->prof_stat.state == ESP_HF_INIT_ALREADY);
            if (hfp_ready) {
                request_slc_connect(true);
            }
            break;

        case ESP_HF_CONNECTION_STATE_EVT: {
            const esp_hf_connection_state_t state = param->conn_stat.state;
            slc_connected = (state == ESP_HF_CONNECTION_STATE_SLC_CONNECTED);
            slc_connecting = (state == ESP_HF_CONNECTION_STATE_CONNECTING ||
                              state == ESP_HF_CONNECTION_STATE_CONNECTED);

            if (state == ESP_HF_CONNECTION_STATE_DISCONNECTED) {
                slc_connected = false;
                slc_connecting = false;
                audio_connected = false;
                audio_connecting = false;
                call_active = false;
                call_start_at_ms = 0;
                audio_open_at_ms = 0;
                microphone_enabled = false;
                speaker_clear_requested = true;
                stop_hfp_audio_timer();
#if PILOT_DEBUG_SERIAL
                Serial.println("Intercom desconectado. Se reintentara automaticamente.");
#endif
            } else if (slc_connected) {
                slc_connecting = false;
#if PILOT_DEBUG_SERIAL
                Serial.println("Intercom HFP conectado");
#endif
                esp_hf_ag_volume_control(intercom_addr, ESP_HF_VOLUME_CONTROL_TARGET_SPK, 12);
                esp_hf_ag_volume_control(intercom_addr, ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15);
                call_start_at_ms = millis() + INTERCOM_CALL_DELAY_MS;
            }
            break;
        }

        case ESP_HF_AUDIO_STATE_EVT: {
            const esp_hf_audio_state_t state = param->audio_stat.state;
            audio_connecting = (state == ESP_HF_AUDIO_STATE_CONNECTING);
            audio_connected = (state == ESP_HF_AUDIO_STATE_CONNECTED ||
                               state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC);
            codec_msbc = (state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC);

            if (audio_connected) {
                audio_connecting = false;
                downsample_phase = false;
                speaker_clear_requested = true;
                esp_hf_ag_register_data_callback(incoming_audio_callback, outgoing_audio_callback);
                start_hfp_audio_timer();
#if PILOT_DEBUG_SERIAL
                Serial.printf("Audio del intercom conectado: %s\n",
                    codec_msbc ? "mSBC 16 kHz" : "CVSD 8 kHz");
#endif
            } else if (state == ESP_HF_AUDIO_STATE_DISCONNECTED) {
                audio_connecting = false;
                stop_hfp_audio_timer();
#if PILOT_DEBUG_SERIAL
                Serial.println("Audio del intercom desconectado");
#endif
            }
            break;
        }

        case ESP_HF_IND_UPDATE_EVT:
            send_indicator_state(param->ind_upd.remote_addr);
            break;

        case ESP_HF_CIND_RESPONSE_EVT:
            esp_hf_ag_cind_response(
                param->cind_rep.remote_addr,
                call_active ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS : ESP_HF_CALL_STATUS_NO_CALLS,
                ESP_HF_CALL_SETUP_STATUS_IDLE,
                ESP_HF_NETWORK_STATE_AVAILABLE,
                5,
                ESP_HF_ROAMING_STATUS_INACTIVE,
                5,
                ESP_HF_CALL_HELD_STATUS_NONE
            );
            break;

        case ESP_HF_COPS_RESPONSE_EVT: {
            static char operator_name[] = "FormulaGades";
            esp_hf_ag_cops_response(param->cops_rep.remote_addr, operator_name);
            break;
        }

        case ESP_HF_CLCC_RESPONSE_EVT:
            if (call_active) {
                esp_hf_ag_clcc_response(
                    param->clcc_rep.remote_addr,
                    1,
                    ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
                    ESP_HF_CURRENT_CALL_STATUS_ACTIVE,
                    ESP_HF_CURRENT_CALL_MODE_VOICE,
                    ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE,
                    test_number,
                    ESP_HF_CALL_ADDR_TYPE_UNKNOWN
                );
            }
            esp_hf_ag_clcc_response(
                param->clcc_rep.remote_addr,
                0,
                ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
                ESP_HF_CURRENT_CALL_STATUS_ACTIVE,
                ESP_HF_CURRENT_CALL_MODE_VOICE,
                ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE,
                test_number,
                ESP_HF_CALL_ADDR_TYPE_UNKNOWN
            );
            break;

        case ESP_HF_CNUM_RESPONSE_EVT:
            esp_hf_ag_cnum_response(
                param->cnum_rep.remote_addr,
                test_number,
                129,
                ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE
            );
            break;

        case ESP_HF_ATA_RESPONSE_EVT:
            answer_current_call(param->ata_rep.remote_addr);
            break;

        case ESP_HF_CHUP_RESPONSE_EVT:
            call_active = false;
            audio_connected = false;
            audio_connecting = false;
            stop_hfp_audio_timer();
            break;

        case ESP_HF_DIAL_EVT:
            esp_hf_ag_cmee_send(
                param->out_call.remote_addr,
                ESP_HF_AT_RESPONSE_CODE_OK,
                ESP_HF_CME_AG_FAILURE
            );
            answer_current_call(param->out_call.remote_addr);
            break;

        case ESP_HF_UNAT_RESPONSE_EVT: {
            // El V6 Pro+ envia +SUGCODEC=1. Hay que responder OK para completar SLC.
            static char ok_response[] = "OK";
            esp_hf_ag_unknown_at_send(param->unat_rep.remote_addr, ok_response);
            break;
        }

        default:
            break;
    }
}

static bool init_bluetooth()
{
    if (mic_buffer == nullptr) {
        mic_buffer = xStreamBufferCreate(INTERCOM_MIC_BUFFER_BYTES, 1);
    }
    if (speaker_buffer == nullptr) {
        speaker_buffer = xStreamBufferCreate(INTERCOM_SPEAKER_BUFFER_BYTES, 1);
    }

    if (mic_buffer == nullptr || speaker_buffer == nullptr) {
#if PILOT_DEBUG_SERIAL
        Serial.println("ERROR creando buffers de audio Bluetooth");
#endif
        return false;
    }

    if (!btStarted() && !btStartMode(BT_MODE_CLASSIC_BT)) {
#if PILOT_DEBUG_SERIAL
        Serial.println("ERROR iniciando Bluetooth Classic");
#endif
        return false;
    }

    if (esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9) != ESP_OK) {
#if PILOT_DEBUG_SERIAL
        Serial.println("ERROR configurando Bluetooth Classic a maxima potencia");
#endif
        return false;
    }

    esp_bluedroid_status_t status = esp_bluedroid_get_status();
    if (status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        if (esp_bluedroid_init() != ESP_OK) {
            return false;
        }
    }

    status = esp_bluedroid_get_status();
    if (status != ESP_BLUEDROID_STATUS_ENABLED) {
        if (esp_bluedroid_enable() != ESP_OK) {
            return false;
        }
    }

    if (esp_bt_gap_register_callback(gap_callback) != ESP_OK) {
        return false;
    }

    esp_bt_io_cap_t io_capability = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &io_capability, sizeof(io_capability));

    esp_bt_pin_code_t unused_pin = {0};
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, unused_pin);
    esp_bt_dev_set_device_name(INTERCOM_BT_NAME);

    esp_bt_cod_t cod = {};
    cod.major = ESP_BT_COD_MAJOR_DEV_PHONE;
    cod.minor = 0x00;
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    if (esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI) != ESP_OK) {
        return false;
    }
    if (esp_hf_ag_register_callback(hfp_callback) != ESP_OK) {
        return false;
    }
    if (esp_hf_ag_register_data_callback(incoming_audio_callback, outgoing_audio_callback) != ESP_OK) {
        return false;
    }
    if (esp_hf_ag_init() != ESP_OK) {
        return false;
    }

    return true;
}

/* ============================================================
   API USADA POR RADIO_PILOTO
   ============================================================ */

bool audio_init()
{
    playback_enabled = false;
    speaker_clear_requested = true;
    microphone_enabled = false;

#if PILOT_DEBUG_SERIAL
    Serial.print("Intercom configurado: ");
    print_intercom_mac();
    Serial.println();
#endif

    return init_bluetooth();
}

void audio_process()
{
    const uint32_t now = millis();

    if (hfp_ready && !slc_connected) {
        request_slc_connect(false);
    }

    if (slc_connected && call_start_at_ms != 0 &&
        (int32_t)(now - call_start_at_ms) >= 0) {
        call_start_at_ms = 0;
        start_fake_call();
    }

    if (slc_connected && call_active && audio_open_at_ms != 0 &&
        (int32_t)(now - audio_open_at_ms) >= 0) {
        audio_open_at_ms = 0;
        request_audio_connect(true);
    }

    if (slc_connected && call_active && !audio_connected &&
        (uint32_t)(now - last_audio_attempt_ms) >= INTERCOM_AUDIO_RETRY_MS) {
        request_audio_connect(false);
    }
}

void audio_set_microphone_enabled(bool enabled)
{
    microphone_enabled = false;
    drain_microphone_buffer();
    downsample_phase = false;
    microphone_enabled = enabled;
}

bool audio_capture_frame(const int8_t **out_audio, uint16_t *out_len)
{
    if (out_audio == nullptr || out_len == nullptr || mic_buffer == nullptr ||
        !microphone_enabled || !audio_connected) {
        return false;
    }

    size_t total = 0;
    const uint32_t started = millis();

    while (total < AUDIO_SAMPLES_PER_BLOCK) {
        if (!microphone_enabled || !audio_connected) {
            return false;
        }

        const size_t received = xStreamBufferReceive(
            mic_buffer,
            mic_frame + total,
            AUDIO_SAMPLES_PER_BLOCK - total,
            pdMS_TO_TICKS(20)
        );

        total += received;

        if ((uint32_t)(millis() - started) >= INTERCOM_MIC_FRAME_TIMEOUT_MS) {
            return false;
        }
    }

    *out_audio = mic_frame;
    *out_len = AUDIO_SAMPLES_PER_BLOCK;
    return true;
}

void audio_play_frame_s8(const int8_t *audio, uint16_t len)
{
    if (audio == nullptr || len == 0 || speaker_buffer == nullptr ||
        !playback_enabled || !audio_connected) {
        return;
    }

    if (len > AUDIO_MAX_PAYLOAD) {
        len = AUDIO_MAX_PAYLOAD;
    }

    const size_t required_bytes = (size_t)len * sizeof(int16_t) * (codec_msbc ? 2U : 1U);
    if (xStreamBufferSpacesAvailable(speaker_buffer) < required_bytes) {
        return;
    }

    constexpr uint16_t INPUT_CHUNK = 160;
    int16_t converted[INPUT_CHUNK * 2];

    uint16_t position = 0;
    while (position < len) {
        uint16_t chunk = (uint16_t)(len - position);
        if (chunk > INPUT_CHUNK) {
            chunk = INPUT_CHUNK;
        }

        size_t output_samples = 0;
        for (uint16_t i = 0; i < chunk; ++i) {
            const int16_t sample = (int16_t)((int32_t)audio[position + i] * 256);
            converted[output_samples++] = sample;

            if (codec_msbc) {
                converted[output_samples++] = sample;
            }
        }

        const size_t bytes = output_samples * sizeof(int16_t);
        const size_t written = xStreamBufferSend(
            speaker_buffer,
            reinterpret_cast<const uint8_t *>(converted),
            bytes,
            0
        );

        if (written != bytes) {
            return;
        }

        position = (uint16_t)(position + chunk);
    }
}

void audio_request_playback_stop()
{
    playback_enabled = false;
    speaker_clear_requested = true;
}

void audio_resume_playback()
{
    playback_enabled = true;
}

void audio_cut_playback()
{
    playback_enabled = false;
    speaker_clear_requested = true;
}
