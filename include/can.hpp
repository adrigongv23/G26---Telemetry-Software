#ifndef CAN_HPP
#define CAN_HPP

#define RX_PIN 13
#define TX_PIN 38
#define POLLING_RATE_MS 1000
#define TRANSMIT_RATE_MS 1000

#include "driver/twai.h"
#include "common/common_libraries.hpp"
#include "data_processor.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class CAN {
public:
    CAN(): _mutex(xSemaphoreCreateMutex()), _listen_task_handle(NULL), _should_stop_listening(false) {}
    ~CAN();
    void start();
    void listen();
    void start_listening_task();
    void stop_listening_task();
    void send_frame(twai_message_t message);
    twai_message_t createBoolMessage(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);

    void set_data_proccessor(DataProcessor *data_processor) {
        _data_processor = data_processor;
    }

    SemaphoreHandle_t get_mutex() {
        return _mutex;
    }

    static void listenTask(void *arg) {
        CAN *controller = static_cast<CAN*>(arg);
        controller->listen();
    }

private:
    twai_message_t _rx_message;
    DataProcessor *_data_processor;
    SemaphoreHandle_t _mutex;
    TaskHandle_t _listen_task_handle;
    volatile bool _should_stop_listening;
    int test = 0;
};

#endif
