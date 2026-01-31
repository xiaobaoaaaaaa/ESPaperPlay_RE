#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_NONE
};

// Native global variables

extern const char *get_var_current_time();
extern void set_var_current_time(const char *value);
extern const char *get_var_current_date();
extern void set_var_current_date(const char *value);
extern const char *get_var_current_weekday();
extern void set_var_current_weekday(const char *value);
extern const char *get_var_yiyan();
extern void set_var_yiyan(const char *value);
extern const char *get_var_solar_term();
extern void set_var_solar_term(const char *value);
extern const char *get_var_weather_text();
extern void set_var_weather_text(const char *value);
extern const char *get_var_weather_icon();
extern void set_var_weather_icon(const char *value);
extern const char *get_var_weather_temp();
extern void set_var_weather_temp(const char *value);
extern const char *get_var_weather_uptime();
extern void set_var_weather_uptime(const char *value);
extern const char *get_var_weather_location();
extern void set_var_weather_location(const char *value);
extern const char *get_var_weather_feelslike();
extern void set_var_weather_feelslike(const char *value);
extern const char *get_var_weather_wind_dir();
extern void set_var_weather_wind_dir(const char *value);
extern int32_t get_var_weather_wind_scale();
extern void set_var_weather_wind_scale(int32_t value);
extern int32_t get_var_weather_humidity();
extern void set_var_weather_humidity(int32_t value);
extern int32_t get_var_weather_precip();
extern void set_var_weather_precip(int32_t value);
extern int32_t get_var_weather_pressure();
extern void set_var_weather_pressure(int32_t value);
extern int32_t get_var_weather_visibility();
extern void set_var_weather_visibility(int32_t value);
extern int32_t get_var_weather_cloud();
extern void set_var_weather_cloud(int32_t value);
extern int32_t get_var_weather_dew();
extern void set_var_weather_dew(int32_t value);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/