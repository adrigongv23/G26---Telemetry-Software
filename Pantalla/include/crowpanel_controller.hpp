#ifndef CROWPANEL_CONTROLLER_HPP
#define CROWPANEL_CONTROLLER_HPP

#include <lvgl.h>
#include <DHT20.h>
#include <SPI.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include "ui/ui.h"

#define TFT_BL 2

extern SemaphoreHandle_t lvgl_mutex;

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;
    LGFX(void)
    {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = 800;
            cfg.memory_height = 480;
            cfg.panel_width = 800;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0 = GPIO_NUM_8;  // B0
            cfg.pin_d1 = GPIO_NUM_3;  // B1
            cfg.pin_d2 = GPIO_NUM_46; // B2
            cfg.pin_d3 = GPIO_NUM_9;  // B3
            cfg.pin_d4 = GPIO_NUM_1;  // B4

            cfg.pin_d5 = GPIO_NUM_5;  // G0
            cfg.pin_d6 = GPIO_NUM_6;  // G1
            cfg.pin_d7 = GPIO_NUM_7;  // G2
            cfg.pin_d8 = GPIO_NUM_15; // G3
            cfg.pin_d9 = GPIO_NUM_16; // G4
            cfg.pin_d10 = GPIO_NUM_4; // G5

            cfg.pin_d11 = GPIO_NUM_45; // R0
            cfg.pin_d12 = GPIO_NUM_48; // R1
            cfg.pin_d13 = GPIO_NUM_47; // R2
            cfg.pin_d14 = GPIO_NUM_21; // R3
            cfg.pin_d15 = GPIO_NUM_14; // R4

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync = GPIO_NUM_41;
            cfg.pin_hsync = GPIO_NUM_39;
            cfg.pin_pclk = GPIO_NUM_0;
            cfg.freq_write = 15000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;
            
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;

            cfg.pclk_active_neg = 1;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 0;
            cfg.x_max      = 799;
            cfg.y_min      = 0;
            cfg.y_max      = 479;
            cfg.pin_int    = -1;
            cfg.pin_rst    = -1;
            cfg.bus_shared = true;
            cfg.offset_rotation = 2;
            cfg.i2c_port   = I2C_NUM_1;
            cfg.pin_sda    = GPIO_NUM_19;
            cfg.pin_scl    = GPIO_NUM_20;
            cfg.freq       = 400000;
            cfg.i2c_addr   = 0x14;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

class CrowPanelController {
public:

    bool encendido_pantalla=false;
    LGFX lcd;
    static uint32_t screenWidth;
    static uint32_t screenHeight;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t disp_draw_buf[800 * 480 / 10];
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    CrowPanelController();
    void begin();
    static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    static void set_value_to_label(lv_obj_t *label, double value);
    void set_string_to_label(lv_obj_t *label, const char *string);
    void change_screen();
    static void iniciar_pantalla(lv_timer_t * timer);
    void show_label(lv_obj_t *label);
    void hide_label(lv_obj_t *label);
    
    // New methods for conditional color management
    void set_label_color(lv_obj_t *label, uint32_t color);
    void set_panel_color(lv_obj_t *panel, uint32_t bg_color);
    void set_conditional_colors();
    void update_rpm_bar(int rpm);  
    int rpm_to_pixel(int rpm);    // Update RPM LED bar based on RPM (8000-12500 range)
    static void create_rpm_bar(int, int);
    void inic_barra_rpm();
    static int set_lineal();
    
    // Predefined colors for different conditions
    static const uint32_t COLOR_NORMAL = 0xFFFFFF;      // White
    static const uint32_t COLOR_WARNING = 0xFFFF00;     // Yellow  
    static const uint32_t COLOR_CRITICAL = 0xFF0000;    // Red
    static const uint32_t COLOR_GOOD = 0x00FF00;        // Green
    static const uint32_t COLOR_INFO = 0xFF8500;        // Orange (current label color)
    static const uint32_t COLOR_BLUE = 0x0080FF;        // Blue
    static const uint32_t COLOR_PANEL_DEFAULT = 0x2C2C2C;  // Dark grey for panel backgrounds

    static const uint32_t COLOR_ROJO=0xFF5454;    // Rojo mas oscuro
    static const uint32_t COLOR_VERDE=0x00CB36;     // Verde mas oscuro
    static const uint32_t COLOR_NEGROP=0x101010;    // El color negro de la pantalla negra

    static const uint32_t COLOR_NEGRO_NUEVO=0x232323;
    static const uint32_t COLOR_ROJO_NUEVO=0x9E3D3D;
    static const uint32_t COLOR_VERDE_NUEVO=0x458F45;
    static const uint32_t COLOR_AMARILLO_NUEVO=0xA8A800;
    static const uint32_t COLOR_AZUL_NUEVO=0x02A6AC;
    static const uint32_t COLOR_BLANCO_NUEVO=0xd2d2d2;
    static const uint32_t COLOR_ROJO_BARRA=0xbb1818;
    static const uint32_t COLOR_FONDO_BARRA_OSCURO=0x3A3A3A;

    static const uint32_t COLOR_AMARILLO_CLARO=0xD7D737;
    static const uint32_t COLOR_AZUL_CLARO=0x31CBD1;
    static const uint32_t COLOR_ROJO_CLARO=0xD54A4A;
    static const uint32_t COLOR_VERDE_CLARO=0x5BB95B;
    static const uint32_t COLOR_BLANCO_CLARO=0xE9E9E9;
    static const uint32_t COLOR_FONDO_BARRA_CLARO=0xAAAAAA;
    
private:

    bool initialized=false;
    int new_screen=0;
    uint32_t last_parpadeo=0;
    bool last_rpm12300=false;
    bool parpadeo=false;

};

#endif
