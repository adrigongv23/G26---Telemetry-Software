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
   
    void send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int ecth, int ectl, int gear);
    //void send_serial_frame_1(int lfws, int rfws, int lrws, int rrws, int maph, int mapl, int ect);
    //void send_serial_frame_2(int lambh, int lambl, int lamth, int lamtl, int bvolth, int bvoltl, int iat);
    //void send_serial_frame_3(int aux1, int aux2, int aux3, int aux4, int aux5, int aux6, int aux7);
    //void send_serial_frame_4(int aux1, int aux2, int aux3, int aux4, int aux5, int aux6, int aux7);
    

    //Métodos extras
    void send_serial(byte type, unsigned int value);

    char* process(std::vector<float> data);

private:
};

#endif