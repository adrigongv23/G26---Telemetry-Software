/**
 * @file data_processor.cpp
 * @author Raúl Arcos Herrera
 * @brief This file contains the implementation of the Data Processor class for Link G4+ ECU.
 */

#include "../include/data_processor.hpp"

void DataProcessor::send_serial(byte type, unsigned int value) {                     //Como parámetros se pasan el ID (type), que es el ID establecido al inicio del código para el dato que se quiera enviar. Ej: RPM_ID -> 0x51; y se envía el valor de dicho dato.
    byte dato[8] = { 0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00 };  //Se establece un arreglo de bytes con los primeros datos necesarios para que la pantalla lo interprete como mensaje (En la Wiki hay tutoriales que lo explican a fondo), como ser la longitud y el tipo de mensaje.
    dato[4] = type;                                                     //Se configura en el mensaje el ID correspondiente al dato a enviar.
    dato[6] = (value >> 8) & 0xFF;                                      //Se configura el dato en los últimos 2 bytes.
    dato[7] = value & 0xFF;

    Serial.write(dato, 8);                                              //Se envía serialmente el mensaje, indicando su longituden bytes para ello.
}

//RPM + TPS + vBatt + ECT
void DataProcessor::send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect){
    Serial.println("send_serial_frame_0");
    
    int rpm = (rpmh * 256) + rpml; 
    int tps = (tpsh * 256) + tpsl; 
    double vbatt = ((vbatth * 256) + vbattl) / 100.0; 

    _crow_panel_controller->set_value_to_label(ui_rpm, rpm);
    _crow_panel_controller->set_value_to_label(ui_battvolt, vbatt);
    _crow_panel_controller->set_value_to_label(ui_ect, ect);
    _crow_panel_controller->set_value_to_label(ui_ect2, ect);
    
    // Update RPM LED bar (8000-12500 RPM range)
    _crow_panel_controller->update_rpm_bar(rpm);
    
    // Battery voltage color (typical car battery: 12.6V resting, 13.2-14.4V running)
    if (vbatt < 11.5) {
        _crow_panel_controller->set_label_color(ui_battvolt, CrowPanelController::COLOR_CRITICAL);  // Red for low
    } else if (vbatt < 12.0) {
        _crow_panel_controller->set_label_color(ui_battvolt, CrowPanelController::COLOR_WARNING);   // Yellow for warning
    } else if (vbatt > 15.0) {
        _crow_panel_controller->set_label_color(ui_battvolt, CrowPanelController::COLOR_WARNING);   // Yellow for overcharge
    } else {
        _crow_panel_controller->set_label_color(ui_battvolt, CrowPanelController::COLOR_NORMAL);      // Green for good
    }
    
    //El numero que muestra la temperatura siempre será blanco
    _crow_panel_controller->set_label_color(ui_ect, CrowPanelController::COLOR_PANEL_DEFAULT);   
    _crow_panel_controller->set_label_color(ui_ect2, CrowPanelController::COLOR_PANEL_DEFAULT);

    // Engine coolant temperature (typical range: 80-105°C normal operating temp)
    if (ect > 105) {
        // Crítico: Rojo
        _crow_panel_controller->set_panel_color(ui_PanelETC, CrowPanelController::COLOR_CRITICAL);
    } else if (ect >= 95) {
        // Advertencia: Amarillo (95 a 105)
        _crow_panel_controller->set_panel_color(ui_PanelETC, CrowPanelController::COLOR_WARNING);
    } else if (ect >= 65) {
        //Temperatura Ideal: Verde (65 a 94)
        _crow_panel_controller->set_panel_color(ui_PanelETC, CrowPanelController::COLOR_GOOD);
    } else { // etc <= 60 Azul
        _crow_panel_controller->set_panel_color(ui_PanelETC, CrowPanelController::COLOR_BLUE);
    }
}

//LAMB + LAMBTRG + FUEL + GEAR
void DataProcessor::send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear){
    Serial.println("send_serial_frame_1");
   int lmb = (lmbh * 256) + lmbl;
   int lmbtrg = (lmbth * 256) + lmbtl;
   int fuel = (fuelh * 256) + fuell;
   _crow_panel_controller->set_value_to_label(ui_lambda, lmb);
   _crow_panel_controller->set_value_to_label(ui_lambdatarget, lmbtrg);
   _crow_panel_controller->set_value_to_label(ui_fuel, fuel);
//    _crow_panel_controller->set_value_to_label(ui_gear, gear);
}


void DataProcessor::send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1){
    Serial.println("send_serial_frame_2");
    int lmbcorrect = (lmbch * 256) + lmbcl;
    int brake = (brakeh * 256) + brakel;

    char shut_str[10];
    char fan_str[10];
    char aux1_str[10];

    if (shut == 3){
        strcpy(shut_str, "ON");
    } else {
        strcpy(shut_str, "OFF");
    }

    if (fan == 1){
        strcpy(fan_str, "ON");
    } else {
        strcpy(fan_str, "OFF");
    }

    if (aux1 == 1){
        strcpy(aux1_str, "N");
        _crow_panel_controller->set_label_color(ui_PanelGear, CrowPanelController::COLOR_GOOD);
    } else {
        strcpy(aux1_str, "D");
        _crow_panel_controller->set_label_color(ui_PanelGear, CrowPanelController::COLOR_PANEL_DEFAULT);
    }

    

    _crow_panel_controller->set_string_to_label(ui_shutdown, shut_str);
    _crow_panel_controller->set_string_to_label(ui_fan, fan_str);
    _crow_panel_controller->set_value_to_label(ui_correctionlambda, lmbcorrect);
    _crow_panel_controller->set_value_to_label(ui_auxstatus9, brake);
    _crow_panel_controller->set_string_to_label(ui_gear, aux1_str);
    
    // Shutdown status color
    if (shut == 3) {
        _crow_panel_controller->set_label_color(ui_shutdown, CrowPanelController::COLOR_CRITICAL);  // Red when shutdown is ON (emergency)
    } else {
        _crow_panel_controller->set_label_color(ui_shutdown, CrowPanelController::COLOR_GOOD);      // Green when shutdown is OFF (normal)
    }
    
    // Fan status color
    if (fan == 1) {
        _crow_panel_controller->set_label_color(ui_fan, CrowPanelController::COLOR_BLUE);          // Blue when fan is ON (cooling)
    } else {
        _crow_panel_controller->set_label_color(ui_fan, CrowPanelController::COLOR_NORMAL);        // White when fan is OFF
    }
    
    // Brake pressure color (assuming brake > 0 means brakes applied)
    if (brake > 100) {  // Adjust threshold as needed
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_WARNING); // Yellow for heavy braking
    } else if (brake > 0) {
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_NORMAL);  // White for light braking
    } else {
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_GOOD);    // Green for no braking
    }
}

void DataProcessor::send_serial_frame_3(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1){
    Serial.println("send_serial_frame_3");

    char aux3_str[10];
    char aux4_str[10];
    char aux5_str[10];
    char aux6_str[10];
    char aux7_str[10];
    char aux8_str[10];
    char dig1_str[10];

    if (aux3 == 1){
        strcpy(aux3_str, "ON");
    } else {
        strcpy(aux3_str, "OFF");
    }

    if (aux4 == 1){

        strcpy(aux4_str, "ON");
    } else {
        strcpy(aux4_str, "OFF");
    }

    if (aux5 == 1){
        strcpy(aux5_str, "ON");
    } else {
        strcpy(aux5_str, "OFF");
    }

    if (aux6 == 1){
        strcpy(aux6_str, "ON");
    } else {
        strcpy(aux6_str, "OFF");
    }

    if (aux7 == 1){
        strcpy(aux7_str, "ON");
    } else {
        strcpy(aux7_str, "OFF");
    }

    if (aux8 == 1){
        strcpy(aux8_str, "ON");
    } else {
        strcpy(aux8_str, "OFF");
    }

    if (dig1 == 1){
        strcpy(dig1_str, "ON");
    } else {
        strcpy(dig1_str, "OFF");
    }

    _crow_panel_controller -> set_string_to_label(ui_auxstatus3, aux3_str);
    if(aux3 == 1 && change_screen_requested == false){
        switch(current_display){
            case 0:
                _crow_panel_controller->change_screen(ui_Screen1);
                break;
            case 1:
                _crow_panel_controller->change_screen(ui_Screen2);
                break;
            case 2:
                _crow_panel_controller->change_screen(ui_Screen3);
                break;
            case 3:
                _crow_panel_controller->change_screen(ui_Screen4);
                break;
        }
        current_display++;
        change_screen_requested = true;
        if(current_display > 3){
            current_display = 0;
        }
    }else if(aux3 == 0 && change_screen_requested == true){
        change_screen_requested = false;
    }

    _crow_panel_controller -> set_string_to_label(ui_auxstatus4, aux4_str);
    _crow_panel_controller -> set_string_to_label(ui_auxstatus5, aux5_str);
    _crow_panel_controller -> set_string_to_label(ui_auxstatus6, aux6_str);
    _crow_panel_controller -> set_string_to_label(ui_auxstatus7, aux7_str);
    _crow_panel_controller -> set_string_to_label(ui_auxstatus8, aux8_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus1, dig1_str);
}

void DataProcessor::send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9){
    Serial.println("send_serial_frame_4");

    char dig3_str[10];
    char dig4_str[10];
    char dig5_str[10];
    char dig6_str[10];
    char dig7_str[10];
    char dig8_str[10];
    char dig9_str[10];

    if (dig3 == 1){
        strcpy(dig3_str, "ON");
    } else {
        strcpy(dig3_str, "OFF");
    }

    if (dig4 == 1){
        strcpy(dig4_str, "ON");
    } else {
        strcpy(dig4_str, "OFF");
    }

    if (dig5 == 1){
        strcpy(dig5_str, "ON");
    } else {
        strcpy(dig5_str, "OFF");
    }

    if (dig6 == 1){
        strcpy(dig6_str, "ON");
    } else {
        strcpy(dig6_str, "OFF");
    }

    if (dig7 == 1){
        strcpy(dig7_str, "ON");
    } else {
        strcpy(dig7_str, "OFF");
    }

    if (dig8 == 1){
        strcpy(dig8_str, "ON");
    } else {
        strcpy(dig8_str, "OFF");
    }

    if (dig9 == 1){
        strcpy(dig9_str, "ON");
    } else {
        strcpy(dig9_str, "OFF");
    }

    _crow_panel_controller -> set_string_to_label(ui_digitalstatus3, dig3_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus4, dig4_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus5, dig5_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus6, dig6_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus7, dig7_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus8, dig8_str);
    _crow_panel_controller -> set_string_to_label(ui_digitalstatus9, dig9_str);
}

void DataProcessor::send_serial_screen_test(int test) {
    _crow_panel_controller->set_value_to_label(ui_rpm, test);
    Serial.println(test);
}