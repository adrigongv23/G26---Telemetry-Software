#include "../include/data_processor.hpp"

char* DataProcessor::process(std::vector<float> data) {
    return nullptr;
}

void DataProcessor::send_serial(byte type, unsigned int value) {
    byte dato[8] = { 0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00 };
    dato[4] = type;
    dato[6] = (value >> 8) & 0xFF;
    dato[7] = value & 0xFF;
    Serial.write(dato, 8);
}

// RPM + TPS + vBatt + ECT
void DataProcessor::send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect) {
    int rpm = (rpmh * 256) + rpml;
    double vbatt = ((vbatth * 256) + vbattl) / 100.0;
    int tps = (tpsh * 256) + tpsl;

    this->current_ect_value = ect;
    this->current_rpm_value = rpm;
    this->current_vbatt_value = vbatt;
    this->current_tps_value = tps;

    flushToSD();
}

void DataProcessor::send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear) {
    float lambda = ((lmbh * 256) + lmbl) / 100.0;
    float lambdaTarget = ((lmbth * 256) + lmbtl) / 100.0;
    float presionComb = ((fuelh * 256) + fuell) / 100.0;

    this->current_lambda_value = lambda;
    this->current_lambda_obj_value = lambdaTarget;
    this->current_pcomb_value = presionComb;
    flushToSD();

}

void DataProcessor::send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1) {
    float freno = (brakeh * 256) + brakel;
    this->current_freno_del_value = freno;
    flushToSD();

}

void DataProcessor::send_serial_frame_3(int oilth, int oiltl, int oilph, int oilpl, int maph, int mapl, int dig1) {
    float tempOil = ((oilth * 256) + oiltl) / 100.0;
    float presionOil = ((oilph * 256) + oilpl) / 100.0;
    float map = ((maph * 256) + mapl) / 100.0;

    this->current_taceite_value = tempOil;
    this->current_paceite_value = presionOil;
    this->current_map_value = map;
    flushToSD();

}

void DataProcessor::send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9) {
}

// --- ESCRITURA SD ---

void DataProcessor::flushToSD() {
    if (_sd && _logFile && _logFile->isOpen()) {
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer),
            "%lu,%d,%d,%.1f,%.2f,%.1f,%.2f,%.1f,%.2f,%.1f,%.3f,%.3f\n",
            millis(),
            current_ect_value,
            current_rpm_value,
            current_tps_value,
            current_vbatt_value,
            current_freno_del_value,
            current_pcomb_value,
            current_taceite_value,
            current_paceite_value,
            current_map_value,
            current_lambda_value,
            current_lambda_obj_value);

        _logFile->write(buffer, len);

        if (millis() - _last_sync_time > _sync_interval_ms) {
            _logFile->sync();
            _last_sync_time = millis();
        }
    }
}
