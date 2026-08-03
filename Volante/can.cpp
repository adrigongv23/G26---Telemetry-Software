#include "can.hpp"
#include "led.hpp"
#include "configuracion.hpp"
#include <Arduino.h>

static bool driver_installed = false;

void CAN::start() {
    Serial.println("Starting CAN Controller...");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 64;
    g_config.tx_queue_len = 16;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("Failed to install TWAI driver");
        driver_installed = false;
        return;
    }

    Serial.println("TWAI driver installed");

    if (twai_start() != ESP_OK) {
        Serial.println("Failed to start TWAI driver");
        twai_driver_uninstall();
        driver_installed = false;
        return;
    }

    Serial.println("TWAI driver started");

    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;

    if (twai_reconfigure_alerts(alerts_to_enable, nullptr) != ESP_OK) {
        Serial.println("Failed to reconfigure alerts");
        twai_stop();
        twai_driver_uninstall();
        driver_installed = false;
        return;
    }

    Serial.println("CAN Alerts reconfigured");
    driver_installed = true;
}

CAN::~CAN() {
    stop_listening_task();

    if (driver_installed) {
        twai_stop();
        twai_driver_uninstall();
        driver_installed = false;
    }
}

void CAN::start_listening_task() {
    if (_listen_task_handle != nullptr) {
        Serial.println("CAN listening task already running");
        return;
    }

    _should_stop_listening = false;

    BaseType_t result = xTaskCreate(listenTask, "CAN_Listen_Task", 4096, this, 1, &_listen_task_handle);

    if (result == pdPASS) {
        Serial.println("CAN listening task created successfully");
    } else {
        Serial.println("Failed to create CAN listening task");
        _listen_task_handle = nullptr;
    }
}

void CAN::stop_listening_task() {
    if (_listen_task_handle == nullptr) {
        return;
    }

    _should_stop_listening = true;

    for (int i = 0; i < 100 && _listen_task_handle != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (_listen_task_handle != nullptr) {
        vTaskDelete(_listen_task_handle);
        _listen_task_handle = nullptr;
    }

    Serial.println("CAN listening task stopped");
}

void CAN::listenTask(void *arg) {
    CAN *instance = static_cast<CAN *>(arg);
    instance->listen_id();
    instance->_listen_task_handle = nullptr;
    vTaskDelete(nullptr);
}

void CAN::listen_id() {
    Serial.println("CAN listening task started");

    uint32_t last_can_update = 0;

    while (!_should_stop_listening) {
        if (!driver_installed) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        uint32_t alerts_triggered = 0;
        twai_read_alerts(&alerts_triggered, 0);

        if (alerts_triggered & (TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL)) {
            twai_status_info_t twai_status;
            twai_get_status_info(&twai_status);

            if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
                Serial.println("Alert: TWAI controller has become error passive.");
            }

            if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
                Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
                Serial.printf("Bus error count: %lu\n", twai_status.bus_error_count);
            }

            if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
                Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
                Serial.printf("RX buffered: %lu\t", twai_status.msgs_to_rx);
                Serial.printf("RX missed: %lu\t", twai_status.rx_missed_count);
                Serial.printf("RX overrun: %lu\n", twai_status.rx_overrun_count);
            }
        }

        if (alerts_triggered & TWAI_ALERT_RX_DATA) {
            twai_message_t message;

            while (twai_receive(&message, 0) == ESP_OK && !_should_stop_listening) {
                
                if (message.rtr || message.data_length_code < 3) {
                    taskYIELD();
                    continue;
                }

                switch (message.data[0]) {
                    case 0:
                        led_show_rpm(message.data[1], message.data[2]);
                        break;

                    case 1:

                        break;

                    case 2:

                        break;

                    case 3:

                        break;

                    case 100:

                        break;

                    default:
                        break;
                }

                taskYIELD();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    Serial.println("CAN listening task ending");
}
