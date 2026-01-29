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


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/