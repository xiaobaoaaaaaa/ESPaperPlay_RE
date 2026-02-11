#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "touch.h"
#include <math.h>
#include <stdio.h>

#include "audio.h"
#include "config_manager.h"
#include "date_update.h"
#include "epaper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "ip_location.h"
#include "lvgl_init.h"
#include "sntp.h"
#include "weather.h"
#include "webserver.h"
#include "wifi.h"

#define TAG "main"
#define INIT_DONE_BIT BIT0

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static EventGroupHandle_t s_init_event_group = NULL;

static void audio_pcm_test_play_task(void *param) {
    ESP_LOGI(TAG, "Audio test: playing 440Hz sine wave...");
    vTaskDelay(pdMS_TO_TICKS(200));

    const int sample_rate = 44100;
    const int duration_ms = 5000;
    const int frequency_hz = 880;
    const float amplitude = 0.2f;
    const int chunk_samples = 480;
    const float two_pi = 6.2831853f;

    int total_samples = (sample_rate * duration_ms) / 1000;
    int16_t buffer[chunk_samples];

    for (int offset = 0; offset < total_samples; offset += chunk_samples) {
        int count = chunk_samples;
        if (offset + count > total_samples) {
            count = total_samples - offset;
        }
        for (int i = 0; i < count; ++i) {
            float t = (float)(offset + i) / (float)sample_rate;
            float sample = sinf(two_pi * (float)frequency_hz * t);
            buffer[i] = (int16_t)(sample * amplitude * 32767.0f);
        }

        esp_err_t err = audio_write(buffer, count * sizeof(int16_t));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "audio_write failed: %s", esp_err_to_name(err));
            break;
        }
        vTaskDelay(1); // 让出 CPU，避免看门狗超时
    }

    ESP_LOGI(TAG, "Audio test: done");
    vTaskDelete(NULL);
}

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

void lvgl_init_task(void *param) {
    lvgl_init_epaper_display();
    vTaskDelete(NULL);
}

void app_main(void) {
    // 初始化 FATFS
    esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = true,
                                               .max_files = 5,
                                               .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    // 2. 执行挂载
    esp_err_t err =
        esp_vfs_fat_spiflash_mount_rw_wl("/flash", "storage", &mount_config, &s_wl_handle);
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

    // 初始化音频
    audio_init();
    xTaskCreate(audio_pcm_test_play_task, "audio_pcm_test_play_task", 8192, NULL, 5, NULL);

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
    xTaskCreate(lvgl_init_task, "lvgl_init_task", 8192, NULL, 5, NULL);

    return;
}
