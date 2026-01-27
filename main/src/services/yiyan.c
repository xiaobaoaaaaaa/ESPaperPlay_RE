/**
 * @file yiyan.c
 * @brief 一言（hitokoto）句子获取模块
 *
 * 本模块通过在线 API 获取随机的日本动画、漫画、游戏等来源的经典句子。
 * 支持功能：
 * - 从 hitokoto.cn API 获取随机句子
 * - 解析 JSON 响应并提取句子内容
 * - 自动内存管理
 *
 * @author
 * @date YYYY-MM-DD
 */

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

/** @brief 日志标签 */
#define TAG "yiyan"

/**
 * @brief 解析一言 API 响应
 *
 * 从 JSON 响应中提取 hitokoto（一言）字段，获取句子内容。
 * 如果解析成功，分配内存并复制字符串到结果指针。
 *
 * @param response API 响应 JSON 字符串
 * @param result 输出指针，包含提取的句子内容，失败时设为 NULL
 */
static void parse_yiyan(const char *response, char **result) {
    // 解析 JSON 响应
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        *result = NULL;
        return;
    }

    // 提取 hitokoto 字段
    cJSON *hitokoto = cJSON_GetObjectItem(json, "hitokoto");
    if (cJSON_IsString(hitokoto) && (hitokoto->valuestring != NULL)) {
        ESP_LOGI(TAG, "Hitokoto: %s", hitokoto->valuestring);
        // 分配内存并复制字符串
        *result = strdup(hitokoto->valuestring);
    } else {
        ESP_LOGE(TAG, "Hitokoto not found or not a string");
        *result = NULL;
    }

    // 释放 JSON 对象
    cJSON_Delete(json);
}

/**
 * @brief HTTP 客户端事件处理回调函数
 *
 * 处理 HTTP 请求的各个阶段事件，积累响应数据。
 * 使用 SPIRAM 进行内存分配以节省内部 RAM。
 *
 * @param evt HTTP 客户端事件结构体
 * @return esp_err_t 错误码
 */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
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
        // 接收响应数据块（仅处理非分块传输的响应）
        if (!esp_http_client_is_chunked_response(evt->client)) {
            // 重新分配缓冲区以容纳新数据
            char *new_buf =
                heap_caps_realloc(response_buf, total_len + evt->data_len + 1, MALLOC_CAP_SPIRAM);
            if (new_buf == NULL) {
                ESP_LOGE(TAG, "Failed to allocate response buffer");
                heap_caps_free(response_buf); // 释放已有的旧缓冲区
                response_buf = NULL;
                total_len = 0;
                return ESP_ERR_NO_MEM;
            }
            response_buf = new_buf;
            // 复制新数据到缓冲区
            memcpy(response_buf + total_len, evt->data, evt->data_len);
            total_len += evt->data_len;
            response_buf[total_len] = '\0'; // 字符串终止符
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        // 响应接收完成
        if (response_buf) {
            // 将数据复制到用户提供的缓冲区，使用 SPIRAM 保持内存分配一致
            *(char **)evt->user_data = heap_caps_malloc(total_len + 1, MALLOC_CAP_SPIRAM);
            if (*(char **)evt->user_data != NULL) {
                memcpy(*(char **)evt->user_data, response_buf, total_len);
                (*(char **)evt->user_data)[total_len] = '\0';
            }
            // 释放临时缓冲区
            heap_caps_free(response_buf);
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
 * @brief 获取一言句子
 *
 * 通过 HTTPS API 从 hitokoto.cn 获取随机的日本动画、漫画、游戏等来源的经典句子。
 * 返回值是动态分配的字符串，调用者需要手动释放。
 *
 * @param return_str 输出指针，包含获取的句子内容
 * @return esp_err_t 错误码，ESP_OK 表示成功
 */
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

    // 初始化 HTTP 客户端
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
        // 清理错误路径上可能分配的内存
        if (response_data != NULL) {
            heap_caps_free(response_data);
            response_data = NULL;
        }
        return err;
    }

    esp_http_client_cleanup(client);

    // 解析响应数据并返回一言字符串
    if (response_data != NULL) {
        parse_yiyan(response_data, return_str);
        // 使用 heap_caps_free 与分配方式对应
        heap_caps_free(response_data);
    }

    return ESP_OK;
}