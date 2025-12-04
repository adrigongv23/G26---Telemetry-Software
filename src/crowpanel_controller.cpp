#include "../include/crowpanel_controller.hpp"

uint32_t CrowPanelController::screenWidth;
uint32_t CrowPanelController::screenHeight;
lv_disp_draw_buf_t CrowPanelController::draw_buf;
lv_color_t CrowPanelController::disp_draw_buf[800 * 480 / 10];
lv_disp_drv_t CrowPanelController::disp_drv;
lv_indev_drv_t CrowPanelController::indev_drv;

CrowPanelController::CrowPanelController()
{
    Wire.begin(19, 20);
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);
    lcd.begin();
    lcd.setRotation(2);  // Rotate display 180 degrees (upside down)
    lcd.fillScreen(TFT_BLACK);
    delay(200);

    lv_init();

    screenWidth = lcd.width();
    screenHeight = lcd.height();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 10);
    lv_disp_drv_init(&disp_drv);

    disp_drv.user_data = this;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;

    lv_indev_drv_register(&indev_drv);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    ui_init();
    
    // Initialize RPM bar panels with default dark style
    set_panel_default_style(ui_RPMBar1);
    set_panel_default_style(ui_RPMBar2);
    set_panel_default_style(ui_RPMBar3);
    set_panel_default_style(ui_RPMBar4);
    set_panel_default_style(ui_RPMBar5);
    set_panel_default_style(ui_RPMBar6);
    set_panel_default_style(ui_RPMBar7);
    set_panel_default_style(ui_RPMBar8);
    set_panel_default_style(ui_RPMBar9);
    set_panel_default_style(ui_RPMBar10);
    set_panel_default_style(ui_RPMBar11);
    set_panel_default_style(ui_RPMBar12);
    set_panel_default_style(ui_RPMBar13);
    set_panel_default_style(ui_RPMBar14);
    set_panel_default_style(ui_RPMBar15);
    set_panel_default_style(ui_RPMBar16);
    
    lv_timer_handler();
}

void CrowPanelController::my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    CrowPanelController* controller = static_cast<CrowPanelController*>(disp->user_data);
    LGFX& lcd = controller->lcd;

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
#else
    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
#endif

    lv_disp_flush_ready(disp);
}

void CrowPanelController::set_value_to_label(lv_obj_t *label, double value)
{
    if (value == (int)value) {
        lv_label_set_text_fmt(label, "%d", (int)value);
    } else {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%.2f", value);
        lv_label_set_text(label, buffer);
    }
}

void CrowPanelController::set_string_to_label(lv_obj_t *label, const char *string){
    lv_label_set_text(label, string);
}

void CrowPanelController::change_screen(lv_obj_t *screen){
    if (screen != NULL) {
        lv_disp_load_scr(screen);
    }
}

// New color management methods
void CrowPanelController::set_label_color(lv_obj_t *label, uint32_t color) {
    if (label != NULL) {
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

/*void CrowPanelController::set_panel_color(lv_obj_t *panel, uint32_t bg_color) {
    if (panel != NULL) {
        lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        // IMPORTANTE: Asegura que el fondo sea totalmente visible (opacidad 255)
        // Si esto estaba en 0 o 50, el color nuevo no se vería bien.
        lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
*/
void CrowPanelController::set_panel_color(lv_obj_t *panel, uint32_t bg_color) {
    if (panel != NULL) {
        // 1. COPIAMOS la lógica de opacidad de la función default
        // LV_OPA_COVER significa 100% opaco (es lo mismo que 255)
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 2. Aplicamos el color que queremos (Rojo, Verde, etc.)
        lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        
        // 3. Mantenemos el borde bonito (opcional, pero recomendado para consistencia)
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void CrowPanelController::set_conditional_colors() {
    // This method will be called from data_processor.cpp to update colors based on conditions
    // Implementation will be added when specific conditions are defined
}

void CrowPanelController::set_panel_default_style(lv_obj_t *panel) {
    if (panel != NULL) {
        lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL_DEFAULT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);  // Slightly lighter border
    }
}

void CrowPanelController::update_rpm_bar(int rpm) {
    // RPM range: 8000-12500 (4500 RPM total)
    // 16 panels: each represents 281.25 RPM (4500/16)
    // Colors: 1-6 green, 7-12 yellow, 13-16 red
    
    // Array of all RPM bar panels for easy iteration
    lv_obj_t* rpm_bars[16] = {
        ui_RPMBar1, ui_RPMBar2, ui_RPMBar3, ui_RPMBar4,
        ui_RPMBar5, ui_RPMBar6, ui_RPMBar7, ui_RPMBar8,
        ui_RPMBar9, ui_RPMBar10, ui_RPMBar11, ui_RPMBar12,
        ui_RPMBar13, ui_RPMBar14, ui_RPMBar15, ui_RPMBar16
    };
    
    // Calculate how many panels should be lit based on RPM
    int panels_to_light = 0;
    if (rpm >= 8000) {
        // RPM above 8000, calculate how many panels to light
        int rpm_above_8000 = rpm - 8000;
        if (rpm_above_8000 > 4500) rpm_above_8000 = 4500;  // Cap at 12500 RPM
        panels_to_light = (rpm_above_8000 * 16) / 4500;    // 16 panels over 4500 RPM range
        if (panels_to_light > 16) panels_to_light = 16;
    }
    
    // Update each panel based on its position and RPM
    for (int i = 0; i < 16; i++) {
        if (rpm_bars[i] != NULL) {
            if (i < panels_to_light) {
                // Panel should be lit - determine color based on position
                if (i < 6) {
                    // Panels 1-6: Green (safe zone)
                    set_panel_color(rpm_bars[i], COLOR_GOOD);
                } else if (i < 12) {
                    // Panels 7-12: Yellow (warning zone)
                    set_panel_color(rpm_bars[i], COLOR_WARNING);
                } else {
                    // Panels 13-16: Red (danger zone)
                    set_panel_color(rpm_bars[i], COLOR_CRITICAL);
                }
            } else {
                // Panel should be off - dark grey
                set_panel_color(rpm_bars[i], COLOR_PANEL_DEFAULT);
            }
        }
    }
}