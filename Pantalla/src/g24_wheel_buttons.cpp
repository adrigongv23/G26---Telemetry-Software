#include "../include/g24_wheel_buttons.hpp"


    void G24WheelButtons::begin() {
    pinMode(B1_PIN, INPUT_PULLUP); // Arriba izquierda, funcion: neutro
    pinMode(B2_PIN, INPUT_PULLUP); // Arriba derecha, funcion: cambiar de pantalla
    pinMode(B3_PIN, INPUT_PULLUP); // Abajo izquierda, funcion: arrancar
    pinMode(B4_PIN, INPUT_PULLUP); // Abajo derecha, funcion: launch control
    pinMode(LEVA_IZQ_PIN, INPUT_PULLUP); // Leva derecha
    pinMode(LEVA_DER_PIN, INPUT_PULLUP); // Leva izquierda
    
    /*
    pinMode(B1_LED_PIN, OUTPUT);
    pinMode(B2_LED_PIN, OUTPUT);
    pinMode(B3_LED_PIN, OUTPUT);
    pinMode(B4_LED_PIN, OUTPUT);
    pinMode(E1_PIN_A, INPUT);
    pinMode(E1_PIN_B, INPUT);
    pinMode(E2_PIN_A, INPUT);
    pinMode(E2_PIN_B, INPUT);
    pinMode(E1_BUTTON_PIN, INPUT_PULLUP);
    pinMode(E2_BUTTON_PIN, INPUT_PULLUP);
    */
    

    // attachInterruptArg(digitalPinToInterrupt(E1_PIN_A), handleEncoderInterrupt, this, CHANGE);
    // attachInterruptArg(digitalPinToInterrupt(E2_PIN_A), handleEncoderInterrupt, this, CHANGE);
}

void G24WheelButtons::ejecucion(){

    if(manejador_tarea==NULL){
        estado_tarea=true;

        BaseType_t tarea = xTaskCreate(
            botonera,           // Task function
            "Botones",    // Task name
            4096,                 // Stack size (words)
            this,                 // Task parameter 
            1,                    // Priority 
            &manejador_tarea  // Task handle
        );

        if(tarea==pdPASS){
            Serial.println("Tarea de botones del volante creada");
        } else {
            Serial.println("Error creacion del boton de la tarea del boton del volante");
            manejador_tarea=NULL;
        }
    }

}

void G24WheelButtons::botonera(void *arg){        // Funcion para la tarea de la botonera

    G24WheelButtons* instance = static_cast<G24WheelButtons*>(arg);

    while(instance->estado_tarea){
        instance->check_boton();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    instance->manejador_tarea = NULL;
    vTaskDelete(NULL);
}

void G24WheelButtons::estado_boton(gpio_num_t buttonPin, volatile bool &buttonState, volatile bool &laststate, volatile unsigned long &lastPressTime, volatile unsigned long &temporizador){    // Mira como esta el boton (presionado o no)

    int boton=digitalRead(buttonPin);
    unsigned long tiempo=millis();

    laststate=buttonState;

    if(boton==LOW && !buttonState){   // Esta presionado
        if(tiempo - lastPressTime >= debounceTime){    // No hay doble presion
            if(temporizador==0) temporizador=tiempo;
            if(tiempo-temporizador>=PRESS_TIME){               // Temporizador para que no se presione solo por la cara, hay que mantener pulsado el boton 50ms
                lastPressTime=tiempo;
                buttonState=true;
            }
        }
    } else if(boton==HIGH && buttonState){   // Se ha dejado de presionar
        if(tiempo - lastPressTime >= debounceTime){      // No hay doble presion
            lastPressTime=tiempo;
            buttonState=false;
        }   
    }

    if(boton==HIGH) temporizador=0;

}

void G24WheelButtons::check_boton(){

    uint8_t b1=0, b2=0, b3=0, b4=0, leval=0, levar=0;
    bool enviar=false;

    estado_boton(B1_PIN, B1.estado, B1.last_estado, B1.lastPressTime, B1.temporizador);
    estado_boton(B2_PIN, B2.estado, B2.last_estado, B2.lastPressTime, B2.temporizador);
    estado_boton(B3_PIN, B3.estado, B3.last_estado, B3.lastPressTime, B3.temporizador);
    estado_boton(B4_PIN, B4.estado, B4.last_estado, B4.lastPressTime, B4.temporizador);
    estado_boton(LEVA_IZQ_PIN, levaizq.estado, levaizq.last_estado, levaizq.lastPressTime, levaizq.temporizador);
    estado_boton(LEVA_DER_PIN, levader.estado, levader.last_estado, levader.lastPressTime, levader.temporizador);

    if(B1.estado!=B1.last_estado){    // Boton neutral presionado, evento puntual
        b1=B1.estado;
        enviar=true;
    }

    if(B2.estado!=B2.last_estado){    // Cambio de pantalla presionado, evento puntual
        b2=B2.estado;
        enviar=true;
    }

    if(B3.estado!=B3.last_estado){    // Arrancado presionado, mantener
        b3=B3.estado;
        enviar=true;
    }

    if(B4.estado!=B4.last_estado){    // Launch control presionado, mantener
        b4=B4.estado;
        enviar=true;
    }

    if(levaizq.estado!=levaizq.last_estado){
        leval=levaizq.estado;
        enviar=true;
    }

    if(levader.estado!=levader.last_estado){
        levar=levader.estado;
        enviar=true;
    }

    if(enviar){
    _can->send_frame(_can->mensaje_botonera(b1, b2, b3, b4, leval, levar));   // Crea un mensaje y lo envia
    }

}