#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vars.h"

#include "yiyan.h"

#define TAG "yiyan"

// 解析一言内容
static void parse_yiyan(const char *response, char **result) {
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        *result = NULL;
        return;
    }

    cJSON *hitokoto = cJSON_GetObjectItem(json, "hitokoto");
    if (cJSON_IsString(hitokoto) && (hitokoto->valuestring != NULL)) {
        ESP_LOGI(TAG, "Hitokoto: %s", hitokoto->valuestring);
        *result = strdup(hitokoto->valuestring); // 分配内存并复制字符串
    } else {
        ESP_LOGE(TAG, "Hitokoto not found or not a string");
        *result = NULL;
    }

    cJSON_Delete(json);
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *response_buf = NULL;
    static int total_len = 0;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        total_len = 0;
        heap_caps_free(response_buf); // 清理上一次的
        response_buf = NULL;
        break;

    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            response_buf =
                heap_caps_realloc(response_buf, total_len + evt->data_len + 1, MALLOC_CAP_SPIRAM);
            if (response_buf == NULL) {
                ESP_LOGE(TAG, "Failed to allocate response buffer");
                total_len = 0;
                return ESP_ERR_NO_MEM;
            }
            memcpy(response_buf + total_len, evt->data, evt->data_len);
            total_len += evt->data_len;
            response_buf[total_len] = '\0'; // null 终止
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        if (response_buf) {
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

esp_err_t get_yiyan(char **return_str) {
    *return_str = NULL;

    char *response_data = NULL;

    // 建立 HTTP 客户端配置
    esp_http_client_config_t config = {
        .url = "https://v1.hitokoto.cn/",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &response_data,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 发送 GET 请求
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

    // 解析响应数据并返回一言字符串
    if (response_data != NULL) {
        parse_yiyan(response_data, return_str);
        free(response_data);
    }

    return ESP_OK;
}