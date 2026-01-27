#pragma once

#include "dither.h"
#include <stdint.h>

typedef struct {
    char device_name[32];

    struct {
        char ssid[33];
        char password[65];
    } wifi;

    struct {
        int fast_refresh_count;
        dither_mode_t dither_mode;
    } display;

    struct {
        char id[32];
        char key[32];
    } ip_location;

} sys_config_t;