/*
  Prueba EJEAS V6 Pro+ <-> ESP32-Audio-Kit ESP32-A1S
  ===================================================

  Objetivo:
    - El ESP32 actua como HFP Audio Gateway (como si fuera un telefono).
    - El EJEAS actua como manos libres HFP.
    - Busca automaticamente nombres que contengan EJEAS, V4 o V6. Tambien usa la MAC fija detectada 00:12:6F:60:F2:98.
    - Empareja mediante SSP o PIN 0000.
    - Simula una llamada activa para abrir el audio SCO.
    - Permite probar:
        * Tono: ESP32 -> altavoces del intercom.
        * Loopback: microfono del intercom -> ESP32 -> altavoces del intercom.

  Requisitos:
    - ESP32 clasico (ESP32-A1S sirve; ESP32-S3/C3 no sirven para BT Classic).
    - Arduino-ESP32 de Espressif 3.3.8 o posterior.
    - Placa recomendada en Arduino IDE: "ESP32 Dev Module".
    - Monitor serie: 115200 baudios, final de linea "Nueva linea".

  Antes de encender la placa:
    1. Apaga el Bluetooth del movil que suele usar el EJEAS.
    2. Pon el EJEAS V4 Plus en modo de emparejamiento CON TELEFONO.
    3. Enciende/reinicia el ESP32.

  Esta prueba NO utiliza el codec ES8388, los jacks ni UART2.
*/

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

// Arduino-ESP32 3.3.7+ libera antes de setup() la memoria de Bluetooth
// que no detecta como utilizada. Como aqui usamos la API HFP de ESP-IDF
// directamente, debemos marcar expresamente que Bluetooth Classic se usa.
#include "esp32-hal-alloc-bt-classic-mem.h"
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

// -----------------------------------------------------------------------------
// Comprobaciones de compilacion
// -----------------------------------------------------------------------------
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Este programa necesita un ESP32 clasico con Bluetooth Classic (por ejemplo ESP32-A1S)."
#endif

#if !defined(CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI) || !(CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI)
#error "El core instalado no tiene HFP Voice-over-HCI activado. Instala esp32 by Espressif Systems 3.3.8 o posterior."
#endif

// -----------------------------------------------------------------------------
// Ajustes de usuario
// -----------------------------------------------------------------------------
static constexpr char LOCAL_BT_NAME[] = "FORMULA_GADES_HFP";

// Se conecta al primer nombre que contenga una de estas cadenas.
static constexpr const char *TARGET_WORDS[] = {
  "ejeas",
  "v4 plus",
  "v4plus",
  "v4",
  "v6 pro+",
  "v6 pro",
  "v6"
};

// Si el escaneo por nombre no encuentra el intercom, puedes poner su MAC aqui.
// Ejemplo: {0xC8, 0x7F, 0x54, 0x12, 0x34, 0x56}
static constexpr bool USE_FIXED_MAC = true;
static const uint8_t FIXED_MAC[ESP_BD_ADDR_LEN] = {0x00, 0x12, 0x6F, 0x60, 0xF2, 0x98};

// Al conseguir SLC, simula una llamada y abre SCO automaticamente.
static constexpr bool AUTO_START_AUDIO = true;

// Audio inicial. LOOPBACK permite comprobar microfono y altavoz a la vez.
enum class AudioTestMode : uint8_t {
  LOOPBACK,
  TONE
};
static AudioTestMode audioMode = AudioTestMode::LOOPBACK;

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t DISCOVERY_RETRY_MS = 3000;
static constexpr uint32_t SLC_RETRY_MS = 5000;
static constexpr uint32_t AUDIO_RETRY_MS = 3000;
static constexpr uint32_t AUTO_CALL_DELAY_MS = 1200;
static constexpr uint32_t AUDIO_OPEN_DELAY_MS = 600;
static constexpr uint8_t DISCOVERY_LENGTH = 10;  // 10 x 1,28 s aproximadamente
static constexpr size_t AUDIO_RING_BYTES = 8192;
static constexpr uint32_t CVSD_TRIGGER_US = 4000;
static constexpr uint32_t MSBC_TRIGGER_US = 7500;
static constexpr float TEST_TONE_HZ = 1000.0f;
static constexpr int16_t TEST_TONE_AMPLITUDE = 7000;
static char TEST_NUMBER[] = "100";

// -----------------------------------------------------------------------------
// Estado global
// -----------------------------------------------------------------------------
Preferences prefs;
static const char PREF_NAMESPACE[] = "ejeas_hfp";
static const char PREF_BDA_KEY[] = "peer_bda";

static esp_bd_addr_t remoteBda = {0};
static bool haveRemoteBda = false;
static bool saveRemotePending = false;
static bool hfpReady = false;
static bool discoveryRunning = false;
static bool connectPending = false;
static bool slcConnected = false;
static bool slcInProgress = false;
static bool audioConnecting = false;
static bool audioConnected = false;
static bool callActive = false;
static bool codecMsbc = false;

static uint32_t lastDiscoveryAttemptMs = 0;
static uint32_t lastSlcAttemptMs = 0;
static uint32_t lastAudioAttemptMs = 0;
static uint32_t autoCallAtMs = 0;
static uint32_t audioOpenAtMs = 0;
static uint32_t lastDiagnosticMs = 0;

static StreamBufferHandle_t audioRing = nullptr;
static esp_timer_handle_t audioTimer = nullptr;
static float tonePhase = 0.0f;

static volatile uint32_t rxFrames = 0;
static volatile uint32_t rxBytes = 0;
static volatile uint32_t rxDrops = 0;
static volatile uint32_t txFrames = 0;
static volatile uint32_t txBytes = 0;
static volatile uint32_t txUnderruns = 0;
static volatile uint32_t lastRequestedBytes = 0;

// -----------------------------------------------------------------------------
// Declaraciones
// -----------------------------------------------------------------------------
static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void hfpCallback(esp_hf_cb_event_t event, esp_hf_cb_param_t *param);
static void incomingAudioCallback(const uint8_t *buf, uint32_t len);
static uint32_t outgoingAudioCallback(uint8_t *buf, uint32_t len);

// -----------------------------------------------------------------------------
// Utilidades
// -----------------------------------------------------------------------------
static void logEsp(const char *name, esp_err_t err) {
  if (err == ESP_OK) {
    Serial.printf("%s: OK\n", name);
  } else {
    Serial.printf("%s: %s (0x%04X)\n", name, esp_err_to_name(err), static_cast<unsigned>(err));
  }
}

static void printBda(const uint8_t *bda) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

static void setRemoteBda(const uint8_t *bda, bool saveLater) {
  memcpy(remoteBda, bda, ESP_BD_ADDR_LEN);
  haveRemoteBda = true;
  if (saveLater) {
    saveRemotePending = true;
  }
}

static bool loadSavedBda() {
  prefs.begin(PREF_NAMESPACE, false);
  if (prefs.getBytesLength(PREF_BDA_KEY) != ESP_BD_ADDR_LEN) {
    return false;
  }

  if (prefs.getBytes(PREF_BDA_KEY, remoteBda, ESP_BD_ADDR_LEN) != ESP_BD_ADDR_LEN) {
    return false;
  }

  haveRemoteBda = true;
  Serial.print("MAC EJEAS guardada: ");
  printBda(remoteBda);
  Serial.println();
  return true;
}

static void saveBdaIfNeeded() {
  if (!saveRemotePending || !haveRemoteBda) {
    return;
  }

  saveRemotePending = false;
  const size_t written = prefs.putBytes(PREF_BDA_KEY, remoteBda, ESP_BD_ADDR_LEN);
  Serial.printf("MAC guardada en NVS: %s\n", written == ESP_BD_ADDR_LEN ? "SI" : "NO");
}

static bool containsIgnoreCase(const char *text, const char *word) {
  if (!text || !word || !*word) {
    return false;
  }

  String haystack(text);
  String needle(word);
  haystack.toLowerCase();
  needle.toLowerCase();
  return haystack.indexOf(needle) >= 0;
}

static bool isTargetName(const char *name) {
  if (!name || !*name) {
    return false;
  }

  for (const char *word : TARGET_WORDS) {
    if (containsIgnoreCase(name, word)) {
      return true;
    }
  }
  return false;
}

static bool getNameFromEir(uint8_t *eir, char *name, size_t nameCapacity) {
  if (!eir || !name || nameCapacity < 2) {
    return false;
  }

  uint8_t nameLen = 0;
  uint8_t *nameData = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &nameLen);
  if (!nameData) {
    nameData = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &nameLen);
  }
  if (!nameData || nameLen == 0) {
    return false;
  }

  const size_t copyLen = min(static_cast<size_t>(nameLen), nameCapacity - 1);
  memcpy(name, nameData, copyLen);
  name[copyLen] = '\0';
  return true;
}

static const char *connectionStateName(esp_hf_connection_state_t state) {
  switch (state) {
    case ESP_HF_CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
    case ESP_HF_CONNECTION_STATE_CONNECTING: return "CONNECTING";
    case ESP_HF_CONNECTION_STATE_CONNECTED: return "RFCOMM_CONNECTED";
    case ESP_HF_CONNECTION_STATE_SLC_CONNECTED: return "SLC_CONNECTED";
    case ESP_HF_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
    default: return "UNKNOWN";
  }
}

static const char *audioStateName(esp_hf_audio_state_t state) {
  switch (state) {
    case ESP_HF_AUDIO_STATE_DISCONNECTED: return "DISCONNECTED";
    case ESP_HF_AUDIO_STATE_CONNECTING: return "CONNECTING";
    case ESP_HF_AUDIO_STATE_CONNECTED: return "CONNECTED_CVSD_8_KHZ";
    case ESP_HF_AUDIO_STATE_CONNECTED_MSBC: return "CONNECTED_MSBC_16_KHZ";
    default: return "UNKNOWN";
  }
}

static void printHelp() {
  Serial.println();
  Serial.println("================ COMANDOS ================");
  Serial.println("h  -> mostrar esta ayuda");
  Serial.println("s  -> buscar de nuevo el EJEAS");
  Serial.println("c  -> conectar HFP usando la MAC recordada");
  Serial.println("a  -> simular llamada activa y abrir audio");
  Serial.println("o  -> intentar abrir solo el canal SCO");
  Serial.println("l  -> modo LOOPBACK: micro EJEAS vuelve a sus altavoces");
  Serial.println("t  -> modo TONO: genera 1 kHz hacia los altavoces");
  Serial.println("x  -> cerrar audio y terminar la llamada simulada");
  Serial.println("d  -> desconectar completamente el intercom");
  Serial.println("b  -> borrar MAC y emparejamiento y volver a buscar");
  Serial.println("i  -> mostrar estado actual");
  Serial.println("==========================================");
  Serial.println();
}

static void printStatus() {
  Serial.println();
  Serial.println("--------------- ESTADO ----------------");
  Serial.printf("HFP preparado:       %s\n", hfpReady ? "SI" : "NO");
  Serial.printf("Buscando:            %s\n", discoveryRunning ? "SI" : "NO");
  Serial.printf("MAC conocida:        %s", haveRemoteBda ? "SI, " : "NO");
  if (haveRemoteBda) {
    printBda(remoteBda);
  }
  Serial.println();
  Serial.printf("SLC conectado:       %s\n", slcConnected ? "SI" : "NO");
  Serial.printf("Llamada simulada:    %s\n", callActive ? "ACTIVA" : "INACTIVA");
  Serial.printf("Audio SCO:           %s\n", audioConnected ? "CONECTADO" : (audioConnecting ? "CONECTANDO" : "DESCONECTADO"));
  Serial.printf("Codec:               %s\n", codecMsbc ? "mSBC 16 kHz" : "CVSD 8 kHz / pendiente");
  Serial.printf("Modo de prueba:      %s\n", audioMode == AudioTestMode::LOOPBACK ? "LOOPBACK" : "TONO 1 kHz");
  Serial.println("---------------------------------------");
  Serial.println();
}

// -----------------------------------------------------------------------------
// Escaneo y conexion
// -----------------------------------------------------------------------------
static void startDiscovery(bool force) {
  if (!hfpReady || slcConnected || discoveryRunning) {
    return;
  }

  const uint32_t now = millis();
  if (!force && now - lastDiscoveryAttemptMs < DISCOVERY_RETRY_MS) {
    return;
  }

  lastDiscoveryAttemptMs = now;
  Serial.println("\nBuscando dispositivos Bluetooth Classic...");
  Serial.println("Pon el EJEAS en modo PHONE PAIRING y apaga el Bluetooth del movil.");
  const esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, DISCOVERY_LENGTH, 0);
  logEsp("esp_bt_gap_start_discovery", err);
  if (err == ESP_OK) {
    discoveryRunning = true;
  }
}

static void requestSlcConnect(bool force) {
  if (!hfpReady || !haveRemoteBda || slcConnected || slcInProgress) {
    return;
  }

  const uint32_t now = millis();
  if (!force && now - lastSlcAttemptMs < SLC_RETRY_MS) {
    return;
  }

  if (discoveryRunning) {
    esp_bt_gap_cancel_discovery();
    connectPending = true;
    return;
  }

  lastSlcAttemptMs = now;
  Serial.print("Conectando HFP/SLC con ");
  printBda(remoteBda);
  Serial.println("...");
  const esp_err_t err = esp_hf_ag_slc_connect(remoteBda);
  logEsp("esp_hf_ag_slc_connect", err);
  if (err == ESP_OK) {
    slcInProgress = true;
  }
}

// -----------------------------------------------------------------------------
// Llamada simulada y audio
// -----------------------------------------------------------------------------
static void startFakeCall() {
  if (!slcConnected || !haveRemoteBda) {
    Serial.println("No se puede iniciar la llamada: falta SLC_CONNECTED.");
    return;
  }

  callActive = true;
  Serial.println("Simulando llamada activa para que el EJEAS habilite el audio...");
  logEsp(
    "esp_hf_ag_out_call",
    esp_hf_ag_out_call(
      remoteBda,
      1,
      0,
      ESP_HF_CALL_STATUS_CALL_IN_PROGRESS,
      ESP_HF_CALL_SETUP_STATUS_IDLE,
      TEST_NUMBER,
      ESP_HF_CALL_ADDR_TYPE_UNKNOWN
    )
  );
  audioOpenAtMs = millis() + AUDIO_OPEN_DELAY_MS;
}

static void requestAudioConnect(bool force) {
  if (!slcConnected || !haveRemoteBda || audioConnected) {
    return;
  }

  const uint32_t now = millis();
  if (!force && audioConnecting && now - lastAudioAttemptMs < AUDIO_RETRY_MS) {
    return;
  }

  lastAudioAttemptMs = now;
  audioConnecting = true;
  Serial.println("Abriendo canal de audio SCO/eSCO...");
  logEsp("esp_hf_ag_audio_connect", esp_hf_ag_audio_connect(remoteBda));
}

static void stopTestCall() {
  audioOpenAtMs = 0;
  autoCallAtMs = 0;

  if (audioConnected || audioConnecting) {
    logEsp("esp_hf_ag_audio_disconnect", esp_hf_ag_audio_disconnect(remoteBda));
  }

  if (slcConnected && callActive) {
    logEsp(
      "esp_hf_ag_end_call",
      esp_hf_ag_end_call(
        remoteBda,
        0,
        0,
        ESP_HF_CALL_STATUS_NO_CALLS,
        ESP_HF_CALL_SETUP_STATUS_IDLE,
        TEST_NUMBER,
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN
      )
    );
  }

  callActive = false;
  audioConnecting = false;
}

// -----------------------------------------------------------------------------
// Pipeline de audio HCI
// -----------------------------------------------------------------------------
static void audioTimerCallback(void *arg) {
  (void)arg;
  if (audioConnected) {
    esp_hf_ag_outgoing_data_ready();
  }
}

static void stopAudioPipeline() {
  if (audioTimer) {
    esp_timer_stop(audioTimer);
    esp_timer_delete(audioTimer);
    audioTimer = nullptr;
  }

  if (audioRing) {
    vStreamBufferDelete(audioRing);
    audioRing = nullptr;
  }
}

static void startAudioPipeline(bool msbc) {
  stopAudioPipeline();

  audioRing = xStreamBufferCreate(AUDIO_RING_BYTES, 1);
  if (!audioRing) {
    Serial.println("ERROR: no se pudo crear el buffer circular de audio.");
    return;
  }

  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = audioTimerCallback;
  timerArgs.arg = nullptr;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "hfp_audio";
  timerArgs.skip_unhandled_events = true;

  esp_err_t err = esp_timer_create(&timerArgs, &audioTimer);
  if (err != ESP_OK) {
    logEsp("esp_timer_create", err);
    vStreamBufferDelete(audioRing);
    audioRing = nullptr;
    return;
  }

  err = esp_timer_start_periodic(audioTimer, msbc ? MSBC_TRIGGER_US : CVSD_TRIGGER_US);
  if (err != ESP_OK) {
    logEsp("esp_timer_start_periodic", err);
    esp_timer_delete(audioTimer);
    audioTimer = nullptr;
    vStreamBufferDelete(audioRing);
    audioRing = nullptr;
    return;
  }

  rxFrames = 0;
  rxBytes = 0;
  rxDrops = 0;
  txFrames = 0;
  txBytes = 0;
  txUnderruns = 0;
  lastRequestedBytes = 0;
  tonePhase = 0.0f;
  lastDiagnosticMs = millis();
}

static void incomingAudioCallback(const uint8_t *buf, uint32_t len) {
  if (!buf || len == 0) {
    return;
  }

  ++rxFrames;
  rxBytes += len;

  if (audioMode != AudioTestMode::LOOPBACK || !audioRing) {
    return;
  }

  if (xStreamBufferSpacesAvailable(audioRing) < len) {
    ++rxDrops;
    return;
  }

  const size_t written = xStreamBufferSend(audioRing, buf, len, 0);
  if (written != len) {
    ++rxDrops;
  }
}

static uint32_t outgoingAudioCallback(uint8_t *buf, uint32_t len) {
  if (!buf || len == 0 || !audioConnected) {
    return 0;
  }

  lastRequestedBytes = len;

  if (audioMode == AudioTestMode::TONE) {
    const float sampleRate = codecMsbc ? 16000.0f : 8000.0f;
    const float phaseStep = 2.0f * PI * TEST_TONE_HZ / sampleRate;
    const uint32_t samples = len / sizeof(int16_t);
    int16_t *pcm = reinterpret_cast<int16_t *>(buf);

    for (uint32_t i = 0; i < samples; ++i) {
      pcm[i] = static_cast<int16_t>(sinf(tonePhase) * TEST_TONE_AMPLITUDE);
      tonePhase += phaseStep;
      if (tonePhase >= 2.0f * PI) {
        tonePhase -= 2.0f * PI;
      }
    }

    if (len & 1U) {
      buf[len - 1] = 0;
    }

    ++txFrames;
    txBytes += len;
    return len;
  }

  if (!audioRing || xStreamBufferBytesAvailable(audioRing) < len) {
    ++txUnderruns;
    return 0;
  }

  const size_t received = xStreamBufferReceive(audioRing, buf, len, 0);
  if (received != len) {
    ++txUnderruns;
    return 0;
  }

  ++txFrames;
  txBytes += received;
  return static_cast<uint32_t>(received);
}

// -----------------------------------------------------------------------------
// Respuestas HFP obligatorias/basicas
// -----------------------------------------------------------------------------
static void sendIndicatorState(esp_bd_addr_t addr) {
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALL, callActive ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS : ESP_HF_CALL_STATUS_NO_CALLS);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALLSETUP, ESP_HF_CALL_SETUP_STATUS_IDLE);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_SERVICE, ESP_HF_NETWORK_STATE_AVAILABLE);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_SIGNAL, 5);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_ROAM, ESP_HF_ROAMING_STATUS_INACTIVE);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_BATTCHG, 5);
  esp_hf_ag_ciev_report(addr, ESP_HF_IND_TYPE_CALLHELD, ESP_HF_CALL_HELD_STATUS_NONE);
}

static void answerCurrentCall(esp_bd_addr_t addr) {
  callActive = true;
  logEsp(
    "esp_hf_ag_answer_call",
    esp_hf_ag_answer_call(
      addr,
      1,
      0,
      ESP_HF_CALL_STATUS_CALL_IN_PROGRESS,
      ESP_HF_CALL_SETUP_STATUS_IDLE,
      TEST_NUMBER,
      ESP_HF_CALL_ADDR_TYPE_UNKNOWN
    )
  );
  audioOpenAtMs = millis() + AUDIO_OPEN_DELAY_MS;
}

// -----------------------------------------------------------------------------
// Callback GAP: escaneo y emparejamiento
// -----------------------------------------------------------------------------
static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
      int8_t rssi = 0;

      for (int i = 0; i < param->disc_res.num_prop; ++i) {
        const esp_bt_gap_dev_prop_t &prop = param->disc_res.prop[i];

        if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME && prop.val && prop.len > 0) {
          const size_t copyLen = min(static_cast<size_t>(prop.len), sizeof(name) - 1);
          memcpy(name, prop.val, copyLen);
          name[copyLen] = '\0';
        } else if (prop.type == ESP_BT_GAP_DEV_PROP_EIR && name[0] == '\0') {
          getNameFromEir(static_cast<uint8_t *>(prop.val), name, sizeof(name));
        } else if (prop.type == ESP_BT_GAP_DEV_PROP_RSSI && prop.val) {
          rssi = *static_cast<int8_t *>(prop.val);
        }
      }

      Serial.print("Encontrado: ");
      printBda(param->disc_res.bda);
      Serial.printf("  RSSI=%d  nombre=\"%s\"\n", rssi, name[0] ? name : "sin nombre");

      if (!slcConnected && isTargetName(name)) {
        Serial.println("*** Coincide con EJEAS/V4. Guardando y conectando. ***");
        setRemoteBda(param->disc_res.bda, true);
        connectPending = true;
        esp_bt_gap_cancel_discovery();
      }
      break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      discoveryRunning = (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
      Serial.printf("Estado de busqueda: %s\n", discoveryRunning ? "INICIADA" : "TERMINADA");
      if (!discoveryRunning && connectPending) {
        connectPending = false;
        requestSlcConnect(true);
      }
      break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        Serial.print("Emparejamiento correcto con ");
        printBda(param->auth_cmpl.bda);
        Serial.printf("  nombre=\"%s\"\n", param->auth_cmpl.device_name);
        setRemoteBda(param->auth_cmpl.bda, true);
      } else {
        Serial.printf("ERROR de emparejamiento. Estado=%d\n", param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_CFM_REQ_EVT:
      Serial.printf("SSP: aceptando automaticamente el codigo %06lu para ", static_cast<unsigned long>(param->cfm_req.num_val));
      printBda(param->cfm_req.bda);
      Serial.println();
      setRemoteBda(param->cfm_req.bda, true);
      logEsp("esp_bt_gap_ssp_confirm_reply", esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true));
      break;

    case ESP_BT_GAP_PIN_REQ_EVT: {
      esp_bt_pin_code_t pinCode = {'0', '0', '0', '0'};
      Serial.print("PIN solicitado por ");
      printBda(param->pin_req.bda);
      Serial.println(". Respondiendo 0000.");
      setRemoteBda(param->pin_req.bda, true);
      logEsp("esp_bt_gap_pin_reply", esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pinCode));
      break;
    }

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Callback HFP Audio Gateway
// -----------------------------------------------------------------------------
static void hfpCallback(esp_hf_cb_event_t event, esp_hf_cb_param_t *param) {
  switch (event) {
    case ESP_HF_PROF_STATE_EVT:
      hfpReady = (param->prof_stat.state == ESP_HF_INIT_SUCCESS || param->prof_stat.state == ESP_HF_INIT_ALREADY);
      Serial.printf("Perfil HFP AG: estado=%d, preparado=%s\n", param->prof_stat.state, hfpReady ? "SI" : "NO");
      if (hfpReady) {
        if (USE_FIXED_MAC) {
          setRemoteBda(FIXED_MAC, false);
        }
        if (haveRemoteBda) {
          requestSlcConnect(true);
        } else {
          startDiscovery(true);
        }
      }
      break;

    case ESP_HF_CONNECTION_STATE_EVT: {
      setRemoteBda(param->conn_stat.remote_bda, true);
      const esp_hf_connection_state_t state = param->conn_stat.state;
      slcConnected = (state == ESP_HF_CONNECTION_STATE_SLC_CONNECTED);
      slcInProgress = (state == ESP_HF_CONNECTION_STATE_CONNECTING || state == ESP_HF_CONNECTION_STATE_CONNECTED);

      Serial.printf(
        "HFP conexion: %s  peer_feat=0x%08lX  chld=0x%08lX  ",
        connectionStateName(state),
        static_cast<unsigned long>(param->conn_stat.peer_feat),
        static_cast<unsigned long>(param->conn_stat.chld_feat)
      );
      printBda(param->conn_stat.remote_bda);
      Serial.println();

      if (state == ESP_HF_CONNECTION_STATE_DISCONNECTED) {
        slcConnected = false;
        slcInProgress = false;
        audioConnecting = false;
        audioConnected = false;
        callActive = false;
        codecMsbc = false;
        autoCallAtMs = 0;
        audioOpenAtMs = 0;
        stopAudioPipeline();
      } else if (slcConnected) {
        slcInProgress = false;
        Serial.println("*** SLC_CONNECTED: control HFP listo. ***");
        logEsp("volumen altavoz", esp_hf_ag_volume_control(remoteBda, ESP_HF_VOLUME_CONTROL_TARGET_SPK, 12));
        logEsp("volumen microfono", esp_hf_ag_volume_control(remoteBda, ESP_HF_VOLUME_CONTROL_TARGET_MIC, 15));
        if (AUTO_START_AUDIO) {
          autoCallAtMs = millis() + AUTO_CALL_DELAY_MS;
        }
      }
      break;
    }

    case ESP_HF_AUDIO_STATE_EVT: {
      const esp_hf_audio_state_t state = param->audio_stat.state;
      audioConnecting = (state == ESP_HF_AUDIO_STATE_CONNECTING);
      audioConnected = (state == ESP_HF_AUDIO_STATE_CONNECTED || state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC);
      codecMsbc = (state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC);

      Serial.printf(
        "Audio HFP: %s, frame recomendado=%u bytes, handle=%u\n",
        audioStateName(state),
        param->audio_stat.preferred_frame_size,
        param->audio_stat.sync_conn_handle
      );

      if (audioConnected) {
        audioConnecting = false;
        logEsp("esp_hf_ag_register_data_callback", esp_hf_ag_register_data_callback(incomingAudioCallback, outgoingAudioCallback));
        startAudioPipeline(codecMsbc);
        Serial.printf(
          "*** AUDIO CONECTADO: %s. Modo=%s ***\n",
          codecMsbc ? "mSBC 16 kHz" : "CVSD 8 kHz",
          audioMode == AudioTestMode::LOOPBACK ? "LOOPBACK" : "TONO"
        );
      } else if (state == ESP_HF_AUDIO_STATE_DISCONNECTED) {
        audioConnecting = false;
        codecMsbc = false;
        stopAudioPipeline();
      }
      break;
    }

    case ESP_HF_IND_UPDATE_EVT:
      sendIndicatorState(param->ind_upd.remote_addr);
      break;

    case ESP_HF_CIND_RESPONSE_EVT:
      logEsp(
        "esp_hf_ag_cind_response",
        esp_hf_ag_cind_response(
          param->cind_rep.remote_addr,
          callActive ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS : ESP_HF_CALL_STATUS_NO_CALLS,
          ESP_HF_CALL_SETUP_STATUS_IDLE,
          ESP_HF_NETWORK_STATE_AVAILABLE,
          5,
          ESP_HF_ROAMING_STATUS_INACTIVE,
          5,
          ESP_HF_CALL_HELD_STATUS_NONE
        )
      );
      break;

    case ESP_HF_COPS_RESPONSE_EVT: {
      static char operatorName[] = "FormulaGades";
      logEsp("esp_hf_ag_cops_response", esp_hf_ag_cops_response(param->cops_rep.remote_addr, operatorName));
      break;
    }

    case ESP_HF_CLCC_RESPONSE_EVT:
      if (callActive) {
        esp_hf_ag_clcc_response(
          param->clcc_rep.remote_addr,
          1,
          ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
          ESP_HF_CURRENT_CALL_STATUS_ACTIVE,
          ESP_HF_CURRENT_CALL_MODE_VOICE,
          ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE,
          TEST_NUMBER,
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
        TEST_NUMBER,
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN
      );
      break;

    case ESP_HF_CNUM_RESPONSE_EVT:
      logEsp(
        "esp_hf_ag_cnum_response",
        esp_hf_ag_cnum_response(param->cnum_rep.remote_addr, TEST_NUMBER, 129, ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE)
      );
      break;

    case ESP_HF_ATA_RESPONSE_EVT:
      Serial.println("El EJEAS ha enviado ATA (responder llamada).");
      answerCurrentCall(param->ata_rep.remote_addr);
      break;

    case ESP_HF_CHUP_RESPONSE_EVT:
      Serial.println("El EJEAS ha solicitado colgar.");
      stopTestCall();
      break;

    case ESP_HF_DIAL_EVT:
      Serial.println("El EJEAS ha solicitado marcar. Aceptando como llamada de prueba.");
      logEsp("respuesta OK", esp_hf_ag_cmee_send(param->out_call.remote_addr, ESP_HF_AT_RESPONSE_CODE_OK, ESP_HF_CME_AG_FAILURE));
      answerCurrentCall(param->out_call.remote_addr);
      break;

    case ESP_HF_UNAT_RESPONSE_EVT: {
      const char *cmd = param->unat_rep.unat ? param->unat_rep.unat : "(null)";
      Serial.printf("AT propietario/no reconocido: %s\n", cmd);

      // El V6 Pro+ envia +SUGCODEC=1 durante la negociacion HFP.
      // Arduino-ESP32 3.3.11 no lo interpreta, asi que lo aceptamos para
      // que el intercom continue hasta SLC_CONNECTED en lugar de desconectar.
      static char okResponse[] = "OK";
      logEsp(
        "respuesta AT propietario OK",
        esp_hf_ag_unknown_at_send(param->unat_rep.remote_addr, okResponse)
      );
      break;
    }

    case ESP_HF_VOLUME_CONTROL_EVT:
      Serial.printf("Volumen desde EJEAS: destino=%d, valor=%d\n", param->volume_control.type, param->volume_control.volume);
      break;

    case ESP_HF_NREC_RESPONSE_EVT:
      Serial.printf("NREC solicitado: %d\n", param->nrec.state);
      break;

    case ESP_HF_BVRA_RESPONSE_EVT:
      Serial.printf("Reconocimiento de voz: %d\n", param->vra_rep.value);
      break;

#if defined(CONFIG_BT_HFP_WBS_ENABLE) && CONFIG_BT_HFP_WBS_ENABLE
    case ESP_HF_WBS_RESPONSE_EVT:
      Serial.printf("WBS: codec=%d\n", param->wbs_rep.codec);
      break;
#endif

    case ESP_HF_BCS_RESPONSE_EVT:
      Serial.printf("Negociacion de codec BCS: modo=%d\n", param->bcs_rep.mode);
      break;

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Inicializacion Bluetooth
// -----------------------------------------------------------------------------
static bool initBluetooth() {
  Serial.println("Iniciando controlador Bluetooth Classic...");

  if (!btStarted()) {
    if (!btStartMode(BT_MODE_CLASSIC_BT)) {
      Serial.println("ERROR: btStartMode(BT_MODE_CLASSIC_BT) ha fallado.");
      Serial.printf("Memoria BT Classic liberada antes de setup(): %s\n", btMemReleased(BT_MODE_CLASSIC_BT) ? "SI" : "NO");
      Serial.println("Comprueba que el sketch incluye esp32-hal-alloc-bt-classic-mem.h.");
      return false;
    }
  }

  esp_bluedroid_status_t status = esp_bluedroid_get_status();
  if (status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    const esp_err_t err = esp_bluedroid_init();
    logEsp("esp_bluedroid_init", err);
    if (err != ESP_OK) {
      return false;
    }
  }

  status = esp_bluedroid_get_status();
  if (status != ESP_BLUEDROID_STATUS_ENABLED) {
    const esp_err_t err = esp_bluedroid_enable();
    logEsp("esp_bluedroid_enable", err);
    if (err != ESP_OK) {
      return false;
    }
  }

  logEsp("esp_bt_gap_register_callback", esp_bt_gap_register_callback(gapCallback));

  esp_bt_io_cap_t ioCapability = ESP_BT_IO_CAP_NONE;
  logEsp(
    "esp_bt_gap_set_security_param",
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &ioCapability, sizeof(ioCapability))
  );

  esp_bt_pin_code_t unusedPin = {0};
  logEsp("esp_bt_gap_set_pin", esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, unusedPin));
  logEsp("esp_bt_dev_set_device_name", esp_bt_dev_set_device_name(LOCAL_BT_NAME));

  esp_bt_cod_t cod = {};
  cod.major = ESP_BT_COD_MAJOR_DEV_PHONE;
  cod.minor = 0x00;
  cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;
  logEsp("esp_bt_gap_set_cod", esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL));

  logEsp(
    "esp_bt_gap_set_scan_mode",
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE)
  );

  // Esta llamada y los callbacks deben estar preparados antes de iniciar HFP.
  logEsp("esp_bredr_sco_datapath_set", esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI));
  logEsp("esp_hf_ag_register_callback", esp_hf_ag_register_callback(hfpCallback));
  logEsp(
    "esp_hf_ag_register_data_callback",
    esp_hf_ag_register_data_callback(incomingAudioCallback, outgoingAudioCallback)
  );
  logEsp("esp_hf_ag_init", esp_hf_ag_init());

  return true;
}

// -----------------------------------------------------------------------------
// Comandos del monitor serie
// -----------------------------------------------------------------------------
static void forgetIntercom() {
  stopTestCall();

  if (slcConnected && haveRemoteBda) {
    esp_hf_ag_slc_disconnect(remoteBda);
  }

  if (haveRemoteBda) {
    const esp_err_t err = esp_bt_gap_remove_bond_device(remoteBda);
    logEsp("esp_bt_gap_remove_bond_device", err);
  }

  prefs.remove(PREF_BDA_KEY);
  memset(remoteBda, 0, sizeof(remoteBda));
  haveRemoteBda = false;
  saveRemotePending = false;
  slcConnected = false;
  callActive = false;
  audioConnected = false;
  audioConnecting = false;
  Serial.println("MAC y emparejamiento borrados.");
  lastDiscoveryAttemptMs = 0;
  startDiscovery(true);
}

static void processSerialCommand(char command) {
  switch (command) {
    case 'h':
    case '?':
      printHelp();
      break;

    case 's':
      if (discoveryRunning) {
        esp_bt_gap_cancel_discovery();
      }
      memset(remoteBda, 0, sizeof(remoteBda));
      haveRemoteBda = false;
      lastDiscoveryAttemptMs = 0;
      startDiscovery(true);
      break;

    case 'c':
      requestSlcConnect(true);
      break;

    case 'a':
      startFakeCall();
      break;

    case 'o':
      requestAudioConnect(true);
      break;

    case 'l':
      audioMode = AudioTestMode::LOOPBACK;
      Serial.println("Modo LOOPBACK activado. Habla: deberias oirte con un pequeno retardo.");
      break;

    case 't':
      audioMode = AudioTestMode::TONE;
      Serial.println("Modo TONO activado. Deberias oir un tono de 1 kHz.");
      break;

    case 'x':
      stopTestCall();
      break;

    case 'd':
      stopTestCall();
      if (slcConnected && haveRemoteBda) {
        logEsp("esp_hf_ag_slc_disconnect", esp_hf_ag_slc_disconnect(remoteBda));
      }
      break;

    case 'b':
      forgetIntercom();
      break;

    case 'i':
      printStatus();
      break;

    case '\r':
    case '\n':
    case ' ':
      break;

    default:
      Serial.printf("Comando desconocido: '%c'. Escribe h.\n", command);
      break;
  }
}

// -----------------------------------------------------------------------------
// Arduino setup/loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1200);

  Serial.println();
  Serial.println("====================================================");
  Serial.println(" PRUEBA EJEAS V6 PRO+ - ESP32 HFP AUDIO GATEWAY V5");
  Serial.println("====================================================");
  Serial.println("El ESP32 se presenta como un telefono HFP.");
  Serial.println("Pon el EJEAS en modo de emparejamiento con TELEFONO.");
  Serial.println();

  loadSavedBda();
  printHelp();

  if (!initBluetooth()) {
    Serial.println("ERROR FATAL inicializando Bluetooth.");
    return;
  }
}

void loop() {
  while (Serial.available() > 0) {
    processSerialCommand(static_cast<char>(Serial.read()));
  }

  saveBdaIfNeeded();

  const uint32_t now = millis();

  if (hfpReady && !slcConnected) {
    if (haveRemoteBda) {
      requestSlcConnect(false);
    } else {
      startDiscovery(false);
    }
  }

  if (slcConnected && autoCallAtMs != 0 && static_cast<int32_t>(now - autoCallAtMs) >= 0) {
    autoCallAtMs = 0;
    startFakeCall();
  }

  if (slcConnected && callActive && audioOpenAtMs != 0 && static_cast<int32_t>(now - audioOpenAtMs) >= 0) {
    audioOpenAtMs = 0;
    requestAudioConnect(true);
  }

  if (audioConnecting && !audioConnected && now - lastAudioAttemptMs >= AUDIO_RETRY_MS) {
    requestAudioConnect(false);
  }

  if (audioConnected && now - lastDiagnosticMs >= 1000) {
    lastDiagnosticMs = now;

    noInterrupts();
    const uint32_t rxF = rxFrames;
    const uint32_t rxB = rxBytes;
    const uint32_t rxD = rxDrops;
    const uint32_t txF = txFrames;
    const uint32_t txB = txBytes;
    const uint32_t txU = txUnderruns;
    const uint32_t req = lastRequestedBytes;
    interrupts();

    Serial.printf(
      "AUDIO [%s/%s] RX=%lu frames, %lu B, drops=%lu | TX=%lu frames, %lu B, underrun=%lu | pedido=%lu B\n",
      codecMsbc ? "mSBC16k" : "CVSD8k",
      audioMode == AudioTestMode::LOOPBACK ? "LOOPBACK" : "TONO",
      static_cast<unsigned long>(rxF),
      static_cast<unsigned long>(rxB),
      static_cast<unsigned long>(rxD),
      static_cast<unsigned long>(txF),
      static_cast<unsigned long>(txB),
      static_cast<unsigned long>(txU),
      static_cast<unsigned long>(req)
    );
  }

  delay(20);
}
