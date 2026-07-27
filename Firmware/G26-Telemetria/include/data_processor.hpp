#ifndef DATAPROCESSOR_HPP
#define DATAPROCESSOR_HPP

#include "common_libraries.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class DataProcessor {
public:
    DataProcessor() = default;

    // Variables CAN (actualizadas por los frames)
    volatile int current_ect_value = 0;
    volatile int current_rpm_value = 0;
    volatile float current_vbatt_value = 0.0;

    volatile float current_tps_value = 0.0;
    volatile float current_freno_del_value = 0.0;
    volatile float current_pcomb_value = 0.0;
    volatile float current_taceite_value = 0.0;
    volatile float current_paceite_value = 0.0;
    volatile float current_map_value = 0.0;
    volatile float current_lambda_value = 0.0;
    volatile float current_lambda_obj_value = 0.0;

    // Pendientes de instalar
    volatile float current_freno_tra_value = 0.0;
    volatile float current_velocidad_value = 0.0;

    // Configuracion SD
    void setLogSystem(SdFat* sd_inst, SdFile* file_inst) {
        _sd = sd_inst;
        _logFile = file_inst;
    }

    // Metodos CAN
    void send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect);
    void send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear);
    void send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1);
    void send_serial_frame_3(int oilth, int oiltl, int oilph, int oilpl, int maph, int mapl, int dig1);
    void send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9);

    void send_serial(byte type, unsigned int value);

    char* process(std::vector<float> data);

private:
    SdFat* _sd = nullptr;
    SdFile* _logFile = nullptr;

    uint32_t _last_sync_time = 0;
    const uint32_t _sync_interval_ms = 1000;

    void flushToSD();
};

#endif
