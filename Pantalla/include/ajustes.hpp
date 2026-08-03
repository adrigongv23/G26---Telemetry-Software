#ifndef AJUSTES_HPP
#define AJUSTES_HPP

#include "../include/guardado.hpp"

void set_data(data *datos);

int get_rpm();
int get_desplazamiento();
bool get_modo_oscuro();

int get_pantalla();
int get_rpm_display();
int get_diff_rpm();
int get_recepcion_datos();

float get_vbatt_too_high();
float get_vbatt_high();
float get_vbatt_low();

int get_water_hot();
int get_water_mild();
int get_water_cold();

int get_oil_hot();
int get_oil_mild();
int get_oil_cold();

float get_oil_pressure_too_high();
float get_oil_pressure_high();
float get_oil_pressure_low();

float get_fuel_pressure_too_high();
float get_fuel_pressure_high();
float get_fuel_pressure_low();

void save();
void load();

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
);

int modify_value(int value, int modifier);
float modify_value(float value, float modifier);

#endif