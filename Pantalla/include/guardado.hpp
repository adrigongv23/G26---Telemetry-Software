#ifndef GUARDADO_HPP
#define GUARDADO_HPP

#define nombre_archivo "/guardado.txt"

class data{

    public:
        void load_data();
        void save_data();

        void update_data(
            int rpm_maxn,
            int desplazamienton,
            bool modo_oscuron,
            int pantallan,
            int rpm_displayn,
            int diff_rpmn,
            int recepcion_datosn,
            float vbatt_too_highn,
            float vbatt_highn,
            float vbatt_lown,
            int water_hotn,
            int water_mildn,
            int water_coldn,
            int oil_hotn,
            int oil_mildn,
            int oil_coldn,
            float oil_pressure_too_highn,
            float oil_pressure_highn,
            float oil_pressure_lown,
            float fuel_pressure_too_highn,
            float fuel_pressure_highn,
            float fuel_pressure_lown
        );

        int get_rpm();
        int get_desplazamiento();
        bool get_modo_oscuro();

        int get_pantalla();
        int get_rpm_display();
        int get_diff_rpm();
        int get_recepcion_datos();

        float get_vbatt_too_high();
        float get_vbatt_high();
        float get_vbatt_low();

        int get_water_hot();
        int get_water_mild();
        int get_water_cold();

        int get_oil_hot();
        int get_oil_mild();
        int get_oil_cold();

        float get_oil_pressure_too_high();
        float get_oil_pressure_high();
        float get_oil_pressure_low();

        float get_fuel_pressure_too_high();
        float get_fuel_pressure_high();
        float get_fuel_pressure_low();

    private:
        int rpm_max=8000;
        int desplazamiento=0;
        bool modo_oscuro=true;

        int pantalla=0;
        int rpm_display=false;
        int diff_rpm=0;
        int recepcion_datos=0;

        float vbatt_too_high=0;
        float vbatt_high=0;
        float vbatt_low=0;


        int water_hot=0;
        int water_mild=0;
        int water_cold=0;

        int oil_hot=0;
        int oil_mild=0;
        int oil_cold=0;

        float oil_pressure_too_high=0;
        float oil_pressure_high=0;
        float oil_pressure_low=0;

        float fuel_pressure_too_high=0;
        float fuel_pressure_high=0;
        float fuel_pressure_low=0;

};

#endif