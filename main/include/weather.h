#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <time.h>

typedef struct {
    float temperature;  // 温度，单位摄氏度
    float feelslike;    // 体感温度，单位摄氏度
    uint16_t icon;      // 天气图标代码（支持100-9999）
    char text[32];      // 天气描述文本
    char wind_dir[8];   // 风向
    uint8_t wind_scale; // 风力等级
    uint8_t humidity;   // 相对湿度，百分比
    float precip;       // 降水量，单位毫米
    float pressure;     // 大气压，单位百帕
    float visibility;   // 能见度，单位公里
    float cloud;        // 云量，百分比
    float dew;          // 露点温度，单位摄氏度
    time_t obs_time;    // 观测时间戳（Unix时间戳）
} weather_now_t;

/**
 * @brief 每日天气预报数据结构
 *
 * 包含一天的天气预报信息，包括最高/最低温度、
 * 白天/夜间天气条件、风向风速等。
 */
typedef struct {
    char fx_date[11];         // 预报日期，格式 YYYY-MM-DD
    char sunrise[6];          // 日出时间，格式 HH:MM
    char sunset[6];           // 日落时间，格式 HH:MM
    char moonrise[6];         // 月升时间，格式 HH:MM，可能为空
    char moonset[6];          // 月落时间，格式 HH:MM，可能为空
    char moon_phase[16];      // 月相名称
    uint16_t moon_phase_icon; // 月相图标代码
    int8_t temp_max;          // 最高温度，单位摄氏度
    int8_t temp_min;          // 最低温度，单位摄氏度
    uint16_t icon_day;        // 白天天气图标代码
    char text_day[16];        // 白天天气描述
    uint16_t icon_night;      // 夜间天气图标代码
    char text_night[16];      // 夜间天气描述
    uint16_t wind_360_day;    // 白天风向360度角
    char wind_dir_day[8];     // 白天风向名称
    char wind_scale_day[8];   // 白天风力等级，如 "1-2"
    uint8_t wind_speed_day;   // 白天风速，单位 km/h
    uint16_t wind_360_night;  // 夜间风向360度角
    char wind_dir_night[8];   // 夜间风向名称
    char wind_scale_night[8]; // 夜间风力等级，如 "1-2"
    uint8_t wind_speed_night; // 夜间风速，单位 km/h
    uint8_t humidity;         // 相对湿度，百分比
    float precip;             // 降水量，单位毫米
    uint16_t pressure;        // 大气压，单位百帕
    uint8_t vis;              // 能见度，单位公里
    uint8_t cloud;            // 云量，百分比
    uint8_t uv_index;         // 紫外线强度指数
} weather_daily_t;

/**
 * @brief 天气预报数组
 *
 * 存储多天的天气预报数据。
 */
typedef struct {
    uint8_t count;             // 预报数据天数
    weather_daily_t daily[10]; // 预报数据数组，最多支持10天
} weather_forecast_t;

esp_err_t get_weather_now(location_t *location, weather_now_t *weather_now);
esp_err_t get_weather_forecast(location_t *location, uint8_t days,
                               weather_forecast_t *weather_forecast);