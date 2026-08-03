#include "../include/guardado.hpp"
#include <LittleFS.h>
#include <Arduino.h>

void data::save_data(){

    File archivo=LittleFS.open(nombre_archivo, "w");

    if(!archivo){
        Serial.println("Archivo escritura no abierto");
        return;
    }

    archivo.printf("RPM-%d\n", rpm_max);
    archivo.printf("DESP-%d\n", desplazamiento);
    archivo.printf("MODO-%d\n", modo_oscuro ? 1 : 0);
    archivo.printf("PANTALLA-%d\n", pantalla);
    archivo.printf("RPM_DISPLAY-%d\n", rpm_display);
    archivo.printf("DIFF_RPM-%d\n", diff_rpm);

    archivo.printf("VBATT_TOO_HIGH-%.2f\n", vbatt_too_high);
    archivo.printf("VBATT_HIGH-%.2f\n", vbatt_high);
    archivo.printf("VBATT_LOW-%.2f\n", vbatt_low);

    archivo.printf("WATER_HOT-%d\n", water_hot);
    archivo.printf("WATER_MILD-%d\n", water_mild);
    archivo.printf("WATER_COLD-%d\n", water_cold);

    archivo.printf("OIL_HOT-%d\n", oil_hot);
    archivo.printf("OIL_MILD-%d\n", oil_mild);
    archivo.printf("OIL_COLD-%d\n", oil_cold);

    archivo.printf("OIL_PRESSURE_TOO_HIGH-%.2f\n", oil_pressure_too_high);
    archivo.printf("OIL_PRESSURE_HIGH-%.2f\n", oil_pressure_high);
    archivo.printf("OIL_PRESSURE_LOW-%.2f\n", oil_pressure_low);

    archivo.printf("FUEL_PRESSURE_TOO_HIGH-%.2f\n", fuel_pressure_too_high);
    archivo.printf("FUEL_PRESSURE_HIGH-%.2f\n", fuel_pressure_high);
    archivo.printf("FUEL_PRESSURE_LOW-%.2f\n", fuel_pressure_low);

    archivo.printf("RECEPCION_DATOS-%d\n", recepcion_datos);

    archivo.close();

}

void data::load_data(){

    File archivo=LittleFS.open(nombre_archivo, "r");

    if(!archivo){
        Serial.println("Archivo lectura no abierto");

        rpm_max=8000;
        desplazamiento=0;
        modo_oscuro=1;
        pantalla=0;
        rpm_display=0;
        diff_rpm=1200;
        recepcion_datos=0;

        vbatt_too_high=15.0;
        vbatt_high=14.5;
        vbatt_low=12.5;

        water_hot=110;
        water_mild=100;
        water_cold=70;

        oil_hot=110;
        oil_mild=100;
        oil_cold=70;

        oil_pressure_too_high=4.0;
        oil_pressure_high=3.0;
        oil_pressure_low=1.0;

        fuel_pressure_too_high=15.0;
        fuel_pressure_high=12.0;
        fuel_pressure_low=3.0;

        return;
    }

    int cantidad=0;

    while(archivo.available() && cantidad<22){
        String linea=archivo.readStringUntil('\n');
        int guion=linea.indexOf('-');

        if(guion<0){
            continue;
        }

        String clave=linea.substring(0, guion);
        float valor=linea.substring(guion+1).toFloat();

        if(clave=="RPM") rpm_max=(int)valor;
        else if(clave=="DESP") desplazamiento=(int)valor;
        else if(clave=="MODO") modo_oscuro=(valor!=0);
        else if(clave=="PANTALLA") pantalla=(int)valor;
        else if(clave=="RPM_DISPLAY") rpm_display=(int)valor;
        else if(clave=="DIFF_RPM") diff_rpm=(int)valor;
        else if(clave=="RECEPCION_DATOS") recepcion_datos=(int)valor;
        else if(clave=="VBATT_TOO_HIGH") vbatt_too_high=valor;
        else if(clave=="VBATT_HIGH") vbatt_high=valor;
        else if(clave=="VBATT_LOW") vbatt_low=valor;
        else if(clave=="WATER_HOT") water_hot=(int)valor;
        else if(clave=="WATER_MILD") water_mild=(int)valor;
        else if(clave=="WATER_COLD") water_cold=(int)valor;
        else if(clave=="OIL_HOT") oil_hot=(int)valor;
        else if(clave=="OIL_MILD") oil_mild=(int)valor;
        else if(clave=="OIL_COLD") oil_cold=(int)valor;
        else if(clave=="OIL_PRESSURE_TOO_HIGH") oil_pressure_too_high=valor;
        else if(clave=="OIL_PRESSURE_HIGH") oil_pressure_high=valor;
        else if(clave=="OIL_PRESSURE_LOW") oil_pressure_low=valor;
        else if(clave=="FUEL_PRESSURE_TOO_HIGH") fuel_pressure_too_high=valor;
        else if(clave=="FUEL_PRESSURE_HIGH") fuel_pressure_high=valor;
        else if(clave=="FUEL_PRESSURE_LOW") fuel_pressure_low=valor;

        cantidad++;
    }

    if(recepcion_datos<0 || recepcion_datos>1){
        recepcion_datos=0;
    }

    archivo.close();
}

void data::update_data(
    int rpm_maxn,
    int desplazamienton,
    bool modo_oscuron,
    int pantallan,
    int rpm_displayn,
    int diff_rpmn,
    int recepcion_datosn,
    float vbatt_too_highn,
    float vbatt_highn,
    float vbatt_lown,
    int water_hotn,
    int water_mildn,
    int water_coldn,
    int oil_hotn,
    int oil_mildn,
    int oil_coldn,
    float oil_pressure_too_highn,
    float oil_pressure_highn,
    float oil_pressure_lown,
    float fuel_pressure_too_highn,
    float fuel_pressure_highn,
    float fuel_pressure_lown
){

    rpm_max=rpm_maxn;
    desplazamiento=desplazamienton;
    modo_oscuro=modo_oscuron;
    pantalla=pantallan;
    rpm_display=rpm_displayn;
    diff_rpm=diff_rpmn;
    recepcion_datos=recepcion_datosn;

    vbatt_too_high=vbatt_too_highn;
    vbatt_high=vbatt_highn;
    vbatt_low=vbatt_lown;

    water_hot=water_hotn;
    water_mild=water_mildn;
    water_cold=water_coldn;

    oil_hot=oil_hotn;
    oil_mild=oil_mildn;
    oil_cold=oil_coldn;

    oil_pressure_too_high=oil_pressure_too_highn;
    oil_pressure_high=oil_pressure_highn;
    oil_pressure_low=oil_pressure_lown;

    fuel_pressure_too_high=fuel_pressure_too_highn;
    fuel_pressure_high=fuel_pressure_highn;
    fuel_pressure_low=fuel_pressure_lown;

}

int data::get_rpm(){
    return rpm_max;
}

int data::get_desplazamiento(){
    return desplazamiento;
}

bool data::get_modo_oscuro(){
    return modo_oscuro;
}

int data::get_pantalla(){
    return pantalla;
}

int data::get_rpm_display(){
    return rpm_display;
}

int data::get_diff_rpm(){
    return diff_rpm;
}

float data::get_vbatt_too_high(){
    return vbatt_too_high;
}

float data::get_vbatt_high(){
    return vbatt_high;
}

float data::get_vbatt_low(){
    return vbatt_low;
}

int data::get_water_hot(){
    return water_hot;
}

int data::get_water_mild(){
    return water_mild;
}

int data::get_water_cold(){
    return water_cold;
}

int data::get_oil_hot(){
    return oil_hot;
}

int data::get_oil_mild(){
    return oil_mild;
}

int data::get_oil_cold(){
    return oil_cold;
}

float data::get_oil_pressure_too_high(){
    return oil_pressure_too_high;
}

float data::get_oil_pressure_high(){
    return oil_pressure_high;
}

float data::get_oil_pressure_low(){
    return oil_pressure_low;
}

float data::get_fuel_pressure_too_high(){
    return fuel_pressure_too_high;
}

float data::get_fuel_pressure_high(){
    return fuel_pressure_high;
}

float data::get_fuel_pressure_low(){
    return fuel_pressure_low;
}
int data::get_recepcion_datos(){
    return recepcion_datos;
}
