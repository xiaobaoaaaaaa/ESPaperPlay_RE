#pragma once

#include <stdbool.h>

typedef struct {
    int code;
    char continent[64];     // 洲
    char continent_code[8]; // 洲代码（字母）
    char country[64];       // 国家
    char country_code[8];   // 国家代码（字母）
    char province[64];      // 省
    int province_code;      // 省代码（数值）
    bool has_province_code;
    char city[64]; // 市
    int city_code; // 市代码（数值）
    bool has_city_code;
    char district[64]; // 区
    int district_code; // 区代码（数值，可能不存在）
    bool has_district;
    bool has_district_code;
    char isp[64];      // 运营商
    double latitude;   // 纬度
    double longitude;  // 经度
    char message[256]; // 完整位置描述
    char ip[40];       // IP地址（支持 IPv6）
    char td[8];        // 时区相关
} location_t;

esp_err_t get_location(const char *ip, location_t *location);
