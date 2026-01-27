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

#define TAG "weather"

typedef struct {
    void *data;
    size_t data_len;
} response_data_t;

static int network_gzip_decompress(void *in_buf, size_t in_size, void *out_buf, size_t *out_size,
                                   size_t out_buf_size) {
    int err = 0;
    z_stream d_stream = {0}; /* decompression stream */
    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    d_stream.next_in = (Bytef *)in_buf;
    d_stream.avail_in = 0;
    d_stream.next_out = (Bytef *)out_buf;
    d_stream.avail_out = 0;

    err = inflateInit2(&d_stream, 16 + MAX_WBITS);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateInit2 failed: %d", err);
        return err;
    }

    d_stream.avail_in = in_size;
    d_stream.avail_out = out_buf_size - 1;

    while (d_stream.total_out < out_buf_size - 1 && d_stream.total_in < in_size) {
        err = inflate(&d_stream, Z_NO_FLUSH);

        if (err == Z_STREAM_END) {
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

    err = inflateEnd(&d_stream);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateEnd failed: %d", err);
        return err;
    }

    *out_size = d_stream.total_out;
    ((char *)out_buf)[*out_size] = '\0';

    return Z_OK;
}

static void parse_weather_now(const char *json, weather_now_t *weather_now) {
    if (weather_now == NULL || json == NULL) {
        ESP_LOGE(TAG, "Invalid pointer parameters");
        return;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse weather JSON");
        return;
    }

    // 初始化所有字段
    memset(weather_now, 0, sizeof(weather_now_t));

    // 查找 now 字段
    cJSON *now = cJSON_GetObjectItemCaseSensitive(root, "now");
    if (now == NULL || !cJSON_IsObject(now)) {
        ESP_LOGE(TAG, "Missing 'now' object in weather response");
        cJSON_Delete(root);
        return;
    }

    // 提取各个字段
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

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Weather data parsed successfully: %.1f°C, %s", weather_now->temperature,
             weather_now->text);
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
        if (response_buf != NULL && total_len > 0) {
            response_data_t *response_data = (response_data_t *)evt->user_data;
            response_data->data = response_buf;
            response_data->data_len = total_len;
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

esp_err_t get_weather_now(location_t *location, weather_now_t *weather_now) {
    response_data_t response_data = {.data = NULL, .data_len = 0};

    sys_config_t sys_config;
    config_manager_get_config(&sys_config);

    if (strlen(sys_config.weather.api_host) == 0 || strlen(sys_config.weather.api_key) == 0) {
        ESP_LOGE(TAG, "Weather API host or key is not configured");
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://%s/v7/weather/now?location=%.2f,%.2f&key=%s",
             sys_config.weather.api_host, location->longitude, location->latitude,
             sys_config.weather.api_key);

    esp_http_client_config_t config = {.url = url,
                                       .event_handler = http_event_handler,
                                       .crt_bundle_attach = esp_crt_bundle_attach,
                                       .user_data = &response_data};

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

    if (response_data.data != NULL && response_data.data_len > 0) {
        // 解压缩
        size_t decompressed_size = 0;
        char *decompressed_buf = (char *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
        if (decompressed_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for decompressed buffer");
            heap_caps_free(response_data.data);
            return ESP_ERR_NO_MEM;
        }

        int decompress_result = network_gzip_decompress(response_data.data, response_data.data_len,
                                                        decompressed_buf, &decompressed_size, 4096);
        if (decompress_result != Z_OK) {
            ESP_LOGE(TAG, "Failed to decompress response data: %d", decompress_result);
            heap_caps_free(decompressed_buf);
            heap_caps_free(response_data.data);
            return ESP_FAIL;
        }

        // 解析JSON数据
        parse_weather_now(decompressed_buf, weather_now);

        heap_caps_free(decompressed_buf);
        heap_caps_free(response_data.data);
    } else {
        ESP_LOGE(TAG, "No response data received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}