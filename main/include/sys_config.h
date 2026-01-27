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
        char id[64];
        char key[64];
    } ip_location;

    struct {
        char city[64];
        char api_host[128];
        char api_key[64];
    } weather;

} sys_config_t;