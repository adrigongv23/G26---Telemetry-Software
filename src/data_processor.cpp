#include "../include/data_processor.hpp"


//RPM + TPS + vBatt + ECT
void DataProcessor::send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int vbatth, int vbattl, int ect){

    car.ect = ect; // GUARDAR LA VARIABLE DEL FRAME EN LA ESTRUCTURA 
  
  // El resto de variables no son relevantes ahora, se quedan comentadas para ahorrar memoria
  //  int rpm = (rpmh * 256) + rpml; 
  //  int tps = (tpsh * 256) + tpsl; 
  //  double vbatt = ((vbatth * 256) + vbattl) / 100.0; 

    // 2. Escribimos en la SD 
    flushToSD(); 
}



//LAMB + LAMBTRG + FUEL + GEAR
void DataProcessor::send_serial_frame_1(int lmbh, int lmbl, int lmbth, int lmbtl, int fuelh, int fuell, int gear){


  //  int lmb = (lmbh * 256) + lmbl;
  //  int lmbtrg = (lmbth * 256) + lmbtl;
  //  int fuel = (fuelh * 256) + fuell;

}


void DataProcessor::send_serial_frame_2(int shut, int fan, int lmbch, int lmbcl, int brakeh, int brakel, int aux1){
  //  int lmbcorrect = (lmbch * 256) + lmbcl;
  //  int brake = (brakeh * 256) + brakel;

  //  char shut_str[10];
  //  char fan_str[10];
  //  char aux1_str[10];
}
void DataProcessor::send_serial_frame_3(int aux3, int aux4, int aux5, int aux6, int aux7, int aux8, int dig1){

  //  char aux3_str[10];
  //  char aux4_str[10];
  //  char aux5_str[10];
  //  char aux6_str[10];
  //  char aux7_str[10];
  //  char aux8_str[10];
  //  char dig1_str[10];

  //  if (aux3 == 1){
  //      strcpy(aux3_str, "ON");
  //  } else {
  //      strcpy(aux3_str, "OFF");
  //  }

  //  if (aux4 == 1){

  //      strcpy(aux4_str, "ON");
  //  } else {
  //      strcpy(aux4_str, "OFF");
  //  }

  //  if (aux5 == 1){
  //      strcpy(aux5_str, "ON");
  //  } else {
  //      strcpy(aux5_str, "OFF");
  //  }

  //  if (aux6 == 1){
  //      strcpy(aux6_str, "ON");
  //  } else {
  //      strcpy(aux6_str, "OFF");
  //  }

  //  if (aux7 == 1){
  //      strcpy(aux7_str, "ON");
  //  } else {
  //      strcpy(aux7_str, "OFF");
  //  }

  //  if (aux8 == 1){
  //      strcpy(aux8_str, "ON");
  //  } else {
  //      strcpy(aux8_str, "OFF");
  //  }

  //  if (dig1 == 1){
  //      strcpy(dig1_str, "ON");
  //  } else {
  //      strcpy(dig1_str, "OFF");
  //  }
}

void DataProcessor::send_serial_frame_4(int dig3, int dig4, int dig5, int dig6, int dig7, int dig8, int dig9){

  //  char dig3_str[10];
  //  char dig4_str[10];
  //  char dig5_str[10];
  //  char dig6_str[10];
  //  char dig7_str[10];
  //  char dig8_str[10];
  //  char dig9_str[10];

  //  if (dig3 == 1){
  //      strcpy(dig3_str, "ON");
  //  } else {
  //      strcpy(dig3_str, "OFF");
  //  }

  //  if (dig4 == 1){
  //      strcpy(dig4_str, "ON");
  //  } else {
  //      strcpy(dig4_str, "OFF");
  //  }

  //  if (dig5 == 1){
  //      strcpy(dig5_str, "ON");
  //  } else {
  //      strcpy(dig5_str, "OFF");
  //  }

  //  if (dig6 == 1){
  //      strcpy(dig6_str, "ON");
  //  } else {
  //      strcpy(dig6_str, "OFF");
  //  }

  //  if (dig7 == 1){
  //      strcpy(dig7_str, "ON");
  //  } else {
  //      strcpy(dig7_str, "OFF");
  //  }

  //  if (dig8 == 1){
  //      strcpy(dig8_str, "ON");
  //  } else {
  //      strcpy(dig8_str, "OFF");
  //  }

  //  if (dig9 == 1){
  //      strcpy(dig9_str, "ON");
  //  } else {
  //      strcpy(dig9_str, "OFF");
  //  }

}

// ESCRITURA EN SD 

void DataProcessor::flushToSD() {
    // Verificamos que los punteros existan Y que el archivo esté abierto
    if (_sd && _logFile && _logFile->isOpen()) {
            
// 1. Escribimos al BUFFER (RAM) - Esto es instantáneo (microsegundos)
        _logFile->print(millis());      
        _logFile->print(",");
        _logFile->println(car.ect);       

        // 2. SYNC CONTROLADO 
        // Solo obligamos a la tarjeta a "escribir de verdad" si ha pasado X tiempo.
        // Esto evita detener el CAN cada 10ms.
        if (millis() - _last_sync_time > _sync_interval_ms) {
            _logFile->sync(); // Aquí sí gastamos tiempo, pero solo 1 vez por segundo
            _last_sync_time = millis();
            // Serial.println("[SD] Sync realizado"); // Descomentar solo para debug
        }
    }
}
