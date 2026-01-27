#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>

#include "config_manager.h"
#include "ip_location.h"

#define TAG "IP_LOCATION"

static void parse_location(const char *json, location_t *location) {
    if (location == NULL) {
        ESP_LOGE(TAG, "location pointer is NULL");
        return;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    // 初始化所有字段
    memset(location, 0, sizeof(location_t));

    // 提取各个字段
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
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->province_code, item->valuestring, sizeof(location->province_code) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "shi");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->city, item->valuestring, sizeof(location->city) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "shicode");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->city_code, item->valuestring, sizeof(location->city_code) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "qu");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->district, item->valuestring, sizeof(location->district) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "qucode");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->district_code, item->valuestring, sizeof(location->district_code) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "isp");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->isp, item->valuestring, sizeof(location->isp) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "lat");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->latitude, item->valuestring, sizeof(location->latitude) - 1);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "lon");
    if (item != NULL && cJSON_IsString(item) && item->valuestring) {
        strncpy(location->longitude, item->valuestring, sizeof(location->longitude) - 1);
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

    ESP_LOGI(TAG, "Location: %s-%s-%s-%s-%s-%s", location->continent, location->country,
             location->province, location->city, location->district, location->isp);

    cJSON_Delete(root);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static char *response_buf = NULL;
    static int total_len = 0;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        total_len = 0;
        heap_caps_free(response_buf); // 清理上一次的
        response_buf = NULL;
        break;

    case HTTP_EVENT_ON_DATA:
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
        if (response_buf != NULL) {
            // 传给解析函数
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

esp_err_t get_location(const char *ip, location_t *location) {
    char *response_data = NULL;

    // 从 config_manager 获取 id 和 key
    sys_config_t config;
    config_manager_get_config(&config);

    // 如果配置中的 id 或 key 为空，使用默认值
    const char *api_id = (config.ip_location.id[0] != '\0') ? config.ip_location.id : "88888888";
    const char *api_key = (config.ip_location.key[0] != '\0') ? config.ip_location.key : "88888888";

    char url[256];
    snprintf(url, sizeof(url),
             "https://cn.apihz.cn/api/ip/"
             "chaapi.php?spm=a2c6h.12873639.article-detail.5.113a57d83nebvB&id=%s&key="
             "%s&ip=%s",
             api_id, api_key, (ip != NULL) ? ip : "");

    // 构建 HTTP 客户端配置
    esp_http_client_config_t config_http = {.url = url,
                                            .event_handler = http_event_handler,
                                            .crt_bundle_attach = esp_crt_bundle_attach,
                                            .user_data = &response_data};

    esp_http_client_handle_t client = esp_http_client_init(&config_http);

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

    if (response_data != NULL) {
        parse_location(response_data, location);
        free(response_data);
    } else {
        ESP_LOGE(TAG, "No response data received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}