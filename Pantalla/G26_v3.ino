#include "include/data_processor.hpp"
#include "include/can.hpp"
#include "include/g24_wheel_buttons.hpp"
#include "include/led_strip.hpp"
#include "include/crowpanel_controller.hpp"
#include "include/ajustes.hpp"
#include "include/guardado.hpp"
#include "lv_conf.h"

#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

DataProcessor dataProcessor;
CAN canController;
G24WheelButtons wheelButtons;
LedStrip ledStrip;
CrowPanelController crowPanelController;
data datos;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting setup...");

    canController.set_data_proccessor(&dataProcessor);
    canController.start();

        set_data(&datos);

    if(!LittleFS.begin(false)){
        Serial.println("LittleFS corrupto o sin formato. Formateando...");

        if(LittleFS.format()){
            Serial.println("LittleFS formateado correctamente");

            if(!LittleFS.begin(false)){
                Serial.println("Error montando LittleFS despues de formatear");
            } else {
                Serial.println("LittleFS iniciado correctamente");
                load();
            }

        } else {
            Serial.println("Error formateando LittleFS");
        }

    } else {
        Serial.println("LittleFS iniciado correctamente");
        load();
    }

    crowPanelController.begin();

    dataProcessor.set_led_strip(&ledStrip);
    dataProcessor.set_crow_panel_controller(&crowPanelController);
    //wheelButtons.set_can(&canController);
    //wheelButtons.begin();
    //ledStrip.begin();
    // wheelButtons.set_led_strip(&ledStrip);
    // wheelButtons.set_can_controller(&canController);
    // wheelButtons.set_data_processor(&dataProcessor);
    // ledStrip.set_mutex(canController.get_mutex());

    //wheelButtons.ejecucion();
    
    // wheelButtons.begin();
   
    // xTaskCreate(wheelButtons.updateTask, "updateTask", 4096, &wheelButtons, 1, NULL);
    
    // Initialize with screen 1

    if(get_modo_oscuro()==true){
        ui_theme_set(1);
    } else {
        ui_theme_set(2);
    }

    DataProcessor::set_dataprocessor();
    CrowPanelController::create_rpm_bar((get_rpm()/1000)*1000, get_desplazamiento());

    lv_obj_add_flag(ui_barcontainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_tiraledsrpmcontainer, LV_OBJ_FLAG_HIDDEN);
    if(get_rpm_display()==0){
        lv_obj_clear_flag(ui_barcontainer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_tiraledsrpmcontainer, LV_OBJ_FLAG_HIDDEN);
    }

    int recepcion_datos=get_recepcion_datos();
    lv_dropdown_set_selected(ui_recepciondatosdrop, recepcion_datos);

    if(recepcion_datos==1){
        canController.start_iracing_task();
    } else {
        canController.clear_rx_queue();
        canController.start_listening_task();
    }

}

void loop(){ 
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_timer_handler();
    xSemaphoreGiveRecursive(lvgl_mutex);

    vTaskDelay(5);
}