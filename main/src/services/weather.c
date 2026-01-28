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
#include "zlib.h"
#include <stdlib.h>
#include <string.h>

#include "config_manager.h"
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
 * @brief GZIP 数据解压缩函数
 *
 * 使用 zlib 库对 GZIP 压缩的数据进行解压缩。
 *
 * @param in_buf 压缩数据输入缓冲区
 * @param in_size 压缩数据大小
 * @param out_buf 解压缩数据输出缓冲区
 * @param out_size 输出指针，保存解压缩后的数据长度
 * @param out_buf_size 输出缓冲区大小
 * @return int 错误码，Z_OK 表示成功
 */
static int network_gzip_decompress(void *in_buf, size_t in_size, void *out_buf, size_t *out_size,
                                   size_t out_buf_size) {
    int err = 0;
    // 初始化 zlib 解压缩流
    z_stream d_stream = {0};
    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    d_stream.next_in = (Bytef *)in_buf;
    d_stream.avail_in = 0;
    d_stream.next_out = (Bytef *)out_buf;
    d_stream.avail_out = 0;

    // 初始化解压缩器（16 + MAX_WBITS 用于处理 GZIP 格式）
    err = inflateInit2(&d_stream, 16 + MAX_WBITS);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateInit2 failed: %d", err);
        return err;
    }

    d_stream.avail_in = in_size;
    d_stream.avail_out = out_buf_size - 1;

    while (d_stream.total_out < out_buf_size - 1 && d_stream.total_in < in_size) {
        // 执行解压缩操作
        err = inflate(&d_stream, Z_NO_FLUSH);

        if (err == Z_STREAM_END) {
            // 解压缩完成
            break;
        }

        if (err != Z_OK) {
            ESP_LOGE(TAG, "inflate failed: %d", err);
            inflateEnd(&d_stream);
            return err;
        }

        if (d_stream.avail_out == 0) {
            ESP_LOGW(TAG, "Output buffer full during decompression");
            break;
        }

        if (d_stream.avail_in == 0 && d_stream.total_in < in_size) {
            d_stream.avail_in = in_size - d_stream.total_in;
        }
    }

    // 清理解压缩流
    err = inflateEnd(&d_stream);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateEnd failed: %d", err);
        return err;
    }

    // 记录解压缩结果
    *out_size = d_stream.total_out;
    ((char *)out_buf)[*out_size] = '\0';

    return Z_OK;
}

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
        weather_now->icon = (uint8_t)item->valueint;
    } else if (cJSON_IsString(item)) {
        weather_now->icon = (uint8_t)atoi(item->valuestring);
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