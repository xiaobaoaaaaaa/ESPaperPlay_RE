#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "touch.h"
#include <stdio.h>

#include "config_manager.h"
#include "epaper.h"
#include "lvgl_init.h"
#include "sntp.h"
#include "wifi.h"

#define TAG "main"

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

void wifi_and_time_init_task(void *pvParameter) {
    // 初始化 WiFi
    wifi_init();

    // 初始化 SNTP 时间同步
    time_init();

    vTaskDelete(NULL);
}

void app_main(void) {
    // 初始化 FATFS
    esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = true,
                                               .max_files = 5,
                                               .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    // 2. 执行挂载
    esp_err_t err = esp_vfs_fat_spiflash_mount("/flash", "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "挂载FATFS失败 (%s)", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "FATFS挂载成功，挂载点为 /flash");

    // 初始化配置管理器
    esp_err_t ret = config_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config_manager_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化 WiFi
    xTaskCreate(wifi_and_time_init_task, "wifi_init_task", 4096, NULL, 5, NULL);

    // 初始化 LVGL
    lvgl_init_epaper_display();

    return;
}
