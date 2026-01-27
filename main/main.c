#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "touch.h"
#include <stdio.h>

#include "config_manager.h"
#include "date_update.h"
#include "epaper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "ip_location.h"
#include "lvgl_init.h"
#include "sntp.h"
#include "webserver.h"
#include "wifi.h"

#define TAG "main"
#define INIT_DONE_BIT BIT0

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static EventGroupHandle_t s_init_event_group = NULL;

void wifi_and_time_init_task(void *pvParameter) {
    // 初始化 WiFi
    wifi_init();

    // 初始化 SNTP 时间同步
    time_init();

    // 初始化日期更新时间服务
    date_update_init();

    // 通知主任务网络与时间初始化已完成
    xEventGroupSetBits(s_init_event_group, INIT_DONE_BIT);

    vTaskDelete(NULL);
}

void get_location_task(void *pvParameter) {
    // 示例：获取当前设备的 IP 位置
    location_t *location = heap_caps_malloc(sizeof(location_t), MALLOC_CAP_SPIRAM);
    esp_err_t err = get_location(NULL, location);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_location failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "IP Location: %s", location->message);
    }
    heap_caps_free(location);
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

    s_init_event_group = xEventGroupCreate();
    if (s_init_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create init event group");
        return;
    }

    // 初始化 WiFi/时间（异步任务，完成后通过事件组通知）
    BaseType_t task_created =
        xTaskCreate(wifi_and_time_init_task, "wifi_init_task", 4096, NULL, 5, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wifi/time init task");
        return;
    }

    // 等待网络与时间初始化完成后再初始化 LVGL
    xEventGroupWaitBits(s_init_event_group, INIT_DONE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Network/time init done, starting LVGL");

    // 启动 Web 服务器，提供配置页面与 API
    err = webserver_start("/flash");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "webserver_start failed: %s", esp_err_to_name(err));
    }

    // 初始化 LVGL
    lvgl_init_epaper_display();

    xTaskCreate(get_location_task, "get_location_task", 8192, NULL, 5, NULL);

    return;
}
