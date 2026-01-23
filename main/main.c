#include "esp_log.h"
#include "nvs_flash.h"
#include "touch.h"
#include <stdio.h>

#include "config_manager.h"
#include "wifi.h"

#define TAG "main"

void wifi_init_task(void *pvParameter) {
    // 初始化 WiFi
    wifi_init();
    vTaskDelete(NULL);
}

void app_main(void) {
    // 初始化配置管理器
    esp_err_t ret = config_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config_manager_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化 WiFi
    xTaskCreate(wifi_init_task, "wifi_init_task", 4096, NULL, 5, NULL);

    // 初始化触摸屏
    touch_init();

    return;
}
