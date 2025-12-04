/**
 * @file can.cpp
 * @author Raúl Arcos Herrera
 * @brief This file contains the implementation of the CAN Controller class for Link G4+ ECU.
 */

#include "../include/can.hpp"

static bool driver_installed = false;

void CAN::start() {
    Serial.println("Starting CAN Controller...");
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t install_status = twai_driver_install(&g_config, &t_config, &f_config);
    if (install_status != ESP_OK) {
        Serial.println("Failed to install TWAI driver");
        driver_installed = false;
        return;
    } else {
        Serial.println("TWAI driver installed");
    }

    esp_err_t start_status = twai_start();
    if (start_status != ESP_OK) {
        Serial.println("Failed to start TWAI driver");
        driver_installed = false;
        return;
    } else {
        Serial.println("TWAI driver started");
    }

    uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
    if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
        Serial.println("CAN Alerts reconfigured");
    } else {
        Serial.println("Failed to reconfigure alerts");
        driver_installed = false;
        return;
    }

    // TWAI driver is now successfully installed and started
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
    if (_listen_task_handle == NULL) {
        _should_stop_listening = false;
        
        BaseType_t result = xTaskCreate(
            listenTask,           // Task function
            "CAN_Listen_Task",    // Task name
            4096,                 // Stack size (words)
            this,                 // Task parameter (this CAN instance)
            1,                    // Priority (lowered from 5 to 1)
            &_listen_task_handle  // Task handle
        );
        
        // BaseType_t result = xTaskCreatePinnedToCore(
        //     listenTask,           // Task function
        //     "CAN_Listen_Task",    // Task name
        //     4096,                 // Stack size (words)
        //     this,                 // Task parameter (this CAN instance)
        //     1,                    // Priority
        //     &_listen_task_handle, // Task handle
        //     0                     // Core 0 (main loop typically runs on Core 1)
        // );
        
        if (result == pdPASS) {
            Serial.println("CAN listening task created successfully");
        } else {
            Serial.println("Failed to create CAN listening task");
            _listen_task_handle = NULL;
        }
    } else {
        Serial.println("CAN listening task already running");
    }
}

void CAN::stop_listening_task() {
    if (_listen_task_handle != NULL) {
        _should_stop_listening = true;
        
        // Wait for task to finish (max 1 second)
        for (int i = 0; i < 100; i++) {
            if (_listen_task_handle == NULL) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Force delete if still running
        if (_listen_task_handle != NULL) {
            vTaskDelete(_listen_task_handle);
            _listen_task_handle = NULL;
        }
        
        Serial.println("CAN listening task stopped");
    }
}

void CAN::send_frame(twai_message_t message) {
    while (xSemaphoreTake(_mutex, portMAX_DELAY) != pdTRUE) {
        Serial.println("Retrying to take mutex in send_frame");
    }
    twai_transmit(&message, pdMS_TO_TICKS(TRANSMIT_RATE_MS));
    xSemaphoreGive(_mutex);
}

twai_message_t CAN::createBoolMessage(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7) {
    twai_message_t message;
    memset(&message, 0, sizeof(message));
    message.identifier = 0x001;
    message.data[0] = (b7 << 7) | (b6 << 6) | (b5 << 5) | (b4 << 4) |
                      (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
    message.data_length_code = 8;
    message.flags = TWAI_MSG_FLAG_NONE;
    return message;
}

void CAN::listen() {
    Serial.println("CAN listening task started");
    
    // Continuous loop for the thread
    while (!_should_stop_listening) {
        if (!driver_installed) {
            // Driver not installed
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Check if alert happened
        uint32_t alerts_triggered;
        twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(1000)); // Reduced timeout for more responsiveness
        twai_status_info_t twaistatus;
        twai_get_status_info(&twaistatus);

        // Handle alerts
        if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
            Serial.println("Alert: TWAI controller has become error passive.");
        }
        if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
            Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
            Serial.printf("Bus error count: %lu\n", twaistatus.bus_error_count);
        }
        if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
            Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
            Serial.printf("RX buffered: %lu\t", twaistatus.msgs_to_rx);
            Serial.printf("RX missed: %lu\t", twaistatus.rx_missed_count);
            Serial.printf("RX overrun %lu\n", twaistatus.rx_overrun_count);
        }

        if (alerts_triggered & TWAI_ALERT_RX_DATA) {
            twai_message_t message;
            int message_count = 0;
            while (twai_receive(&message, 0) == ESP_OK && !_should_stop_listening) {
                bool all_zeros = true;
                for (int i = 0; i < message.data_length_code; i++) {
                    if (message.data[i] != 0) {
                        all_zeros = false;
                        break;
                    }
                }
                
                if (all_zeros) {
                    Serial.println("Ignoring message with all zero data");
                    taskYIELD();
                    continue;
                }
                
                if (message.extd) {
                    Serial.println("Extended Format");
                } else {
                    Serial.println("Standard Format");
                }
                Serial.printf("ID: %lx\nByte:", message.identifier);
                if (!(message.rtr)) {
                    for (int i = 0; i < message.data_length_code; i++) {
                        Serial.printf(" %d = %02x,", i, message.data[i]);
                    }
                    Serial.println("");
                    
                    // Send to data processor based on first byte (maintaining original logic)
                    switch (message.data[0]) {
                        case 0:
                            _data_processor->send_serial_frame_0(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                            break;
                        case 1:
                            _data_processor->send_serial_frame_1(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                            break;
                        case 2:
                             _data_processor->send_serial_frame_2(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                             break;
                        case 3: 
                            _data_processor->send_serial_frame_3(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                        default:
                            break;
                    }
                }
                
                taskYIELD();
            }
        }

        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    Serial.println("CAN listening task ending");
    _listen_task_handle = NULL;
    vTaskDelete(NULL); // Delete this task
}
