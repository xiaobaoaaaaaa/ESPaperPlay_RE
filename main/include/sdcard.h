#pragma once

extern bool sdcard_mounted;

esp_err_t sdcard_init(void);
esp_err_t sdcard_list_root(void);