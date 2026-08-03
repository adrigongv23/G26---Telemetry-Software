#ifndef CAN_HPP
#define CAN_HPP

#define RX_PIN 13
#define TX_PIN 38
#define POLLING_RATE_MS 1000
#define TRANSMIT_RATE_MS 1000
#define ID_BOTONES 5

#include "driver/twai.h"
#include "common/common_libraries.hpp"
#include "data_processor.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class CAN {
public:
    CAN(): _mutex(xSemaphoreCreateMutex()), _listen_task_handle(NULL), _iracing_task_handle(NULL), _should_stop_listening(false), _should_stop_iracing(false) {}
    ~CAN();
    static void listenTask(void *arg);
    static void iracingTask(void *arg);
    void start();
    void listen();
    void start_listening_task();
    void stop_listening_task();
    void start_iracing_task();
    void stop_iracing_task();
    void send_frame(twai_message_t message);

    void clear_rx_queue();

    twai_message_t createBoolMessage(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);
    twai_message_t mensaje_botonera(uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t levaizq, uint8_t levader);

    void set_data_proccessor(DataProcessor *data_processor) {
        _data_processor = data_processor;
    }

    SemaphoreHandle_t get_mutex() {
        return _mutex;
    }

    void listen_id();
    void listen_id2();
    void data_update();


private:
    twai_message_t _rx_message;
    DataProcessor *_data_processor;
    SemaphoreHandle_t _mutex;
    TaskHandle_t _listen_task_handle;
    TaskHandle_t _iracing_task_handle;
    volatile bool _should_stop_listening=0;
    volatile bool _should_stop_iracing=0;
    uint8_t _serial_protocol=0;
    uint8_t _serial_index=0;
    uint8_t _serial_buffer[21]={0};
    int test = 0;
};

#endif
