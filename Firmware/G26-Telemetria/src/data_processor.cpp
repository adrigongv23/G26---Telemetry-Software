#include "../include/data_processor.hpp"


char* DataProcessor::process(std::vector<float> data) {
    // Implementación del procesamiento de datos si es necesario
    return nullptr; // Placeholder
}

void DataProcessor::send_serial(byte type, unsigned int value) {                     //Como parámetros se pasan el ID (type), que es el ID establecido al inicio del código para el dato que se quiera enviar. Ej: RPM_ID -> 0x51; y se envía el valor de dicho dato.
    byte dato[8] = { 0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00 };  //Se establece un arreglo de bytes con los primeros datos necesarios para que la pantalla lo interprete como mensaje (En la Wiki hay tutoriales que lo explican a fondo), como ser la longitud y el tipo de mensaje.
    dato[4] = type;                                                     //Se configura en el mensaje el ID correspondiente al dato a enviar.
    dato[6] = (value >> 8) & 0xFF;                                      //Se configura el dato en los últimos 2 bytes.
    dato[7] = value & 0xFF;

    Serial.write(dato, 8);                                              //Se envía serialmente el mensaje, indicando su longituden bytes para ello.
}

//RPM + TPS + vBatt + ECT
void DataProcessor::send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect){

   //Calculos necesarios para obtener bien el formato de los valores necesarios
    int rpm = (rpmh * 256) + rpml;
    double vbatt = ((vbatth * 256) + vbattl) / 100.0;
    int tps = (tpsh * 256) + tpsl; 

    //Serial.printf("TRAMA: 0\n");
    //Serial.printf("RPM: %d | VBATT: %f | TPS: %d | ECT: %d\n", rpm, vbatt, tps, ect);

    // Actualizamos las variables globales para que puedan ser leidas por el protocolo UDP
    this->current_ect_value = ect;
    this->current_rpm_value = rpm;
    this->current_vbatt_value = vbatt;
    this->current_tps_value = tps;
}


void DataProcessor::send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear){
    float lambda = ((lmbh * 256) + lmbl) / 100.0;
    float lambdaTarget = ((lmbth * 256) + lmbtl) / 100.0;
    float presionComb = ((fuelh * 256) + fuell) / 100.0;

    //Serial.printf("TRAMA: 1\n");
    //Serial.printf("LAMBDA: %f | LAMBDA TARGET: %f | PRESION COMBUSTIBLE: %f\n", lambda, lambdaTarget, presionComb);

    this->current_lambda_value = lambda;
    this->current_lambda_obj_value = lambdaTarget;
    this->current_pcomb_value = presionComb;
    //this->current_marcha_value
}

void DataProcessor::send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1){

}

void DataProcessor::send_serial_frame_3(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1){
    float tempOil = ((oilth * 256) + oiltl) / 100.0;
    float presionOil = ((oilph * 256) + oilpl) / 100.0;
    float map = ((maph * 256) + mapl) / 100.0;

    //Serial.printf("TRAMA: 2\n");
    //Serial.printf("TEMP OIL: %f | PRESION OIL: %f | MAP: %f\n", tempOil, presionOil, map);

    this->current_taceite_value = tempOil;
    this->current_paceite_value = presionOil;
    this->current_map_value = map;

}

void DataProcessor::send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9){

}
