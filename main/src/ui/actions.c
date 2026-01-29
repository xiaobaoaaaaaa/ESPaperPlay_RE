#include "actions.h"
#include "vars.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ip_location.h"
#include "lvgl_init.h"
#include "screens.h"
#include "weather.h"
#include "yiyan.h"

#include <sys/time.h>
#include <time.h>

#define YIYAN_INTERVAL_MS (3 * 60 * 1000)    // 3分钟
#define WEATHER_INTERVAL_MS (10 * 60 * 1000) // 10分钟

TaskHandle_t get_yiyan_task_handle = NULL;
TaskHandle_t get_weather_task_handle = NULL;
void get_yiyan_task(void *pvParameters) {
    while (1) {
        // 获取一言
        char *yiyan_str = NULL;
        esp_err_t ret = get_yiyan(&yiyan_str);
        if (ret == ESP_OK && yiyan_str != NULL) {
            set_var_yiyan(yiyan_str);
            free(yiyan_str);
        } else {
            set_var_yiyan("获取一言失败");
            ESP_LOGE("get_yiyan_task", "get_yiyan failed with error: %s", esp_err_to_name(ret));
        }

        // 等待3分钟或收到立即执行的通知
        // ulTaskNotifyTake 会阻塞直到收到通知或超时
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(YIYAN_INTERVAL_MS));
    }

    // 正常情况下不会执行到这里，但为了安全起见保留
    get_yiyan_task_handle = NULL;
    vTaskDelete(NULL);
}

void action_get_yiyan(lv_event_t *e) {
    if (get_yiyan_task_handle == NULL) {
        // 任务不存在，创建新任务
        xTaskCreate(get_yiyan_task, "get_yiyan_task", 4096, NULL, 5, &get_yiyan_task_handle);
    } else {
        // 任务已存在，通知任务立即执行
        xTaskNotifyGive(get_yiyan_task_handle);
    }
}

/**
 * @brief 将和风天气图标代码转换为Unicode字符
 *
 * 和风天气图标代码范围: 100-999
 * qweather-icons字体映射: 0xf100 - 0xf9ff
 */
static void weather_icon_to_unicode(uint8_t icon, char *out, size_t out_size) {
    if (out == NULL || out_size < 4) {
        return;
    }

    // 和风天气图标代码转换为Unicode码点 (Private Use Area)
    // 图标代码如100转换为0xf100
    uint32_t unicode = 0xf100 + icon - 100;

    // 转换为UTF-8编码 (3字节，针对0xExxx范围)
    // 格式: 1110xxxx 10xxxxxx 10xxxxxx
    out[0] = 0xE0 | ((unicode >> 12) & 0x0F); // 1110xxxx
    out[1] = 0x80 | ((unicode >> 6) & 0x3F);  // 10xxxxxx
    out[2] = 0x80 | (unicode & 0x3F);         // 10xxxxxx
    out[3] = '\0';
}

/**
 * @brief 格式化时间为"X分钟前"或"X小时前"
 */
static void format_time_ago(time_t timestamp, char *out, size_t out_size) {
    time_t now;
    time(&now);
    int diff_seconds = (int)difftime(now, timestamp);

    if (diff_seconds < 60) {
        snprintf(out, out_size, "刚刚");
    } else if (diff_seconds < 3600) {
        snprintf(out, out_size, "%d分钟前", diff_seconds / 60);
    } else if (diff_seconds < 86400) {
        snprintf(out, out_size, "%d小时前", diff_seconds / 3600);
    } else {
        snprintf(out, out_size, "%d天前", diff_seconds / 86400);
    }
}

void get_weather_task(void *pvParameters) {
    const char *TAG = "get_weather_task";
    location_t *location = NULL;
    weather_now_t *weather = NULL;

    while (1) {
        // 分配内存
        if (location == NULL) {
            location = heap_caps_malloc(sizeof(location_t), MALLOC_CAP_SPIRAM);
        }
        if (weather == NULL) {
            weather = heap_caps_malloc(sizeof(weather_now_t), MALLOC_CAP_SPIRAM);
        }

        if (location == NULL || weather == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory");
            vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_MS));
            continue;
        }

        // 获取位置信息
        esp_err_t err = get_location(NULL, location);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "get_location failed: %s", esp_err_to_name(err));

            // 更新UI显示错误
            SemaphoreHandle_t lvgl_mutex = lvgl_get_mutex();
            if (lvgl_mutex != NULL) {
                xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
                lv_label_set_text(objects.main_page_weather_text, "定位失败");
                lv_label_set_text(objects.main_page_weather_uptime, "未更新");
                xSemaphoreGive(lvgl_mutex);
            }

            vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_MS));
            continue;
        }

        // 获取天气信息
        err = get_weather_now(location, weather);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "get_weather_now failed: %s", esp_err_to_name(err));

            // 更新UI显示错误
            SemaphoreHandle_t lvgl_mutex = lvgl_get_mutex();
            if (lvgl_mutex != NULL) {
                xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
                lv_label_set_text(objects.main_page_weather_text, "获取失败");
                lv_label_set_text(objects.main_page_weather_uptime, "未更新");
                xSemaphoreGive(lvgl_mutex);
            }

            vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_MS));
            continue;
        }

        ESP_LOGI(TAG, "Weather updated: %.1f°C, %s (icon: %d, obs_time: %ld)", weather->temperature,
                 weather->text, weather->icon, (long)weather->obs_time);

        // 准备UI更新数据
        char icon_str[4] = {0};
        char temp_str[16] = {0};
        char uptime_str[32] = {0};

        weather_icon_to_unicode(weather->icon, icon_str, sizeof(icon_str));
        snprintf(temp_str, sizeof(temp_str), "%.0f°C", weather->temperature);

        // 使用API返回的观测时间
        if (weather->obs_time > 0) {
            format_time_ago(weather->obs_time, uptime_str, sizeof(uptime_str));
        } else {
            snprintf(uptime_str, sizeof(uptime_str), "未知");
        }

        // 使用互斥锁保护UI更新
        SemaphoreHandle_t lvgl_mutex = lvgl_get_mutex();
        if (lvgl_mutex != NULL) {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

            // 更新天气图标
            lv_label_set_text(objects.main_page_weather_icon, icon_str);

            // 更新温度
            lv_label_set_text(objects.main_page_weather_temp, temp_str);

            // 更新天气描述
            lv_label_set_text(objects.main_page_weather_text, weather->text);

            // 更新时间
            lv_label_set_text(objects.main_page_weather_uptime, uptime_str);

            xSemaphoreGive(lvgl_mutex);
        } else {
            ESP_LOGW(TAG, "LVGL mutex not available");
        }

        // 等待10分钟或收到立即执行的通知
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_INTERVAL_MS));
    }

    // 清理内存（正常情况下不会执行到这里）
    if (location != NULL) {
        heap_caps_free(location);
    }
    if (weather != NULL) {
        heap_caps_free(weather);
    }

    get_weather_task_handle = NULL;
    vTaskDelete(NULL);
}

void action_get_weather(lv_event_t *e) {
    if (get_weather_task_handle == NULL) {
        // 任务不存在，创建新任务
        xTaskCreate(get_weather_task, "get_weather_task", 8192, NULL, 5, &get_weather_task_handle);
    } else {
        // 任务已存在，通知任务立即执行
        xTaskNotifyGive(get_weather_task_handle);
    }
}