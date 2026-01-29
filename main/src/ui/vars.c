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

char weather_text[100] = {0};

const char *get_var_weather_text() { return weather_text; }

void set_var_weather_text(const char *value) {
    strncpy(weather_text, value, sizeof(weather_text) / sizeof(char));
    weather_text[sizeof(weather_text) / sizeof(char) - 1] = 0;
}

char weather_icon[100] = {0};

const char *get_var_weather_icon() { return weather_icon; }

void set_var_weather_icon(const char *value) {
    strncpy(weather_icon, value, sizeof(weather_icon) / sizeof(char));
    weather_icon[sizeof(weather_icon) / sizeof(char) - 1] = 0;
}

char weather_temp[100] = {0};

const char *get_var_weather_temp() { return weather_temp; }

void set_var_weather_temp(const char *value) {
    strncpy(weather_temp, value, sizeof(weather_temp) / sizeof(char));
    weather_temp[sizeof(weather_temp) / sizeof(char) - 1] = 0;
}

char weather_uptime[100] = {0};

const char *get_var_weather_uptime() { return weather_uptime; }

void set_var_weather_uptime(const char *value) {
    strncpy(weather_uptime, value, sizeof(weather_uptime) / sizeof(char));
    weather_uptime[sizeof(weather_uptime) / sizeof(char) - 1] = 0;
}