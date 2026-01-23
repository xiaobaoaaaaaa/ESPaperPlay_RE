#pragma once

#include <stdint.h>

typedef struct {
    char device_name[32];

    struct {
        char ssid[33];
        char password[65];
    } wifi;

} sys_config_t;