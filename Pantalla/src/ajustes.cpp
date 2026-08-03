#include "../include/ajustes.hpp"

data *puntero_datos;

void set_data(data *datos){
    puntero_datos=datos;
}

int get_rpm(){
    return puntero_datos->get_rpm();
}

int get_desplazamiento(){
    return puntero_datos->get_desplazamiento();
}

bool get_modo_oscuro(){
    return puntero_datos->get_modo_oscuro();
}

int get_pantalla(){
    return puntero_datos->get_pantalla();
}

int get_rpm_display(){
    return puntero_datos->get_rpm_display();
}

int get_diff_rpm(){
    return puntero_datos->get_diff_rpm();
}

int get_recepcion_datos(){
    return puntero_datos->get_recepcion_datos();
}

float get_vbatt_too_high(){
    return puntero_datos->get_vbatt_too_high();
}

float get_vbatt_high(){
    return puntero_datos->get_vbatt_high();
}

float get_vbatt_low(){
    return puntero_datos->get_vbatt_low();
}

int get_water_hot(){
    return puntero_datos->get_water_hot();
}

int get_water_mild(){
    return puntero_datos->get_water_mild();
}

int get_water_cold(){
    return puntero_datos->get_water_cold();
}

int get_oil_hot(){
    return puntero_datos->get_oil_hot();
}

int get_oil_mild(){
    return puntero_datos->get_oil_mild();
}

int get_oil_cold(){
    return puntero_datos->get_oil_cold();
}

float get_oil_pressure_too_high(){
    return puntero_datos->get_oil_pressure_too_high();
}

float get_oil_pressure_high(){
    return puntero_datos->get_oil_pressure_high();
}

float get_oil_pressure_low(){
    return puntero_datos->get_oil_pressure_low();
}

float get_fuel_pressure_too_high(){
    return puntero_datos->get_fuel_pressure_too_high();
}

float get_fuel_pressure_high(){
    return puntero_datos->get_fuel_pressure_high();
}

float get_fuel_pressure_low(){
    return puntero_datos->get_fuel_pressure_low();
}

void save(){
    puntero_datos->save_data();
}

void load(){
    puntero_datos->load_data();
}

void update(
    int rpm_max,
    int desplazamiento,
    bool modo_oscuro,
    int pantalla,
    int rpm_display,
    int diff_rpm,
    int recepcion_datos,
    float vbatt_too_high,
    float vbatt_high,
    float vbatt_low,
    int water_hot,
    int water_mild,
    int water_cold,
    int oil_hot,
    int oil_mild,
    int oil_cold,
    float oil_pressure_too_high,
    float oil_pressure_high,
    float oil_pressure_low,
    float fuel_pressure_too_high,
    float fuel_pressure_high,
    float fuel_pressure_low
){
    puntero_datos->update_data(
        rpm_max,
        desplazamiento,
        modo_oscuro,
        pantalla,
        rpm_display,
        diff_rpm,
        recepcion_datos,
        vbatt_too_high,
        vbatt_high,
        vbatt_low,
        water_hot,
        water_mild,
        water_cold,
        oil_hot,
        oil_mild,
        oil_cold,
        oil_pressure_too_high,
        oil_pressure_high,
        oil_pressure_low,
        fuel_pressure_too_high,
        fuel_pressure_high,
        fuel_pressure_low
    );
}

int modify_value(int value, int modifier){
    return value+modifier;
}

float modify_value(float value, float modifier){
    return value+modifier;
}