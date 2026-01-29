#include "vars.h"
#include <string.h>

char current_time[100] = {0};

const char *get_var_current_time() { return current_time; }

void set_var_current_time(const char *value) {
    strncpy(current_time, value, sizeof(current_time) / sizeof(char));
    current_time[sizeof(current_time) / sizeof(char) - 1] = 0;
}

char current_date[100] = {0};

const char *get_var_current_date() { return current_date; }

void set_var_current_date(const char *value) {
    strncpy(current_date, value, sizeof(current_date) / sizeof(char));
    current_date[sizeof(current_date) / sizeof(char) - 1] = 0;
}

char current_weekday[100] = {0};

const char *get_var_current_weekday() { return current_weekday; }

void set_var_current_weekday(const char *value) {
    strncpy(current_weekday, value, sizeof(current_weekday) / sizeof(char));
    current_weekday[sizeof(current_weekday) / sizeof(char) - 1] = 0;
}

char yiyan[100] = {0};

const char *get_var_yiyan() { return yiyan; }

void set_var_yiyan(const char *value) {
    strncpy(yiyan, value, sizeof(yiyan) / sizeof(char));
    yiyan[sizeof(yiyan) / sizeof(char) - 1] = 0;
}

char solar_term[100] = {0};

const char *get_var_solar_term() { return solar_term; }

void set_var_solar_term(const char *value) {
    strncpy(solar_term, value, sizeof(solar_term) / sizeof(char));
    solar_term[sizeof(solar_term) / sizeof(char) - 1] = 0;
}