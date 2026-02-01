/**
 * @file weather.c
 * @brief 天气数据获取和解析模块
 *
 * 本模块通过 HTTPS API 获取实时天气数据，并提供 GZIP 解压缩功能。
 * 支持的功能：
 * - 通过经纬度获取当前天气信息
 * - 解析天气 JSON 数据
 * - 自动解压缩 GZIP 编码的 HTTP 响应
 *
 * 天气数据包括温度、风向、湿度、气压、云量等多项指标。
 *
 * @author
 * @date YYYY-MM-DD
 */

#include "cJSON.h"
#include "esp_attr.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#include "config_manager.h"
#include "decompress.h"
#include "ip_location.h"
#include "weather.h"

/** @brief 日志标签 */
#define TAG "weather"

/**
 * @brief 响应数据结构
 *
 * 用于存储 HTTP 响应的数据和长度信息。
 */
typedef struct {
    void *data;      /**< 响应数据指针 */
    size_t data_len; /**< 响应数据长度 */
} response_data_t;

/**
 * @brief 解析天气 JSON 数据
 *
 * 从 API 响应的 JSON 中提取当前天气信息，包括温度、风向、湿度等。
 * 处理字段可能为数字或字符串格式的情况。
 *
 * @param json JSON 格式的天气数据字符串
 * @param weather_now 输出参数，解析后的天气数据结构体
 */
static void parse_weather_now(const char *json, weather_now_t *weather_now) {
    if (weather_now == NULL || json == NULL) {
        ESP_LOGE(TAG, "Invalid pointer parameters");
        return;
    }

    // 解析 JSON 字符串
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse weather JSON");
        return;
    }

    // 初始化所有字段为零
    memset(weather_now, 0, sizeof(weather_now_t));

    // 查找 now 字段（当前天气数据）
    cJSON *now = cJSON_GetObjectItemCaseSensitive(root, "now");
    if (now == NULL || !cJSON_IsObject(now)) {
        ESP_LOGE(TAG, "Missing 'now' object in weather response");
        cJSON_Delete(root);
        return;
    }

    // 提取各个天气字段信息
    cJSON *item = NULL;

    item = cJSON_GetObjectItemCaseSensitive(now, "temp");
    if (cJSON_IsNumber(item)) {
        weather_now->temperature = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->temperature = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "feelsLike");
    if (cJSON_IsNumber(item)) {
        weather_now->feelslike = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->feelslike = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "icon");
    if (cJSON_IsNumber(item)) {
        weather_now->icon = (uint16_t)item->valueint;
    } else if (cJSON_IsString(item)) {
        weather_now->icon = (uint16_t)atoi(item->valuestring);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "text");
    if (cJSON_IsString(item)) {
        strncpy(weather_now->text, item->valuestring, sizeof(weather_now->text) - 1);
        weather_now->text[sizeof(weather_now->text) - 1] = '\0';
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "windDir");
    if (cJSON_IsString(item)) {
        strncpy(weather_now->wind_dir, item->valuestring, sizeof(weather_now->wind_dir) - 1);
        weather_now->wind_dir[sizeof(weather_now->wind_dir) - 1] = '\0';
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "windScale");
    if (cJSON_IsNumber(item)) {
        weather_now->wind_scale = (uint8_t)item->valueint;
    } else if (cJSON_IsString(item)) {
        weather_now->wind_scale = (uint8_t)atoi(item->valuestring);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "humidity");
    if (cJSON_IsNumber(item)) {
        weather_now->humidity = (uint8_t)item->valueint;
    } else if (cJSON_IsString(item)) {
        weather_now->humidity = (uint8_t)atoi(item->valuestring);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "precip");
    if (cJSON_IsNumber(item)) {
        weather_now->precip = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->precip = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "pressure");
    if (cJSON_IsNumber(item)) {
        weather_now->pressure = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->pressure = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "vis");
    if (cJSON_IsNumber(item)) {
        weather_now->visibility = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->visibility = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "cloud");
    if (cJSON_IsNumber(item)) {
        weather_now->cloud = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->cloud = strtod(item->valuestring, NULL);
    }

    item = cJSON_GetObjectItemCaseSensitive(now, "dew");
    if (cJSON_IsNumber(item)) {
        weather_now->dew = item->valuedouble;
    } else if (cJSON_IsString(item)) {
        weather_now->dew = strtod(item->valuestring, NULL);
    }

    // 解析观测时间 obsTime (ISO 8601格式: "2026-01-29T00:48+08:00")
    item = cJSON_GetObjectItemCaseSensitive(now, "obsTime");
    if (cJSON_IsString(item)) {
        struct tm tm = {0};
        // 解析ISO 8601时间格式: YYYY-MM-DDTHH:MM+TZ:TZ
        if (sscanf(item->valuestring, "%d-%d-%dT%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                   &tm.tm_hour, &tm.tm_min) == 5) {
            tm.tm_year -= 1900; // tm_year是从1900年开始
            tm.tm_mon -= 1;     // tm_mon是0-11
            tm.tm_sec = 0;
            tm.tm_isdst = -1; // 让系统自动判断是否夏令时
            weather_now->obs_time = mktime(&tm);
            ESP_LOGI(TAG, "Parsed obsTime: %s -> timestamp: %ld", item->valuestring,
                     (long)weather_now->obs_time);
        } else {
            ESP_LOGW(TAG, "Failed to parse obsTime: %s", item->valuestring);
            weather_now->obs_time = 0;
        }
    } else {
        weather_now->obs_time = 0;
    }

    cJSON_Delete(root);
    // 记录解析成功的日志
    ESP_LOGI(TAG, "Weather data parsed successfully: %.1f°C, %s", weather_now->temperature,
             weather_now->text);
}

/**
 * @brief 解析每日天气预报 JSON 数据
 *
 * 从 API 响应的 JSON 中提取多天的天气预报信息。
 *
 * @param json JSON 格式的天气预报数据字符串
 * @param weather_forecast 输出参数，解析后的天气预报数据结构体
 */
static void parse_weather_forecast(const char *json, weather_forecast_t *weather_forecast) {
    if (weather_forecast == NULL || json == NULL) {
        ESP_LOGE(TAG, "Invalid pointer parameters");
        return;
    }

    // 解析 JSON 字符串
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse weather forecast JSON");
        return;
    }

    // 初始化天数为0
    weather_forecast->count = 0;
    memset(weather_forecast->daily, 0, sizeof(weather_forecast->daily));

    // 查找 daily 数组（天气预报数据）
    cJSON *daily_array = cJSON_GetObjectItemCaseSensitive(root, "daily");
    if (daily_array == NULL || !cJSON_IsArray(daily_array)) {
        ESP_LOGE(TAG, "Missing 'daily' array in weather forecast response");
        cJSON_Delete(root);
        return;
    }

    // 遍历每一天的预报数据
    cJSON *daily_item = NULL;
    int day_index = 0;
    cJSON_ArrayForEach(daily_item, daily_array) {
        if (day_index >= 10) {
            ESP_LOGW(TAG, "Weather forecast data exceeds maximum 10 days");
            break;
        }

        if (!cJSON_IsObject(daily_item)) {
            continue;
        }

        weather_daily_t *daily = &weather_forecast->daily[day_index];
        cJSON *item = NULL;

        // 解析预报日期
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "fxDate");
        if (cJSON_IsString(item)) {
            strncpy(daily->fx_date, item->valuestring, sizeof(daily->fx_date) - 1);
            daily->fx_date[sizeof(daily->fx_date) - 1] = '\0';
        }

        // 解析日出时间
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "sunrise");
        if (cJSON_IsString(item)) {
            strncpy(daily->sunrise, item->valuestring, sizeof(daily->sunrise) - 1);
            daily->sunrise[sizeof(daily->sunrise) - 1] = '\0';
        }

        // 解析日落时间
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "sunset");
        if (cJSON_IsString(item)) {
            strncpy(daily->sunset, item->valuestring, sizeof(daily->sunset) - 1);
            daily->sunset[sizeof(daily->sunset) - 1] = '\0';
        }

        // 解析月升时间
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "moonrise");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            strncpy(daily->moonrise, item->valuestring, sizeof(daily->moonrise) - 1);
            daily->moonrise[sizeof(daily->moonrise) - 1] = '\0';
        }

        // 解析月落时间
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "moonset");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            strncpy(daily->moonset, item->valuestring, sizeof(daily->moonset) - 1);
            daily->moonset[sizeof(daily->moonset) - 1] = '\0';
        }

        // 解析月相名称
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "moonPhase");
        if (cJSON_IsString(item)) {
            strncpy(daily->moon_phase, item->valuestring, sizeof(daily->moon_phase) - 1);
            daily->moon_phase[sizeof(daily->moon_phase) - 1] = '\0';
        }

        // 解析月相图标
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "moonPhaseIcon");
        if (cJSON_IsNumber(item)) {
            daily->moon_phase_icon = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->moon_phase_icon = (uint16_t)atoi(item->valuestring);
        }

        // 解析最高温度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "tempMax");
        if (cJSON_IsNumber(item)) {
            daily->temp_max = (int8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->temp_max = (int8_t)atoi(item->valuestring);
        }

        // 解析最低温度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "tempMin");
        if (cJSON_IsNumber(item)) {
            daily->temp_min = (int8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->temp_min = (int8_t)atoi(item->valuestring);
        }

        // 解析白天天气图标
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "iconDay");
        if (cJSON_IsNumber(item)) {
            daily->icon_day = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->icon_day = (uint16_t)atoi(item->valuestring);
        }

        // 解析白天天气描述
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "textDay");
        if (cJSON_IsString(item)) {
            strncpy(daily->text_day, item->valuestring, sizeof(daily->text_day) - 1);
            daily->text_day[sizeof(daily->text_day) - 1] = '\0';
        }

        // 解析夜间天气图标
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "iconNight");
        if (cJSON_IsNumber(item)) {
            daily->icon_night = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->icon_night = (uint16_t)atoi(item->valuestring);
        }

        // 解析夜间天气描述
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "textNight");
        if (cJSON_IsString(item)) {
            strncpy(daily->text_night, item->valuestring, sizeof(daily->text_night) - 1);
            daily->text_night[sizeof(daily->text_night) - 1] = '\0';
        }

        // 解析白天风向360度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "wind360Day");
        if (cJSON_IsNumber(item)) {
            daily->wind_360_day = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->wind_360_day = (uint16_t)atoi(item->valuestring);
        }

        // 解析白天风向名称
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windDirDay");
        if (cJSON_IsString(item)) {
            strncpy(daily->wind_dir_day, item->valuestring, sizeof(daily->wind_dir_day) - 1);
            daily->wind_dir_day[sizeof(daily->wind_dir_day) - 1] = '\0';
        }

        // 解析白天风力等级
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windScaleDay");
        if (cJSON_IsString(item)) {
            strncpy(daily->wind_scale_day, item->valuestring, sizeof(daily->wind_scale_day) - 1);
            daily->wind_scale_day[sizeof(daily->wind_scale_day) - 1] = '\0';
        }

        // 解析白天风速
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windSpeedDay");
        if (cJSON_IsNumber(item)) {
            daily->wind_speed_day = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->wind_speed_day = (uint8_t)atoi(item->valuestring);
        }

        // 解析夜间风向360度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "wind360Night");
        if (cJSON_IsNumber(item)) {
            daily->wind_360_night = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->wind_360_night = (uint16_t)atoi(item->valuestring);
        }

        // 解析夜间风向名称
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windDirNight");
        if (cJSON_IsString(item)) {
            strncpy(daily->wind_dir_night, item->valuestring, sizeof(daily->wind_dir_night) - 1);
            daily->wind_dir_night[sizeof(daily->wind_dir_night) - 1] = '\0';
        }

        // 解析夜间风力等级
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windScaleNight");
        if (cJSON_IsString(item)) {
            strncpy(daily->wind_scale_night, item->valuestring,
                    sizeof(daily->wind_scale_night) - 1);
            daily->wind_scale_night[sizeof(daily->wind_scale_night) - 1] = '\0';
        }

        // 解析夜间风速
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "windSpeedNight");
        if (cJSON_IsNumber(item)) {
            daily->wind_speed_night = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->wind_speed_night = (uint8_t)atoi(item->valuestring);
        }

        // 解析相对湿度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "humidity");
        if (cJSON_IsNumber(item)) {
            daily->humidity = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->humidity = (uint8_t)atoi(item->valuestring);
        }

        // 解析降水量
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "precip");
        if (cJSON_IsNumber(item)) {
            daily->precip = item->valuedouble;
        } else if (cJSON_IsString(item)) {
            daily->precip = strtod(item->valuestring, NULL);
        }

        // 解析大气压强
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "pressure");
        if (cJSON_IsNumber(item)) {
            daily->pressure = (uint16_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->pressure = (uint16_t)atoi(item->valuestring);
        }

        // 解析能见度
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "vis");
        if (cJSON_IsNumber(item)) {
            daily->vis = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->vis = (uint8_t)atoi(item->valuestring);
        }

        // 解析云量
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "cloud");
        if (cJSON_IsNumber(item)) {
            daily->cloud = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->cloud = (uint8_t)atoi(item->valuestring);
        }

        // 解析紫外线指数
        item = cJSON_GetObjectItemCaseSensitive(daily_item, "uvIndex");
        if (cJSON_IsNumber(item)) {
            daily->uv_index = (uint8_t)item->valueint;
        } else if (cJSON_IsString(item)) {
            daily->uv_index = (uint8_t)atoi(item->valuestring);
        }

        day_index++;
    }

    weather_forecast->count = day_index;
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Weather forecast data parsed successfully: %d days", day_index);
}

/**
 * @brief HTTP 客户端事件处理回调函数
 *
 * 处理 HTTP 请求的各个阶段事件，积累响应数据。
 * 使用内存重新分配来接收完整的 GZIP 压缩响应数据。
 *
 * @param evt HTTP 客户端事件结构体
 * @return esp_err_t 错误码
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    // 使用静态变量缓存压缩响应数据
    static char *response_buf = NULL; /**< 响应数据缓冲区 */
    static int total_len = 0;         /**< 已接收数据总长度 */

    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        // 在收到响应头时重置缓冲区
        total_len = 0;
        heap_caps_free(response_buf); // 释放之前分配的内存
        response_buf = NULL;
        break;

    case HTTP_EVENT_ON_DATA:
        // 接收响应数据块
        total_len += evt->data_len;
        char *tmp = heap_caps_realloc(response_buf, total_len, MALLOC_CAP_SPIRAM);
        if (tmp == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
            heap_caps_free(response_buf);
            response_buf = NULL;
            return ESP_ERR_NO_MEM;
        }
        response_buf = tmp;
        memcpy(response_buf + total_len - evt->data_len, evt->data, evt->data_len);
        break;

    case HTTP_EVENT_ON_FINISH:
        // 响应接收完成
        if (response_buf != NULL && total_len > 0) {
            // 将数据转移到用户提供的结构体
            response_data_t *response_data = (response_data_t *)evt->user_data;
            response_data->data = response_buf;
            response_data->data_len = total_len;
            response_buf = NULL;
            total_len = 0;
        }
        break;

    case HTTP_EVENT_ERROR:
    case HTTP_EVENT_DISCONNECTED:
        // 清理错误状态下的内存
        heap_caps_free(response_buf);
        response_buf = NULL;
        total_len = 0;
        break;

    default:
        break;
    }

    return ESP_OK;
}

/**
 * @brief 获取当前天气数据
 *
 * 通过 HTTPS API 使用经纬度获取当前天气信息。
 * API 响应数据使用 GZIP 压缩，本函数自动处理解压缩。
 *
 * @param location 位置信息结构体，包含经纬度坐标
 * @param weather_now 输出参数，包含当前天气数据的结构体
 * @return esp_err_t 错误码，ESP_OK 表示成功
 */
esp_err_t get_weather_now(location_t *location, weather_now_t *weather_now) {
    response_data_t response_data = {.data = NULL, .data_len = 0};

    // 获取天气 API 配置
    sys_config_t sys_config;
    config_manager_get_config(&sys_config);

    // 检查 API 配置是否有效
    if (strlen(sys_config.weather.api_host) == 0 || strlen(sys_config.weather.api_key) == 0) {
        ESP_LOGE(TAG, "Weather API host or key is not configured");
        return ESP_ERR_INVALID_ARG;
    }

    // 构建 API 请求 URL
    char url[256];
    snprintf(url, sizeof(url), "https://%s/v7/weather/now?location=%.2f,%.2f&key=%s",
             sys_config.weather.api_host, location->longitude, location->latitude,
             sys_config.weather.api_key);

    // 配置 HTTP 客户端
    esp_http_client_config_t config = {.url = url,
                                       .event_handler = http_event_handler,
                                       .crt_bundle_attach = esp_crt_bundle_attach,
                                       .user_data = &response_data};

    // 初始化并执行 HTTP 请求
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_cleanup(client);

    // 处理响应数据
    if (response_data.data != NULL && response_data.data_len > 0) {
        // 分配解压缩缓冲区
        size_t decompressed_size = 0;
        char *decompressed_buf = (char *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
        if (decompressed_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for decompressed buffer");
            heap_caps_free(response_data.data);
            return ESP_ERR_NO_MEM;
        }

        // 解压缩 GZIP 数据
        int decompress_result = network_gzip_decompress(response_data.data, response_data.data_len,
                                                        decompressed_buf, &decompressed_size, 4096);
        if (decompress_result != Z_OK) {
            ESP_LOGE(TAG, "Failed to decompress response data: %d", decompress_result);
            heap_caps_free(decompressed_buf);
            heap_caps_free(response_data.data);
            return ESP_FAIL;
        }

        // 解析 JSON 数据
        parse_weather_now(decompressed_buf, weather_now);

        // 释放内存
        heap_caps_free(decompressed_buf);
        heap_caps_free(response_data.data);
    } else {
        ESP_LOGE(TAG, "No response data received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

/**
 * @brief 获取每日天气预报数据
 *
 * 通过 HTTPS API 使用经纬度获取多天天气预报信息。
 * API 响应数据使用 GZIP 压缩，本函数自动处理解压缩。
 *
 * @param location 位置信息结构体，包含经纬度坐标
 * @param days 预报天数，可选值：3, 7, 10, 15, 30
 * @param weather_forecast 输出参数，包含多天天气预报数据的结构体
 * @return esp_err_t 错误码，ESP_OK 表示成功
 */
esp_err_t get_weather_forecast(location_t *location, uint8_t days,
                               weather_forecast_t *weather_forecast) {
    response_data_t response_data = {.data = NULL, .data_len = 0};

    // 参数有效性检查
    if (location == NULL || weather_forecast == NULL) {
        ESP_LOGE(TAG, "Invalid pointer parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (days != 3 && days != 7 && days != 10 && days != 15 && days != 30) {
        ESP_LOGE(TAG, "Invalid days parameter: %d (must be one of 3, 7, 10, 15, 30)", days);
        return ESP_ERR_INVALID_ARG;
    }

    // 获取天气 API 配置
    sys_config_t sys_config;
    config_manager_get_config(&sys_config);

    // 检查 API 配置是否有效
    if (strlen(sys_config.weather.api_host) == 0 || strlen(sys_config.weather.api_key) == 0) {
        ESP_LOGE(TAG, "Weather API host or key is not configured");
        return ESP_ERR_INVALID_ARG;
    }

    // 构建 API 请求 URL，根据天数选择端点
    char url[256];
    snprintf(url, sizeof(url), "https://%s/v7/weather/%dd?location=%.2f,%.2f&key=%s",
             sys_config.weather.api_host, days, location->longitude, location->latitude,
             sys_config.weather.api_key);

    // 配置 HTTP 客户端
    esp_http_client_config_t config = {.url = url,
                                       .event_handler = http_event_handler,
                                       .crt_bundle_attach = esp_crt_bundle_attach,
                                       .user_data = &response_data};

    // 初始化并执行 HTTP 请求
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_cleanup(client);

    // 处理响应数据
    if (response_data.data != NULL && response_data.data_len > 0) {
        // 分配解压缩缓冲区
        size_t decompressed_size = 0;
        char *decompressed_buf = (char *)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
        if (decompressed_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for decompressed buffer");
            heap_caps_free(response_data.data);
            return ESP_ERR_NO_MEM;
        }

        // 解压缩 GZIP 数据
        int decompress_result = network_gzip_decompress(response_data.data, response_data.data_len,
                                                        decompressed_buf, &decompressed_size, 8192);
        if (decompress_result != Z_OK) {
            ESP_LOGE(TAG, "Failed to decompress response data: %d", decompress_result);
            heap_caps_free(decompressed_buf);
            heap_caps_free(response_data.data);
            return ESP_FAIL;
        }

        // 解析 JSON 数据
        parse_weather_forecast(decompressed_buf, weather_forecast);

        // 释放内存
        heap_caps_free(decompressed_buf);
        heap_caps_free(response_data.data);
    } else {
        ESP_LOGE(TAG, "No response data received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}