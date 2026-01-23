#pragma once

#include "esp_err.h"
#include "sys_config.h"

/**
 * @brief 保存系统配置到 NVS
 * @param config 指向 sys_config_t 结构体的指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t config_manager_save_config(sys_config_t *config);

/**
 * @brief 获取当前系统配置
 */
void config_manager_get_config(sys_config_t *config);

/**
 * @brief 初始化配置管理器
 * @return 错误码
 */
esp_err_t config_manager_init();