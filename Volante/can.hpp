#ifndef CAN_HPP
#define CAN_HPP

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class CAN {
public:
    CAN() : _listen_task_handle(nullptr), _should_stop_listening(false) {}
    ~CAN();

    void start();
    void start_listening_task();
    void stop_listening_task();
    void listen_id();

private:
    static void listenTask(void *arg);

    TaskHandle_t _listen_task_handle;
    volatile bool _should_stop_listening;
};

#endif
