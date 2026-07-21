#ifndef DATAPROCESSOR_HPP
#define DATAPROCESSOR_HPP

#include "common_libraries.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


class DataProcessor {
public:
    DataProcessor() = default;

    //Variable publica para el CAN
    volatile int current_ect_value = 0; 
    volatile int current_rpm_value = 0;
    volatile float current_vbatt_value = 0.0;

    //Métodos de recepción de CAN
   
    void send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect);
    void send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear);
    void send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1);
    void send_serial_frame_3(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1);
    void send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9);
    

    //Métodos extras
    void send_serial(byte type, unsigned int value);

    char* process(std::vector<float> data);

private:
};

#endif