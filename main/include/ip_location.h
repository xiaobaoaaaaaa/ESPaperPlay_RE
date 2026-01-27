#pragma once

typedef struct {
    int code;
    char continent[64];     // 洲
    char continent_code[8]; // 洲代码
    char country[64];       // 国家
    char country_code[8];   // 国家代码
    char province[64];      // 省
    char province_code[8];  // 省代码
    char city[64];          // 市
    char city_code[8];      // 市代码
    char district[64];      // 区
    char district_code[8];  // 区代码
    char isp[64];           // 运营商
    char latitude[32];      // 纬度
    char longitude[32];     // 经度
    char message[256];      // 完整位置描述
    char ip[32];            // IP地址
    char td[8];             // 时区相关
} location_t;

esp_err_t get_location(const char *ip, location_t *location);
