#include "esp_log.h"
#include "nvs_flash.h"

#include "config_manager.h"

#define TAG "config_manager"
#define CONFIG_NVS_NAMESPACE "sys_config"

sys_config_t sys_config;

/**
 * @brief 从 NVS 加载系统配置
 * @param config 指向 sys_config_t 结构体的指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t sys_config_load(sys_config_t *config) {
    nvs_handle_t nvs_handle;
    size_t required_size = 0;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    required_size = sizeof(config->device_name);
    err = nvs_get_str(nvs_handle, "device_name", config->device_name, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded device_name: %s", config->device_name);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "device_name not found, using default");
        snprintf(config->device_name, sizeof(config->device_name), "ESPDevice");
    } else {
        ESP_LOGI(TAG, "nvs_get_str for device_name failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->wifi.ssid);
    err = nvs_get_str(nvs_handle, "wifi_ssid", config->wifi.ssid, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded wifi_ssid: %s", config->wifi.ssid);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "wifi_ssid not found, using default");
        snprintf(config->wifi.ssid, sizeof(config->wifi.ssid), "DefaultSSID");
    } else {
        ESP_LOGI(TAG, "nvs_get_str for wifi_ssid failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->wifi.password);
    err = nvs_get_str(nvs_handle, "wifi_password", config->wifi.password, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded wifi_password: %s", config->wifi.password);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "wifi_password not found, using default");
        snprintf(config->wifi.password, sizeof(config->wifi.password), "DefaultPassword");
    } else {
        ESP_LOGI(TAG, "nvs_get_str for wifi_password failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->display.fast_refresh_count);
    err = nvs_get_str(nvs_handle, "fast_refresh_count", (char *)&config->display.fast_refresh_count,
                      &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded fast_refresh_count: %d", config->display.fast_refresh_count);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "fast_refresh_count not found, using default");
        config->display.fast_refresh_count = 30;
    } else {
        ESP_LOGI(TAG, "nvs_get_str for fast_refresh_count failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->display.dither_mode);
    err = nvs_get_str(nvs_handle, "dither_mode", (char *)&config->display.dither_mode,
                      &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded dither_mode: %d", config->display.dither_mode);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "dither_mode not found, using default");
        config->display.dither_mode = DITHER_MODE_NONE;
    } else {
        ESP_LOGI(TAG, "nvs_get_str for dither_mode failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 保存系统配置到 NVS
 * @param config 指向 sys_config_t 结构体的指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t config_manager_save_config(sys_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "device_name", config->device_name);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_set_str for device_name failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "wifi_ssid", config->wifi.ssid);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_set_str for wifi_ssid failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "wifi_password", config->wifi.password);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_set_str for wifi_password failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err =
        nvs_set_str(nvs_handle, "fast_refresh_count", (char *)&config->display.fast_refresh_count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_set_str for fast_refresh_count failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "dither_mode", (char *)&config->display.dither_mode);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_set_str for dither_mode failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Saved system configuration to NVS");
    return ESP_OK;
}

void config_manager_get_config(sys_config_t *config) {
    memcpy(config, &sys_config, sizeof(sys_config_t));
}

/**
 * @brief 初始化配置管理器
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t config_manager_init() {
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    } else if (ret != ESP_OK) {
        ESP_LOGI(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化配置管理器
    return sys_config_load(&sys_config);
}