#include "actions.h"
#include "eez-flow.h"
#include "vars.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ip_location.h"
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
 * 基于QWeather Icons字体映射表
 * 参考: qweather-icons.json
 */
static void weather_icon_to_unicode(uint16_t icon, char *out, size_t out_size) {
    if (out == NULL || out_size < 4) {
        return;
    }

    // 和风天气图标代码到Unicode码点的映射表
    // 格式: {图标代码, Unicode码点}
    static const struct {
        uint16_t code;
        uint32_t unicode;
    } icon_map[] = {
        // 晴到多云系列
        {100, 61697},
        {101, 61698},
        {102, 61699},
        {103, 61700},
        {104, 61701},
        // 夜间晴到多云系列
        {150, 61702},
        {151, 61703},
        {152, 61704},
        {153, 61705},
        // 雨天系列
        {300, 61706},
        {301, 61707},
        {302, 61708},
        {303, 61709},
        {304, 61710},
        {305, 61711},
        {306, 61712},
        {307, 61713},
        {308, 61714},
        {309, 61715},
        {310, 61716},
        {311, 61717},
        {312, 61718},
        {313, 61719},
        {314, 61720},
        {315, 61721},
        {316, 61722},
        {317, 61723},
        {318, 61724},
        // 夜间雨天系列
        {350, 61725},
        {351, 61726},
        // 雨
        {399, 61727},
        // 雪天系列
        {400, 61728},
        {401, 61729},
        {402, 61730},
        {403, 61731},
        {404, 61732},
        {405, 61733},
        {406, 61734},
        {407, 61735},
        {408, 61736},
        {409, 61737},
        {410, 61738},
        // 夜间雪天系列
        {456, 61739},
        {457, 61740},
        // 雪
        {499, 61741},
        // 雾霾沙尘系列
        {500, 61742},
        {501, 61743},
        {502, 61744},
        {503, 61745},
        {504, 61746},
        {507, 61747},
        {508, 61748},
        {509, 61749},
        {510, 61750},
        {511, 61751},
        {512, 61752},
        {513, 61753},
        {514, 61754},
        {515, 61755},
        // 月相系列
        {800, 61756},
        {801, 61757},
        {802, 61758},
        {803, 61759},
        {804, 61760},
        {805, 61761},
        {806, 61762},
        {807, 61763},
        // 极端天气
        {900, 61764},
        {901, 61765},
        // 未知
        {999, 61766},
    };

    // 查找图标代码对应的Unicode码点
    uint32_t unicode = 61766; // 默认使用999的未知图标
    for (size_t i = 0; i < sizeof(icon_map) / sizeof(icon_map[0]); i++) {
        if (icon_map[i].code == icon) {
            unicode = icon_map[i].unicode;
            break;
        }
    }

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
            set_var_weather_text("定位失败");
            set_var_weather_uptime("未更新");

            vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_MS));
            continue;
        }

        // 获取天气信息
        err = get_weather_now(location, weather);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "get_weather_now failed: %s", esp_err_to_name(err));

            // 更新UI显示错误
            set_var_weather_text("获取失败");
            set_var_weather_uptime("未更新");

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

        // 通过变量更新UI
        set_var_weather_icon(icon_str);
        set_var_weather_temp(temp_str);
        set_var_weather_text(weather->text);
        set_var_weather_uptime(uptime_str);

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

void action_change_to_previous_screen(lv_event_t *e) {
    // 获取gesture方向
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_event_get_indev(e));
    if (dir == LV_DIR_RIGHT) {
        // 左滑，返回上一个屏幕，使用 eez flow 屏幕栈管理
        ESP_LOGI("screen_change", "Popping screen with eez_flow");
        eez_flow_pop_screen(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    }
}