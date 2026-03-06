#pragma once

#include <stdbool.h>

#include "esp_err.h"

extern bool sdcard_mounted;

esp_err_t sdcard_init(void);
esp_err_t sdcard_list_root(void);