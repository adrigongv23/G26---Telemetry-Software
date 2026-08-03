#include "../include/crowpanel_controller.hpp"
#include "../include/ajustes.hpp"
#include <PCA9557.h>

uint32_t CrowPanelController::screenWidth;
uint32_t CrowPanelController::screenHeight;
lv_disp_draw_buf_t CrowPanelController::draw_buf;
lv_color_t CrowPanelController::disp_draw_buf[800 * 480 / 10];
lv_disp_drv_t CrowPanelController::disp_drv;
lv_indev_drv_t CrowPanelController::indev_drv;

SemaphoreHandle_t lvgl_mutex;
PCA9557 Out;

// -------------------------------------------------------------------------------------------


static const int RPM_SCALE_START_X = -385;
static const int RPM_SCALE_BREAK_RPM = 6000;
static const int RPM_SCALE_BREAK_X = -198;
static const int RPM_SCALE_END_X_BASE = 365;

static const int RPM_MAJOR_Y = -190;
static const int RPM_MAJOR_H = 22;

static const int RPM_MID_Y = -186;
static const int RPM_MID_H = 12;

static const int RPM_LABEL_Y = -218;

static const int RPM_MAX_OBJECTS = 16;

static lv_obj_t *rpm_dynamic_major[RPM_MAX_OBJECTS] = {nullptr};
static lv_obj_t *rpm_dynamic_mid[RPM_MAX_OBJECTS] = {nullptr};
static lv_obj_t *rpm_dynamic_label[RPM_MAX_OBJECTS] = {nullptr};

static bool rpm_original_scale_hidden = false;

static int rpm_scale_x_from_rpm(int rpm_value, int rpm_max, int desplazamiento)
{
    if(rpm_max < 1000){
        rpm_max = 1000;
    }

    if(rpm_value < 0){
        rpm_value = 0;
    }

    if(rpm_value > rpm_max){
        rpm_value = rpm_max;
    }

    int end_x = RPM_SCALE_END_X_BASE + desplazamiento;

    if(end_x > 385){
        end_x = 385;
    }

    if(end_x < RPM_SCALE_BREAK_X + 20){
        end_x = RPM_SCALE_BREAK_X + 20;
    }

    if(CrowPanelController::set_lineal()==2 || CrowPanelController::set_lineal()==3 || rpm_max <= RPM_SCALE_BREAK_RPM){
        return RPM_SCALE_START_X + ((rpm_value * (end_x - RPM_SCALE_START_X)) / rpm_max);
    }

    if(rpm_value <= RPM_SCALE_BREAK_RPM){
        return RPM_SCALE_START_X + ((rpm_value * (RPM_SCALE_BREAK_X - RPM_SCALE_START_X)) / RPM_SCALE_BREAK_RPM);
    }

    return RPM_SCALE_BREAK_X + (((rpm_value - RPM_SCALE_BREAK_RPM) * (end_x - RPM_SCALE_BREAK_X)) / (rpm_max - RPM_SCALE_BREAK_RPM));
}

static void hide_original_rpm_scale()
{
    if(rpm_original_scale_hidden){
        return;
    }

    lv_obj_t *original_objs[] = {
        ui_rpm0, ui_rpm1, ui_rpm2, ui_rpm3, ui_rpm4, ui_rpm5, ui_rpm6,
        ui_rpm7, ui_rpm8, ui_rpm9, ui_rpm10, ui_rpm11, ui_rpm12, ui_rpm13,

        ui_rpmt0, ui_rpmt1, ui_rpmt2, ui_rpmt3, ui_rpmt4, ui_rpmt5, ui_rpmt6,
        ui_rpmt7, ui_rpmt8, ui_rpmt9, ui_rpmt10, ui_rpmt11, ui_rpmt12, ui_rpmt16,

        ui_rpmbaja1, ui_rpmbaja2, ui_rpmbaja3, ui_rpmbaja4,
        ui_rpmbaja5, ui_rpmbaja6, ui_rpmbaja7
    };

    int total = sizeof(original_objs) / sizeof(original_objs[0]);

    for(int i=0; i<total; i++){
        if(original_objs[i] != nullptr){
            lv_obj_add_flag(original_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    rpm_original_scale_hidden = true;
}

static uint32_t rpm_scale_normal_color()
{
    return (ui_theme_idx == 1) ? CrowPanelController::COLOR_BLANCO_NUEVO : CrowPanelController::COLOR_NEGRO_NUEVO;
}

static uint32_t rpm_scale_red_color()
{
    return CrowPanelController::COLOR_ROJO_BARRA;
}

static void set_rpm_mark_color(lv_obj_t *obj, bool red)
{
    if(obj == nullptr){
        return;
    }

    uint32_t color = red ? rpm_scale_red_color() : rpm_scale_normal_color();

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void set_rpm_label_color(lv_obj_t *obj, bool red)
{
    if(obj == nullptr){
        return;
    }

    uint32_t color = red ? rpm_scale_red_color() : rpm_scale_normal_color();

    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}



// -------------------------------------------------------------------------------------------

CrowPanelController::CrowPanelController()
{
}

void CrowPanelController::begin()
{
    if (initialized) return;

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == NULL) {
        Serial.println("ERROR: lvgl_mutex no creado");
        return;
    }

    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

    Serial.println("Init CrowPanel...");

    pinMode(17, OUTPUT);
    digitalWrite(17, LOW);
    pinMode(18, OUTPUT);
    digitalWrite(18, LOW);
    pinMode(42, OUTPUT);
    digitalWrite(42, LOW);

    Wire.begin(19, 20);
    Out.reset();
    Out.setMode(IO_OUTPUT);
    Out.setState(IO0, IO_LOW);
    Out.setState(IO1, IO_LOW);
    delay(20);
    Out.setState(IO0, IO_HIGH);
    delay(100);
    Out.setMode(IO1, IO_INPUT);

    lcd.begin();
    lcd.setRotation(2);
    lcd.fillScreen(TFT_BLACK);
    screenWidth = lcd.width();
    screenHeight = lcd.height();
    Serial.printf("LCD %lux%lu\n", (unsigned long)screenWidth, (unsigned long)screenHeight);

    lv_init();
    delay(100);

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 10);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.full_refresh = 1;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&CrowPanelController::indev_drv);
    CrowPanelController::indev_drv.type = LV_INDEV_TYPE_POINTER;
    CrowPanelController::indev_drv.read_cb = my_touchpad_read;
    CrowPanelController::indev_drv.user_data = this;
    lv_indev_drv_register(&CrowPanelController::indev_drv);

    ui_init();
    inic_barra_rpm();
    lv_timer_create(CrowPanelController::iniciar_pantalla, 2000, this);

    initialized = true;
    Serial.println("CrowPanel OK");

    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::show_label(lv_obj_t *label){
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::hide_label(lv_obj_t *label){
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

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

    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

    CrowPanelController* controller = static_cast<CrowPanelController*>(indev_driver->user_data);

    uint16_t touchX, touchY;
    bool touched = controller->lcd.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = screenWidth - 1 - touchX;
        data->point.y = screenHeight - 1 - touchY;
    }

    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::set_value_to_label(lv_obj_t *label, double value)
{
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

    if (value == (int)value) {
        lv_label_set_text_fmt(label, "%d", (int)value);
    } else {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%.2f", value);
        lv_label_set_text(label, buffer);
    }

    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::set_string_to_label(lv_obj_t *label, const char *string){
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_label_set_text(label, string);
    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::iniciar_pantalla(lv_timer_t * timer){         // Temporizador para que este 2 segundos en el logo y llama al cambio de pantalla
    CrowPanelController *self = (CrowPanelController *) timer->user_data;
    
    if(self){
        self->encendido_pantalla=true;
        self->change_screen();
    }

    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_timer_del(timer);
    xSemaphoreGiveRecursive(lvgl_mutex);
}

void CrowPanelController::change_screen(){

    if(encendido_pantalla){
    new_screen++;

        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
        if(new_screen >= 2){                // Se puede expandir si se ponen mas pantallas
            new_screen = 1;
        }

        switch(new_screen){
                case 1:
                    lv_disp_load_scr(ui_Screen1);
                    break;
                case 2:
                    lv_disp_load_scr(ui_Screen2);
                    break;
                case 3:
                    //lv_disp_load_scr(ui_Screen3);
                    break;
                default:
                    Serial.println("Error cambiar pantalla");
                    break;
        }

        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

// New color management methods
void CrowPanelController::set_label_color(lv_obj_t *label, uint32_t color) {
    if (label != NULL) {
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

void CrowPanelController::set_panel_color(lv_obj_t *panel, uint32_t bg_color) {
    if (panel != NULL) {
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
        // 1. COPIAMOS la lógica de opacidad de la función default
        // LV_OPA_COVER significa 100% opaco (es lo mismo que 255)
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 2. Aplicamos el color que queremos (Rojo, Verde, etc.)
        lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

void CrowPanelController::set_conditional_colors() {
    // This method will be called from data_processor.cpp to update colors based on conditions
    // Implementation will be added when specific conditions are defined
}

void CrowPanelController::update_rpm_bar(int rpm){      // Actualiza la barra de las rpm

    int pixel=0;
    uint32_t color;
    // bool rpm12300=false;

    pixel=rpm_to_pixel(rpm);

    if(pixel<695){
        color=(ui_theme_idx==1) ? COLOR_BLANCO_NUEVO : COLOR_NEGRO_NUEVO;
        lv_obj_set_style_bg_color(ui_barrpm, lv_color_hex(color), LV_PART_INDICATOR);
    } else{
        lv_obj_set_style_bg_color(ui_barrpm, lv_color_hex(COLOR_ROJO_BARRA), LV_PART_INDICATOR);
    }

    lv_bar_set_value(ui_barrpm, pixel, LV_ANIM_ON);
    
    /*
    if(rpm>12300){
        uint32_t ahora=millis();
        rpm12300=true;

        if(ahora-last_parpadeo>50){
            last_parpadeo=ahora;
            parpadeo=!parpadeo;
        }

        if(parpadeo){
            lv_obj_set_style_bg_color(ui_barrpm, lv_color_make(255, 255, 255), LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(ui_barrpm, lv_color_make(255, 255, 255), LV_PART_INDICATOR);

            lv_obj_set_style_bg_color(ui_barrpm1, lv_color_make(255, 255, 255), LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(ui_barrpm1, lv_color_make(255, 255, 255), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_bg_color(ui_barrpm, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(ui_barrpm, lv_color_make(255, 0, 0), LV_PART_INDICATOR);

            lv_obj_set_style_bg_color(ui_barrpm1, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(ui_barrpm1, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
        }
    }

    if(!rpm12300 && last_rpm12300){
        lv_obj_set_style_bg_color(ui_barrpm, lv_color_make(0, 205, 0), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(ui_barrpm, lv_color_make(230, 230, 0), LV_PART_INDICATOR);

        lv_obj_set_style_bg_color(ui_barrpm1, lv_color_make(230, 230, 0), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(ui_barrpm1, lv_color_make(255, 0, 0), LV_PART_INDICATOR);
    }

    last_rpm12300=rpm12300;
    */

}

int CrowPanelController::rpm_to_pixel(int rpm)
{
    int rpm_max = (get_rpm()/1000)*1000;
    int desplazamiento = get_desplazamiento();

    int x = rpm_scale_x_from_rpm(rpm, rpm_max, desplazamiento);
    int pixel = x - RPM_SCALE_START_X;

    if(pixel < 0){
        pixel = 0;
    }

    if(pixel > 770){
        pixel = 770;
    }

    return pixel;
}

void CrowPanelController::inic_barra_rpm(){             // Inicializa el rango de la barra y la animacion
    
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

    lv_bar_set_range(ui_barrpm, 0, 770);
    lv_obj_set_style_anim_time(ui_barrpm, 40, LV_PART_MAIN);        // Si se ve mal se puede cambiar e incluso quitar, si se quita hay que modificar en update_rpm_bar

    xSemaphoreGiveRecursive(lvgl_mutex);
}

// -------------------------------------------------------------------------------------------


void CrowPanelController::create_rpm_bar(int rpm_max, int desplazamiento)
{
    if(ui_barcontainer == nullptr){
        return;
    }

    if(rpm_max < 1000){
        rpm_max = 1000;
    }

    if(rpm_max > 15000){
        rpm_max = 15000;
    }

    int divisor = rpm_max / 1000;

    if(divisor >= RPM_MAX_OBJECTS){
        divisor = RPM_MAX_OBJECTS - 1;
    }

    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);

    hide_original_rpm_scale();

    for(int i=0; i<RPM_MAX_OBJECTS; i++){
        if(rpm_dynamic_major[i] != nullptr){
            lv_obj_add_flag(rpm_dynamic_major[i], LV_OBJ_FLAG_HIDDEN);
        }

        if(rpm_dynamic_mid[i] != nullptr){
            lv_obj_add_flag(rpm_dynamic_mid[i], LV_OBJ_FLAG_HIDDEN);
        }

        if(rpm_dynamic_label[i] != nullptr){
            lv_obj_add_flag(rpm_dynamic_label[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    int previous_x = RPM_SCALE_START_X;

    for(int i=0; i<=divisor; i++){

        int rpm_value = i * 1000;

        if(i == divisor){
            rpm_value = rpm_max;
        }

        int x = rpm_scale_x_from_rpm(rpm_value, rpm_max, desplazamiento);

        bool red = (i == divisor);

        if(rpm_dynamic_major[i] == nullptr){
            rpm_dynamic_major[i] = lv_obj_create(ui_barcontainer);
            lv_obj_set_width(rpm_dynamic_major[i], 3);
            lv_obj_set_height(rpm_dynamic_major[i], RPM_MAJOR_H);
            lv_obj_set_align(rpm_dynamic_major[i], LV_ALIGN_CENTER);
            lv_obj_clear_flag(rpm_dynamic_major[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(rpm_dynamic_major[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_x(rpm_dynamic_major[i], x);
        lv_obj_set_y(rpm_dynamic_major[i], RPM_MAJOR_Y);
        lv_obj_clear_flag(rpm_dynamic_major[i], LV_OBJ_FLAG_HIDDEN);
        set_rpm_mark_color(rpm_dynamic_major[i], red);

        if(rpm_dynamic_label[i] == nullptr){
            rpm_dynamic_label[i] = lv_label_create(ui_barcontainer);
            lv_obj_set_width(rpm_dynamic_label[i], LV_SIZE_CONTENT);
            lv_obj_set_height(rpm_dynamic_label[i], LV_SIZE_CONTENT);
            lv_obj_set_align(rpm_dynamic_label[i], LV_ALIGN_CENTER);
            lv_obj_set_style_text_font(rpm_dynamic_label[i], &ui_font_rpm, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_x(rpm_dynamic_label[i], x);
        lv_obj_set_y(rpm_dynamic_label[i], RPM_LABEL_Y);

        if(rpm_value <= 6000){
            lv_label_set_text_fmt(rpm_dynamic_label[i], "%d", rpm_value / 1000);
        } else {
            lv_label_set_text_fmt(rpm_dynamic_label[i], "%d", rpm_value);
        }

        lv_obj_clear_flag(rpm_dynamic_label[i], LV_OBJ_FLAG_HIDDEN);
        set_rpm_label_color(rpm_dynamic_label[i], red);

        if(i > 0 && rpm_value > 6000){

            int x_middle = (previous_x + x) / 2;
            bool mid_red = (i == divisor);

            if(rpm_dynamic_mid[i] == nullptr){
                rpm_dynamic_mid[i] = lv_obj_create(ui_barcontainer);
                lv_obj_set_width(rpm_dynamic_mid[i], 3);
                lv_obj_set_height(rpm_dynamic_mid[i], RPM_MID_H);
                lv_obj_set_align(rpm_dynamic_mid[i], LV_ALIGN_CENTER);
                lv_obj_clear_flag(rpm_dynamic_mid[i], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_radius(rpm_dynamic_mid[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            }

            lv_obj_set_x(rpm_dynamic_mid[i], x_middle);
            lv_obj_set_y(rpm_dynamic_mid[i], RPM_MID_Y);
            lv_obj_clear_flag(rpm_dynamic_mid[i], LV_OBJ_FLAG_HIDDEN);
            set_rpm_mark_color(rpm_dynamic_mid[i], mid_red);
        }

        previous_x = x;
    }

    xSemaphoreGiveRecursive(lvgl_mutex);
}

int CrowPanelController::set_lineal(){
    return get_rpm_display();
}


// -------------------------------------------------------------------------------------------

