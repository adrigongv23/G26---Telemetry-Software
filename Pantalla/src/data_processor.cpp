/**
 * @file data_processor.cpp
 * @author Raúl Arcos Herrera
 * @brief This file contains the implementation of the Data Processor class for Link G4+ ECU.
 */

#include "../include/data_processor.hpp"
#include "../include/ajustes.hpp"

// ---------------------------------
static int rpm_max=8000;
static int desp=0;
static bool modo_oscuro=true;
static int pantalla=0;
static int rpm_display=false;
static int diff_rpm=1200;

static float vbatt_too_high=15.0;
static float vbatt_high=14.5;
static float vbatt_low=12.5;

static int water_hot=110;
static int water_mild=100;
static int water_cold=70;

static int oil_hot=110;
static int oil_mild=100;
static int oil_cold=70;

static float oil_pressure_too_high=4.0;
static float oil_pressure_high=3.0;
static float oil_pressure_low=1.0;

static float fuel_pressure_too_high=15.0;
static float fuel_pressure_high=12.0;
static float fuel_pressure_low=3.0;

static const uint32_t UI_RPM_MS = 30;
static const uint32_t UI_SLOW_MS = 100;
static const uint32_t UI_AUX_MS = 200;

static bool every_ms(uint32_t &last_time, uint32_t interval)
{
    uint32_t now = millis();

    if(last_time == 0 || now - last_time >= interval){
        last_time = now;
        return true;
    }

    return false;
}
// --------------------------------

void DataProcessor::send_serial(byte type, unsigned int value) {                     //Como parámetros se pasan el ID (type), que es el ID establecido al inicio del código para el dato que se quiera enviar. Ej: RPM_ID -> 0x51; y se envía el valor de dicho dato.
    byte dato[8] = { 0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00 };  //Se establece un arreglo de bytes con los primeros datos necesarios para que la pantalla lo interprete como mensaje (En la Wiki hay tutoriales que lo explican a fondo), como ser la longitud y el tipo de mensaje.
    dato[4] = type;                                                     //Se configura en el mensaje el ID correspondiente al dato a enviar.
    dato[6] = (value >> 8) & 0xFF;                                      //Se configura el dato en los últimos 2 bytes.
    dato[7] = value & 0xFF;

    Serial.write(dato, 8);                                              //Se envía serialmente el mensaje, indicando su longituden bytes para ello.
}

//RPM + TPS + vBatt + ECT
void DataProcessor::send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect){
    //Serial.println("send_serial_frame_0");
    
    uint32_t color;
    
    int rpm = (rpmh * 256) + rpml; 
    float tps = ((tpsh * 256) + tpsl); 
    float vbatt = ((vbatth * 256.0) + (float)vbattl)/100.0;

    static uint32_t last_rpm_update = 0;
    static uint32_t last_slow_update = 0;

    if(every_ms(last_rpm_update, UI_RPM_MS)){

        _crow_panel_controller->set_value_to_label(ui_rpm, rpm);

        _crow_panel_controller->update_rpm_bar(rpm);
        DataProcessor::update_rpm_leds(rpm);

        _crow_panel_controller->set_value_to_label(ui_throttle, tps);
    }

    if(!every_ms(last_slow_update, UI_SLOW_MS)){
        return;
    }

    _crow_panel_controller->set_value_to_label(ui_vbat, vbatt);
    _crow_panel_controller->set_value_to_label(ui_watert, ect);

    // Battery voltage color (typical car battery: 12.6V resting, 13.2-14.4V running)
    if (vbatt < vbatt_low) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_vbatpanel, color);  // Red for low
    } else if (vbatt < vbatt_high) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_vbatpanel, color);   // Yellow for warning
    } else if (vbatt > vbatt_too_high) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_vbatpanel, color);   // Yellow for overcharge
    } else {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_vbatpanel, color);      // Green for good
    }
    

    // Engine coolant temperature (typical range: 80-105°C normal operating temp)
    if (ect > water_hot) {
        // Crítico: Rojo
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_watertpanel, color);
        _crow_panel_controller->hide_label(ui_warmuppanel);
        _crow_panel_controller->hide_label(ui_warmup);
    } else if (ect >= water_mild) {
        // Advertencia: Amarillo (100 a 110)
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_watertpanel, color);
        _crow_panel_controller->hide_label(ui_warmuppanel);
        _crow_panel_controller->hide_label(ui_warmup);
    } else if (ect >= water_cold) {
        //Temperatura Ideal: Verde (70 a 100)
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_watertpanel,  color);
        _crow_panel_controller->hide_label(ui_warmuppanel);
        _crow_panel_controller->hide_label(ui_warmup);
    } else { // etc <= 70 Azul
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AZUL_NUEVO : CrowPanelController::COLOR_AZUL_CLARO;
        _crow_panel_controller->show_label(ui_warmuppanel);
        _crow_panel_controller->show_label(ui_warmup);
        _crow_panel_controller->set_panel_color(ui_watertpanel, color);
    }
}

//LAMB + LAMBTRG + FUEL + GEAR
void DataProcessor::send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear){
    //Serial.println("send_serial_frame_1");

    static uint32_t last_slow_update = 0;

    if(!every_ms(last_slow_update, UI_SLOW_MS)){
        return;
    }

    uint32_t color;

   double lmb = ((lmbh * 256) + lmbl) / 100.0;
   double lmbtrg = ((lmbth * 256) + lmbtl) / 100.0;
   double fuel = ((fuelh * 256) + fuell)/100.0;
   _crow_panel_controller->set_value_to_label(ui_lambda, lmb);               
   _crow_panel_controller->set_value_to_label(ui_lambdatrg, lmbtrg);      
   _crow_panel_controller->set_value_to_label(ui_fuelp, fuel);
   //_crow_panel_controller->set_value_to_label(ui_neutral, gear);

    if (fuel > fuel_pressure_too_high) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_fuelppanel, color);
    } else if (fuel >= fuel_pressure_high) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_fuelppanel, color);
    } else if (fuel >= fuel_pressure_low) {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_fuelppanel,  color);
    } else {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_fuelppanel, color);
    }

    if(lmb > 1.1){
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_lambdapanel, color);
    } else {
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_lambdapanel, color);
    }
}


void DataProcessor::send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1){
    //Serial.println("send_serial_frame_2");

    static uint32_t last_aux_update = 0;

    if(!every_ms(last_aux_update, UI_AUX_MS)){
        return;
    }

    double lmbcorrect = ((lmbch * 256) + lmbcl) / 100.0;
    double brake = ((brakeh * 256) + brakel);

    char shut_str[4];
    char fan_str[4];
    char aux1_str[4];

    uint32_t color;

    if (shut == 1){
        strcpy(shut_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_sdownpanel, color);
    } else {
        strcpy(shut_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_sdownpanel, color);
    }

    if (fan == 1){
        strcpy(fan_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_fanpanel, color);
    } else {
        strcpy(fan_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_fanpanel, color);
    }

    if (aux1 == 0){
        strcpy(aux1_str, "N");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->show_label(ui_neutralpanel);
        _crow_panel_controller->set_panel_color(ui_neutralpanel, color);
    } else {
        strcpy(aux1_str, "D");
        _crow_panel_controller->hide_label(ui_neutralpanel);
    }

    _crow_panel_controller->set_string_to_label(ui_sdown, shut_str);
    _crow_panel_controller->set_string_to_label(ui_fan, fan_str);
    // _crow_panel_controller->set_value_to_label(ui_correctionlambda, lmbcorrect);     NO HAY AHORA MISMO
    _crow_panel_controller->set_value_to_label(ui_brake, brake);
    _crow_panel_controller->set_string_to_label(ui_neutral, aux1_str);
    
    // Brake pressure color (assuming brake > 0 means brakes applied)
    /*
    if (brake > 100) {  // Adjust threshold as needed
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_WARNING); // Yellow for heavy braking
    } else if (brake > 0) {
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_NORMAL);  // White for light braking
    } else {
        _crow_panel_controller->set_label_color(ui_auxstatus9, CrowPanelController::COLOR_GOOD);    // Green for no braking
    }
               TODO COMENTADO PORQUE LO ULTIMO NO VEO UTILIDAD Y LAS 2 COSAS PRIMERAS ESTAN OPTIMIZADAS PAR DE LINEAS ARRIBAS
    */
          
}


void DataProcessor::send_serial_frame_3(int oilth, int oiltl, int oilph, int oilpl, int maph, int mapl, int useless) {

    float oilt = ((oilth * 256) + oiltl) / 100.0;
    float oilp = ((oilph * 256) + oilpl) / 100.0;
    float map = ((maph * 256) + mapl) / 100.0;

    uint32_t color;

    // Temperatura del aceite
    if (oilt > oil_hot) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_oiltpanel, color);
    } else if (oilt >= oil_mild) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_oiltpanel, color);
    } else if (oilt >= oil_cold) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_oiltpanel, color);
    } else {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_oiltpanel, color);
    }

    // Presión del aceite
    if (oilp > oil_pressure_too_high) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_oilppanel, color);
    } else if (oilp >= oil_pressure_high) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
        _crow_panel_controller->set_panel_color(ui_oilppanel, color);
    } else if (oilp >= oil_pressure_low) {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_oilppanel, color);
    } else {
        color = (ui_theme_idx == 1) ? CrowPanelController::COLOR_AZUL_NUEVO : CrowPanelController::COLOR_AZUL_CLARO;
        _crow_panel_controller->set_panel_color(ui_oilppanel, color);
    }

    _crow_panel_controller->set_value_to_label(ui_oilt, oilt);
    _crow_panel_controller->set_value_to_label(ui_oilp, oilp);
    _crow_panel_controller->set_value_to_label(ui_map, map);

}


void DataProcessor::send_serial_frame_5(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1){
    Serial.println("send_serial_frame_5");

    uint32_t color;

    char aux3_str[4];
    char aux4_str[4];
    char aux5_str[4];
    char aux6_str[4];
    char aux7_str[4];
    char aux8_str[4];
    char dig1_str[4];

    if (aux3 == 1){     // Cambio de pantalla
        strcpy(aux3_str, "ON");
        _crow_panel_controller->change_screen();
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux3panel, color);
    } else {
        strcpy(aux3_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux3panel, color);
    }

    if (aux4 == 1){     
        strcpy(aux4_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux4panel, color);
    } else {
        strcpy(aux4_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux4panel, color);
    }

    if (aux5 == 1){     
        strcpy(aux5_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux5panel, color);
    } else {
        strcpy(aux5_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux5panel, color);
    }

    if (aux6 == 1){     
        strcpy(aux6_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux6panel, color);
    } else {
        strcpy(aux6_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux6panel, color);
    }

    if(aux7==1){        
        strcpy(aux7_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux7panel, color);
    } else {
        strcpy(aux7_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux7panel, color);
    }

    if(aux8==1){        
        strcpy(aux8_str, "ON");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux8panel, color);
    } else {
        strcpy(aux8_str, "OFF");
        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
        _crow_panel_controller->set_panel_color(ui_aux8panel, color);
    }

    //_led_strip->launch_control(aux6);

    _crow_panel_controller -> set_string_to_label(ui_aux3, aux3_str);
    _crow_panel_controller -> set_string_to_label(ui_aux4, aux4_str);
    _crow_panel_controller -> set_string_to_label(ui_aux5, aux5_str);
    _crow_panel_controller -> set_string_to_label(ui_aux6, aux6_str);
    _crow_panel_controller -> set_string_to_label(ui_aux7, aux7_str);
    _crow_panel_controller -> set_string_to_label(ui_aux8, aux8_str);
    _crow_panel_controller -> set_string_to_label(ui_dig1, dig1_str);
    
}

void DataProcessor::send_serial_frame_volante(int neutra, int cpant, int arrancar, int launch, int levaizq, int levader, int aux){
/*
    char neutra_str[4];
    char cpant_str[4];
    char arrancar_str[4];
    char launch_str[4];
    char levaizq_str[4];
    char levader_str[4];

    if (neutra== 1){     // Neutra
        strcpy(neutra_str, "ON");
        _crow_panel_controller->set_panel_color(ui_pneutra, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(neutra_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_pneutra, CrowPanelController::COLOR_ROJO);
    }

    if (cpant == 1){     // Cambio de pantalla
        if(request_screen==0){
            _crow_panel_controller->change_screen();
        }
        strcpy(cpant_str, "ON");
        _crow_panel_controller->set_panel_color(ui_ppantalla, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(cpant_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_ppantalla, CrowPanelController::COLOR_ROJO);
    }

    request_screen=cpant;

    if (arrancar == 1){     // Arrancar
        strcpy(arrancar_str, "ON");
        _crow_panel_controller->set_panel_color(ui_parrancar, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(arrancar_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_parrancar, CrowPanelController::COLOR_ROJO);
    }

    if (launch == 1){     // Launch
        strcpy(launch_str, "ON");
        _crow_panel_controller->set_panel_color(ui_plaunch, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(launch_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_plaunch, CrowPanelController::COLOR_ROJO);
    }

    if(levaizq==1){        // Leva izquierda
        strcpy(levaizq_str, "ON");
        _crow_panel_controller->set_panel_color(ui_plevaizq, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(levaizq_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_plevaizq, CrowPanelController::COLOR_ROJO);
    }

    if(levader==1){        // Leva derecha
        strcpy(levader_str, "ON");
        _crow_panel_controller->set_panel_color(ui_plevader, CrowPanelController::COLOR_VERDE);
    } else {
        strcpy(levader_str, "OFF");
        _crow_panel_controller->set_panel_color(ui_plevader, CrowPanelController::COLOR_ROJO);
    }

    _led_strip->launch_control(launch);

    _crow_panel_controller -> set_string_to_label(ui_neutra, neutra_str);
    _crow_panel_controller -> set_string_to_label(ui_pantalla, cpant_str);
    _crow_panel_controller -> set_string_to_label(ui_arrancar, arrancar_str);
    _crow_panel_controller -> set_string_to_label(ui_launch, launch_str);
    _crow_panel_controller -> set_string_to_label(ui_levaizq, levaizq_str);
    _crow_panel_controller -> set_string_to_label(ui_levader, levader_str);

    */

}

void DataProcessor::set_dataprocessor(){

    rpm_max=get_rpm();
    desp=get_desplazamiento();
    modo_oscuro=get_modo_oscuro();
    pantalla=get_pantalla();
    rpm_display=get_rpm_display();
    diff_rpm=get_diff_rpm();

    vbatt_too_high=get_vbatt_too_high();
    vbatt_high=get_vbatt_high();
    vbatt_low=get_vbatt_low();

    water_hot=get_water_hot();
    water_mild=get_water_mild();
    water_cold=get_water_cold();

    oil_hot=get_oil_hot();
    oil_mild=get_oil_mild();
    oil_cold=get_oil_cold();

    oil_pressure_too_high=get_oil_pressure_too_high();
    oil_pressure_high=get_oil_pressure_high();
    oil_pressure_low=get_oil_pressure_low();

    fuel_pressure_too_high=get_fuel_pressure_too_high();
    fuel_pressure_high=get_fuel_pressure_high();
    fuel_pressure_low=get_fuel_pressure_low();

}

void DataProcessor::update_rpm_leds(int rpm){

    static int last_leds = -1;
    static int last_theme = -1;
    static bool last_over_rpm = false;

    static lv_obj_t* rpm_bars[15] = {
        ui_led0, ui_led1, ui_led2, ui_led3, ui_led4,
        ui_led5, ui_led6, ui_led7, ui_led8, ui_led9,
        ui_led10, ui_led11, ui_led12, ui_led13, ui_led14
    };

    int rpm_inicio = rpm_max - diff_rpm;

    if(rpm_inicio < 0){
        rpm_inicio = 0;
    }

    int leds = 0;
    bool over_rpm = rpm >= rpm_max;

    if(rpm >= rpm_max){
        leds = 15;
    } else if(rpm > rpm_inicio){
        int rango = rpm_max - rpm_inicio;

        if(rango > 0){
            leds = ((rpm - rpm_inicio) * 15) / rango;
        }

        if(leds < 0){
            leds = 0;
        } else if(leds > 15){
            leds = 15;
        }
    }

    if(leds == last_leds && ui_theme_idx == last_theme && over_rpm == last_over_rpm){
        return;
    }

    uint32_t color;

    if(ui_theme_idx != last_theme || last_leds == -1 || over_rpm != last_over_rpm){

        for(int i=0;i<15;i++){

            if(i<leds){

                if(over_rpm){
                    color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AZUL_NUEVO : CrowPanelController::COLOR_AZUL_CLARO;
                } else if(i<=4){
                    color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
                } else if(i<=10){
                    color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
                } else {
                    color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
                }

            } else {

                color=(ui_theme_idx==1) ? CrowPanelController::COLOR_FONDO_BARRA_OSCURO : CrowPanelController::COLOR_FONDO_BARRA_CLARO;
            }

            _crow_panel_controller->set_panel_color(rpm_bars[i], color);
        }

    } else if(leds > last_leds){

        for(int i=last_leds;i<leds;i++){

            if(over_rpm){
                color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AZUL_NUEVO : CrowPanelController::COLOR_AZUL_CLARO;
            } else if(i<=4){
                color=(ui_theme_idx==1) ? CrowPanelController::COLOR_VERDE_NUEVO : CrowPanelController::COLOR_VERDE_CLARO;
            } else if(i<=10){
                color=(ui_theme_idx==1) ? CrowPanelController::COLOR_AMARILLO_NUEVO : CrowPanelController::COLOR_AMARILLO_CLARO;
            } else {
                color=(ui_theme_idx==1) ? CrowPanelController::COLOR_ROJO_NUEVO : CrowPanelController::COLOR_ROJO_CLARO;
            }

            _crow_panel_controller->set_panel_color(rpm_bars[i], color);
        }

    } else {

        color=(ui_theme_idx==1) ? CrowPanelController::COLOR_FONDO_BARRA_OSCURO : CrowPanelController::COLOR_FONDO_BARRA_CLARO;

        for(int i=leds;i<last_leds;i++){
            _crow_panel_controller->set_panel_color(rpm_bars[i], color);
        }
    }

    last_leds = leds;
    last_theme = ui_theme_idx;
    last_over_rpm = over_rpm;
}

