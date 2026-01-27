#pragma once

#include "esp_err.h"

/**
 * @brief 启动 HTTP Web 服务器，提供配置接口与静态文件服务。
 * @param base_path FATFS 挂载路径，例如 "/flash"。
 */
esp_err_t webserver_start(const char *base_path);

/**
 * @brief 停止 Web 服务器。
 */
void webserver_stop();
