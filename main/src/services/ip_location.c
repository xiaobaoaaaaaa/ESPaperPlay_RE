/**
 * @file ip_location.c
 * @brief IP 地址定位服务模块
 *
 * 本模块通过查询 IP 地址来获取地理位置信息，包括：
 * - 国家、省份、城市、区县等行政区划
 * - ISP 运营商信息
 * - 经纬度坐标
 *
 * 该模块使用在线 API 服务进行查询，支持配置 API ID 和密钥。
 *
 * @author
 * @date YYYY-MM-DD
 */

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#include "config_manager.h"
#include "ip_location.h"

/** @brief 日志标签 */
#define TAG "IP_LOCATION"

/**
 * @brief 解析 IP 定位 API 响应的 JSON 数据
 *
 * 将 JSON 响应解析为 location_t 结构体，提取所有地理位置信息。
 * 如果某些字段在 JSON 中不存在或类型不符合预期，则保持默认值。
 *
 * @param json JSON 响应字符串
 * @param location 输出参数，解析后的位置信息结构体
 */
static void parse_location(const char *json, location_t *location) {
    if (location == NULL) {
        ESP_LOGE(TAG, "location pointer is NULL");
        return;
    }

    // 解析 JSON 字符串
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    // 初始化所有字段为零
    memset(location, 0, sizeof(location_t));

    // 提取各个字段信息
    cJSON *item = NULL;

    item = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (item != NULL && cJSON_IsNumber(item)) {
        location->code = item->valueint;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "zhou");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->continent, item->valuestring, sizeof(location->continent) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "zhoucode");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->continent_code, item->valuestring, sizeof(location->continent_code) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "guo");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->country, item->valuestring, sizeof(location->country) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "guocode");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->country_code, item->valuestring, sizeof(location->country_code) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "sheng");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->province, item->valuestring, sizeof(location->province) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "shengcode");
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            location->province_code = item->valueint;
            location->has_province_code = true;
        } else if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            location->province_code = (int)strtol(item->valuestring, NULL, 10);
            location->has_province_code = true;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "shi");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->city, item->valuestring, sizeof(location->city) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "shicode");
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            location->city_code = item->valueint;
            location->has_city_code = true;
        } else if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            location->city_code = (int)strtol(item->valuestring, NULL, 10);
            location->has_city_code = true;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "qu");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->district, item->valuestring, sizeof(location->district) - 1);
        location->has_district = true;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "qucode");
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            location->district_code = item->valueint;
            location->has_district_code = true;
        } else if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            location->district_code = (int)strtol(item->valuestring, NULL, 10);
            location->has_district_code = true;
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "isp");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->isp, item->valuestring, sizeof(location->isp) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "lat");
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            location->latitude = item->valuedouble;
        } else if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            location->latitude = strtod(item->valuestring, NULL);
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "lon");
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            location->longitude = item->valuedouble;
        } else if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            location->longitude = strtod(item->valuestring, NULL);
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "msg");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->message, item->valuestring, sizeof(location->message) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "ip");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->ip, item->valuestring, sizeof(location->ip) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "td");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->td, item->valuestring, sizeof(location->td) - 1);
    }

    // 记录解析结果日志
    ESP_LOGI(TAG, "Location: %s-%s-%s-%s-%s-%s", location->continent, location->country,
             location->province, location->city, location->district, location->isp);

    // 释放 JSON 对象
    cJSON_Delete(root);
}

/**
 * @brief HTTP 客户端事件处理回调函数
 *
 * 处理 HTTP 请求的各个阶段事件，包括数据接收、响应完成等。
 * 使用内存重新分配策略来接收完整的响应数据。
 *
 * @param evt HTTP 客户端事件结构体
 * @return esp_err_t 错误码
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    // 使用静态变量缓存响应数据
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
        char *tmp = heap_caps_realloc(response_buf, total_len + 1, MALLOC_CAP_SPIRAM);
        if (tmp == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
            heap_caps_free(response_buf);
            response_buf = NULL;
            return ESP_ERR_NO_MEM;
        }
        response_buf = tmp;
        memcpy(response_buf + total_len - evt->data_len, evt->data, evt->data_len);
        response_buf[total_len] = '\0';
        break;

    case HTTP_EVENT_ON_FINISH:
        // 响应接收完成
        if (response_buf != NULL) {
            // 复制数据到用户指定的指针
            *(char **)evt->user_data = strdup(response_buf);
            heap_caps_free(response_buf);
            response_buf = NULL;
            total_len = 0;
        }
        break;

    case HTTP_EVENT_ERROR:
    case HTTP_EVENT_DISCONNECTED:
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
 * @brief 获取 IP 地址的地理位置信息
 *
 * 通过 HTTPS API 查询给定 IP 地址的地理位置信息，返回包含国家、
 * 省份、城市等详细位置信息的结构体。
 *
 * @param ip 要查询的 IP 地址字符串，为 NULL 时查询当前客户端 IP
 * @param location 输出参数，包含地理位置信息的结构体
 * @return esp_err_t 错误码，ESP_OK 表示成功
 */
esp_err_t get_location(const char *ip, location_t *location) {
    char *response_data = NULL;

    // 获取 API 配置
    sys_config_t config;
    config_manager_get_config(&config);

    // 使用配置中的 API ID 和密钥，如果未配置则使用默认值
    const char *api_id = (config.ip_location.id[0] != '\0') ? config.ip_location.id : "88888888";
    const char *api_key = (config.ip_location.key[0] != '\0') ? config.ip_location.key : "88888888";

    char url[256];
    snprintf(url, sizeof(url),
             "https://cn.apihz.cn/api/ip/"
             "chaapi.php?spm=a2c6h.12873639.article-detail.5.113a57d83nebvB&id=%s&key="
             "%s&ip=%s",
             api_id, api_key, (ip != NULL) ? ip : "");

    // 配置 HTTP 客户端
    esp_http_client_config_t config_http = {.url = url,
                                            .event_handler = http_event_handler,
                                            .crt_bundle_attach = esp_crt_bundle_attach,
                                            .user_data = &response_data};

    // 初始化 HTTP 客户端
    esp_http_client_handle_t client = esp_http_client_init(&config_http);

    // 执行 HTTP 请求
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

    // 解析响应数据
    if (response_data != NULL) {
        parse_location(response_data, location);
        free(response_data);
    } else {
        ESP_LOGE(TAG, "No response data received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}