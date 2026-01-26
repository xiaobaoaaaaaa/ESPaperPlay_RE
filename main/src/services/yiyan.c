#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vars.h"

#include "yiyan.h"

#define TAG "yiyan"

// 一言响应缓冲区大小（一言API返回的JSON通常不超过1KB）
#define YIYAN_RESPONSE_BUF_SIZE 2048

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

// 响应缓冲区结构体
typedef struct {
    char *buffer;
    int len;
    int max_len;
} response_buffer_t;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    response_buffer_t *resp_buf = (response_buffer_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        // 重置缓冲区
        if (resp_buf && resp_buf->buffer) {
            resp_buf->len = 0;
            resp_buf->buffer[0] = '\0';
        }
        break;

    case HTTP_EVENT_ON_DATA:
        if (resp_buf && resp_buf->buffer) {
            // 检查是否有足够空间
            if (resp_buf->len + evt->data_len < resp_buf->max_len) {
                memcpy(resp_buf->buffer + resp_buf->len, evt->data, evt->data_len);
                resp_buf->len += evt->data_len;
                resp_buf->buffer[resp_buf->len] = '\0'; // null 终止
            } else {
                ESP_LOGE(TAG, "Response buffer overflow, data truncated");
            }
        }
        break;

    case HTTP_EVENT_ON_FINISH:
    case HTTP_EVENT_ERROR:
    case HTTP_EVENT_DISCONNECTED:
        // 无需特殊处理，缓冲区由调用者管理
        break;

    default:
        break;
    }

    return ESP_OK;
}

esp_err_t get_yiyan(char **return_str) {
    *return_str = NULL;

    // 分配响应缓冲区
    char *response_buffer = heap_caps_malloc(YIYAN_RESPONSE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    response_buffer[0] = '\0';

    // 初始化响应缓冲区结构
    response_buffer_t resp_buf = {
        .buffer = response_buffer,
        .len = 0,
        .max_len = YIYAN_RESPONSE_BUF_SIZE,
    };

    // 建立 HTTP 客户端配置
    esp_http_client_config_t config = {
        .url = "https://v1.hitokoto.cn/",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &resp_buf,
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
        free(response_buffer);
        return err;
    }

    esp_http_client_cleanup(client);

    if (resp_buf.len > 0) {
        parse_yiyan(response_buffer, return_str);
    }

    free(response_buffer);
    return ESP_OK;
}