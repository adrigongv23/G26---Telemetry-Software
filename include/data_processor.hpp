#ifndef DATAPROCESSOR_HPP
#define DATAPROCESSOR_HPP

#include "common/common_libraries.hpp"

class DataProcessor {
public:
    DataProcessor() = default;

    // Estructura de datos
    struct CarState {
        int ect = 0;     // Temperatura (IMPORTANTE POR AHORA). Luego habría que añadir aquí todas las variables que se vallan a guardar.
    };

    // Configuración SD
    void setLogSystem(SdFat* sd_inst, SdFile* file_inst) {
        _sd = sd_inst;
        _logFile = file_inst;
    }

    void send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect);

    void send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear);
    void send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1);
    void send_serial_frame_3(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1);
    void send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9);

private:
    SdFat* _sd;
    SdFile* _logFile;
    CarState car;

    uint32_t _last_sync_time = 0; 
    const uint32_t _sync_interval_ms = 1000; // Guardar físicamente cada 1 segundo (ajustable)
                                             // Esto optimiza tiempos. Los datos se guardan a la velocidad del CAN en RAM y cada 1 segundo se vuelcan a la SD. Cosa mucho más optima ya que la SD es relativamente lenta comparada al CAN

    void flushToSD(); // Guardado físico
};

#endif
