#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

esp_err_t epaper_init(void);

// 全局面板句柄，可供其他模块直接操作
extern esp_lcd_panel_handle_t s_panel_handle;