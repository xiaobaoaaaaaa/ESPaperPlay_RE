#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>

#include "config_manager.h"
#include "wifi.h"

#define TAG "main"

void app_main(void) {
    // 初始化配置管理器
    esp_err_t ret = config_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config_manager_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化 WiFi
    wifi_init();

    return;
}
