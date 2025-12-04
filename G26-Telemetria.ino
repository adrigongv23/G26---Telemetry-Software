#include "include/data_processor.hpp"
#include "include/can.hpp"
#include "include/g24_wheel_buttons.hpp"
#include "include/led_strip.hpp"
#include "include/crowpanel_controller.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

DataProcessor dataProcessor;
CAN canController;
G24WheelButtons wheelButtons;
LedStrip ledStrip;
CrowPanelController crowPanelController;

// Screen rotation variables
unsigned long lastScreenChange = 0;
int currentScreen = 1;
int screenCycle = 0; // 0 = screen1, 1 = screen2, 2 = screen3, 3 = screen4

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("Starting setup...");
    canController.set_data_proccessor(&dataProcessor);
    dataProcessor.set_led_strip(&ledStrip);
    dataProcessor.set_crow_panel_controller(&crowPanelController);
    // wheelButtons.set_led_strip(&ledStrip);
    // wheelButtons.set_can_controller(&canController);
    // wheelButtons.set_data_processor(&dataProcessor);
    // ledStrip.set_mutex(canController.get_mutex());

    canController.start();
    canController.start_listening_task();
    

    // wheelButtons.begin();
   
    // xTaskCreate(wheelButtons.updateTask, "updateTask", 4096, &wheelButtons, 1, NULL);
    
    // Initialize with screen 1
    lastScreenChange = millis();
}

void loop(){ 
    lv_timer_handler();
    vTaskDelay(5);
}