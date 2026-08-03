/**
 * @file can.cpp
 * @author Raúl Arcos Herrera
 * @brief This file contains the implementation of the CAN Controller class for Link G4+ ECU.
 */

#include "../include/can.hpp"
#include "../include/data_processor.hpp"
#include <Arduino.h>

static bool driver_installed = false;

static const uint32_t UI_CAN_MS = 40;

static bool every_ms(uint32_t &last_time, uint32_t interval)
{
    uint32_t now = millis();

    if(last_time == 0 || now - last_time >= interval){
        last_time = now;
        return true;
    }

    return false;
}


void CAN::start() {
    Serial.println("Starting CAN Controller...");
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 64;
    g_config.tx_queue_len = 16;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
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
    stop_iracing_task();
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

void CAN::start_iracing_task() {
    if (_iracing_task_handle == NULL) {
        _should_stop_iracing = false;
        _serial_protocol = 0;
        _serial_index = 0;

        BaseType_t result = xTaskCreate(iracingTask, "iRacing_Task", 4096, this, 1, &_iracing_task_handle);

        if (result == pdPASS) {
            Serial.println("iRacing serial task created successfully");
        } else {
            Serial.println("Failed to create iRacing serial task");
            _iracing_task_handle = NULL;
        }
    } else {
        Serial.println("iRacing serial task already running");
    }
}

void CAN::stop_iracing_task() {
    if (_iracing_task_handle != NULL) {
        _should_stop_iracing = true;

        for (int i = 0; i < 100; i++) {
            if (_iracing_task_handle == NULL) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (_iracing_task_handle != NULL) {
            vTaskDelete(_iracing_task_handle);
            _iracing_task_handle = NULL;
        }

        Serial.println("iRacing serial task stopped");
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

void CAN::listenTask(void *arg){
    CAN* instance=static_cast<CAN*>(arg);

    instance->listen_id();

    instance->_listen_task_handle=NULL;
    vTaskDelete(NULL);
}

void CAN::iracingTask(void *arg) {
    CAN* instance = static_cast<CAN*>(arg);

    while (!instance->_should_stop_iracing) {
        instance->data_update();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    instance->_iracing_task_handle = NULL;
    vTaskDelete(NULL);
}

void CAN::listen() {
    Serial.println("CAN listening task started");
    
    // Continuous loop for the thread
    while (!_should_stop_listening) {
        if (!driver_installed) {
            // Driver not installed
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
            Serial.println("No driver");
        }

        mensaje_botonera(1, 1, 1, 1, 1, 1);
        Serial.println("Enviado boton");

        // Check if alert happened
        uint32_t alerts_triggered;
        twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(1000)); // Reduced timeout for more responsiveness
        twai_status_info_t twaistatus;
        twai_get_status_info(&twaistatus);

        //Serial.print(alerts_triggered);
        //Serial.println();

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
            //Serial.println("Alerta detectada");
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
                /*
                if (message.extd) {
                    Serial.println("Extended Format");
                } else {
                    Serial.println("Standard Format");
                }
                */
                //Serial.printf("ID: %lx\nByte:", message.identifier);
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
                            break;
                        case 5:
                            _data_processor->send_serial_frame_volante(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                            break;
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

twai_message_t CAN::mensaje_botonera(uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t leval, uint8_t levar) {
    twai_message_t message;
    memset(&message, 0, sizeof(message));
    message.identifier = 0x40;
    message.data[0]=ID_BOTONES;
    message.data[1]=B1;         // Neutra
    message.data[2]=B2;         // Cambio de pantalla
    message.data[3]=B3;         // Arrancado
    message.data[4]=B4;         // Launch
    message.data[5]=leval;    // Leva izquierda
    message.data[6]=levar;    // Leva derecha
    message.data[7]=1;      // Para que el can lo escuche ya que si todos son 0, ignora el mensaje
    message.data_length_code = 8;
    message.flags = TWAI_MSG_FLAG_SELF;
    return message;
}

void CAN::listen_id() {
    Serial.println("CAN listening task started");

    static uint32_t last_can_update=0;
    
    // Continuous loop for the thread
    while (!_should_stop_listening) {

        if (!driver_installed) {
            // Driver not installed
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
            Serial.println("No driver");
        }

        if(!every_ms(last_can_update, UI_CAN_MS)){
            continue;
        }

        // Check if alert happened
        uint32_t alerts_triggered;
        twai_read_alerts(&alerts_triggered, 0); // Reduced timeout for more responsiveness
        twai_status_info_t twaistatus;
        twai_get_status_info(&twaistatus);

        //Serial.print(alerts_triggered);
        //Serial.println();

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
            //Serial.println("Alerta detectada");
            twai_message_t message;
            int message_count = 0;
            while (twai_receive(&message, 0) == ESP_OK && !_should_stop_listening) {
                //bool all_zeros = true;
                /*
                for (int i = 1; i < message.data_length_code; i++){

                    if (message.data[i] != 0) {
                        all_zeros = false;
                        break;
                    }

                }

                if(message.data[0]==0){
                    float vbatt_prueba = ((message.data[6] * 256.0) + (float)message.data[5])/100.0;
                    if(vbatt_prueba<=2){
                        continue;
                    }
                }

                
                if (all_zeros) {
                    //Serial.println("Ignoring message with all zero data");
                    taskYIELD();
                    continue;
                }
                */
                /*
                if (message.extd) {
                    Serial.println("Extended Format");
                } else {
                    Serial.println("Standard Format");
                }
                */
                //Serial.printf("ID: %lx\nByte:", message.identifier);
                if (!(message.rtr)) {
                    /*
                    for (int i = 0; i < message.data_length_code; i++) {
                        Serial.printf(" %d = %02x,", i, message.data[i]);
                    }
                    Serial.println("");
                    */
                    
                    
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
                            break;
                        case 100:
                            _data_processor->send_serial_frame_volante(message.data[1], message.data[2], message.data[3], message.data[4], message.data[5], message.data[6], message.data[7]);
                            break;
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

void CAN::clear_rx_queue() {
    esp_err_t result = twai_clear_receive_queue();

    /*
    if (result == ESP_OK) {
        Serial.println("CAN RX queue cleared");
    } else {
        Serial.printf("Error clearing CAN RX queue: %d\n", result);
    }
        */
}


void CAN::data_update() {
    while(Serial.available()>0){
        uint8_t b=(uint8_t)Serial.read();

        if(_serial_protocol==0){
            if(b==0xAD){
                _serial_buffer[0]=b;
                _serial_protocol=1;
                _serial_index=1;
            } else if(b==0xAA){
                _serial_protocol=3;
                _serial_index=0;
            }

            continue;
        }

        if(_serial_protocol==1){
            if(b==0x02){
                _serial_buffer[1]=b;
                _serial_protocol=2;
                _serial_index=2;
            } else if(b==0xAD){
                _serial_buffer[0]=b;
                _serial_index=1;
            } else if(b==0xAA){
                _serial_protocol=3;
                _serial_index=0;
            } else {
                _serial_protocol=0;
                _serial_index=0;
            }

            continue;
        }

        if(_serial_protocol==2){
            if(_serial_index<sizeof(_serial_buffer)){
                _serial_buffer[_serial_index++]=b;
            } else {
                _serial_protocol=0;
                _serial_index=0;
                continue;
            }

            if(_serial_index==15 && _serial_buffer[14]==0x0A){
                _data_processor->send_serial_frame_0(_serial_buffer[3], _serial_buffer[2], _serial_buffer[5], _serial_buffer[4], _serial_buffer[7], _serial_buffer[6], _serial_buffer[8]);
                _data_processor->send_serial_frame_1(_serial_buffer[11], _serial_buffer[10], 0, 0, _serial_buffer[13], _serial_buffer[12], _serial_buffer[9]);
                _data_processor->send_serial_frame_2(0, 0, 0, 0, 0, 0, _serial_buffer[9]);

                _serial_protocol=0;
                _serial_index=0;
            } else if(_serial_index==21){
                if(_serial_buffer[20]==0x0A){
                    _data_processor->send_serial_frame_0(_serial_buffer[3], _serial_buffer[2], _serial_buffer[5], _serial_buffer[4], _serial_buffer[7], _serial_buffer[6], _serial_buffer[8]);
                    _data_processor->send_serial_frame_1(_serial_buffer[11], _serial_buffer[10], _serial_buffer[15], _serial_buffer[14], _serial_buffer[13], _serial_buffer[12], _serial_buffer[9]);
                    _data_processor->send_serial_frame_2(_serial_buffer[16], _serial_buffer[17], 0, 0, _serial_buffer[19], _serial_buffer[18], _serial_buffer[9]);
                }

                _serial_protocol=0;
                _serial_index=0;
            }

            continue;
        }

        if(_serial_protocol==3){
            if(_serial_index<13){
                _serial_buffer[_serial_index++]=b;
            }

            if(_serial_index==13){
                uint16_t oil_temperature_raw=(uint16_t)_serial_buffer[5]*100U;

                _data_processor->send_serial_frame_0(_serial_buffer[0], _serial_buffer[1], 0, 0, _serial_buffer[10], _serial_buffer[11], _serial_buffer[12]);
                _data_processor->send_serial_frame_1(0, 0, 0, 0, _serial_buffer[8], _serial_buffer[9], _serial_buffer[4]);
                _data_processor->send_serial_frame_2(0, 0, 0, 0, 0, 0, _serial_buffer[4]);
                _data_processor->send_serial_frame_3((oil_temperature_raw>>8)&0xFF, oil_temperature_raw&0xFF, _serial_buffer[6], _serial_buffer[7], 0, 0, 0);

                _serial_protocol=0;
                _serial_index=0;
            }
        }
    }
}