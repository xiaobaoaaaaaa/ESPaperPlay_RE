#pragma once

#include <stdint.h>

typedef struct {
    char device_name[32];

    struct {
        char ssid[32];
        char password[32];
    } wifi;

} sys_config_t;