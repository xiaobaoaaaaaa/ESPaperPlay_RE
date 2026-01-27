#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef struct {
    float temperature;  // 温度，单位摄氏度
    float feelslike;    // 体感温度，单位摄氏度
    uint8_t icon;       // 天气图标代码
    char text[32];      // 天气描述文本
    char wind_dir[8];   // 风向
    uint8_t wind_scale; // 风力等级
    uint8_t humidity;   // 相对湿度，百分比
    float precip;       // 降水量，单位毫米
    float pressure;     // 大气压，单位百帕
    float visibility;   // 能见度，单位公里
    float cloud;        // 云量，百分比
    float dew;          // 露点温度，单位摄氏度
} weather_now_t;

esp_err_t get_weather_now(location_t *location, weather_now_t *weather_now);