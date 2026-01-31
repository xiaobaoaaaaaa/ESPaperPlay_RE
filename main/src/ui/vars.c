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

char weather_location[100] = {0};

const char *get_var_weather_location() { return weather_location; }

void set_var_weather_location(const char *value) {
    strncpy(weather_location, value, sizeof(weather_location) / sizeof(char));
    weather_location[sizeof(weather_location) / sizeof(char) - 1] = 0;
}

char weather_feelslike[100] = {0};

const char *get_var_weather_feelslike() { return weather_feelslike; }

void set_var_weather_feelslike(const char *value) {
    strncpy(weather_feelslike, value, sizeof(weather_feelslike) / sizeof(char));
    weather_feelslike[sizeof(weather_feelslike) / sizeof(char) - 1] = 0;
}

char weather_wind_dir[100] = {0};

const char *get_var_weather_wind_dir() { return weather_wind_dir; }

void set_var_weather_wind_dir(const char *value) {
    strncpy(weather_wind_dir, value, sizeof(weather_wind_dir) / sizeof(char));
    weather_wind_dir[sizeof(weather_wind_dir) / sizeof(char) - 1] = 0;
}

int32_t weather_wind_scale;

int32_t get_var_weather_wind_scale() { return weather_wind_scale; }

void set_var_weather_wind_scale(int32_t value) { weather_wind_scale = value; }

int32_t weather_humidity;

int32_t get_var_weather_humidity() { return weather_humidity; }

void set_var_weather_humidity(int32_t value) { weather_humidity = value; }

int32_t weather_precip;

int32_t get_var_weather_precip() { return weather_precip; }

void set_var_weather_precip(int32_t value) { weather_precip = value; }

int32_t weather_pressure;

int32_t get_var_weather_pressure() { return weather_pressure; }

void set_var_weather_pressure(int32_t value) { weather_pressure = value; }

int32_t weather_visibility;

int32_t get_var_weather_visibility() { return weather_visibility; }

void set_var_weather_visibility(int32_t value) { weather_visibility = value; }

int32_t weather_cloud;

int32_t get_var_weather_cloud() { return weather_cloud; }

void set_var_weather_cloud(int32_t value) { weather_cloud = value; }

int32_t weather_dew;

int32_t get_var_weather_dew() { return weather_dew; }

void set_var_weather_dew(int32_t value) { weather_dew = value; }
