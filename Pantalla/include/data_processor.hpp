#ifndef DATAPROCESSOR_HPP
#define DATAPROCESSOR_HPP

#include "common/common_libraries.hpp"
#include "common/display_id.hpp"
#include "led_strip.hpp"
#include "crowpanel_controller.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class DataProcessor {
public:
    DataProcessor() = default;
    void send_serial(byte type, unsigned int value);
    void send_serial_frame_0(int rpmh, int rpml, int tpsh, int tpsl, int ecth, int ectl, int gear);
    void send_serial_frame_1(int lfws, int rfws, int lrws, int rrws, int maph, int mapl, int ect);
    void send_serial_frame_2(int lambh, int lambl, int lamth, int lamtl, int bvolth, int bvoltl, int iat);
    void send_serial_frame_3(int oilth, int oiltl, int oilph, int oilpl, int maph, int mapl, int useless);
    void send_serial_frame_5(int aux1, int aux2, int aux3, int aux4, int aux5, int aux6, int aux7);
    void send_serial_frame_volante(int neutra, int cpant, int arrancar, int launch, int levaizq, int levader, int aux);
    void set_led_strip(LedStrip *led_strip){
        _led_strip = led_strip;
    }
    void set_crow_panel_controller(CrowPanelController *crow_panel_controller) {
        _crow_panel_controller = crow_panel_controller;
    }
    static void set_dataprocessor();
    void update_rpm_leds(int rpm);

private:
    LedStrip *_led_strip;
    CrowPanelController *_crow_panel_controller;
    int current_display=0;
    int request_screen=0;
};

#endif