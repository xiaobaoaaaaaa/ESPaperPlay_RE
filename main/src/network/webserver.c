/**
 * @file webserver.c
 * @brief HTTP 网络服务器模块
 *
 * 本模块提供嵌入式网络服务器的实现，支持：
 * - 静态文件服务（HTML、CSS、JS、图片等）
 * - REST API 配置接口（获取和更新设备配置）
 * - 配置数据的 JSON 序列化和反序列化
 *
 * 主要功能：
 * - 通过 HTTP GET 请求获取设备配置信息
 * - 通过 HTTP POST 请求更新设备配置信息
 * - 提供 Web 文件静态服务，支持自动路由到 index.html
 *
 * @author
 * @date YYYY-MM-DD
 */

#include "webserver.h"

#include "cJSON.h"
#include "config_manager.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

/** @defgroup WEBSERVER_MACRO 网络服务器宏定义 */
/** @{ */
#define TAG "webserver"      /**< 日志标签 */
#define MAX_JSON_BODY 1024   /**< JSON 请求体最大字节数 */
#define FILE_READ_CHUNK 1024 /**< 文件读取块大小 */
/** @} */

/**
 * @brief 文件服务器数据结构
 *
 * 用于存储文件服务器的基础路径和上下文信息
 */
typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 16]; /**< 文件服务器基础路径 */
} file_server_data_t;

/** @brief 全局 HTTP 服务器句柄 */
static httpd_handle_t s_server = NULL;

/** @brief 全局文件服务器数据 */
static file_server_data_t *s_fs_data = NULL;

/**
 * @brief 根据文件路径设置 HTTP 响应的 Content-Type
 *
 * @param req HTTP 请求句柄
 * @param filepath 文件路径
 */
static void set_content_type_from_file(httpd_req_t *req, const char *filepath) {
    const char *type = "text/plain";

    if (strstr(filepath, ".html")) {
        type = "text/html";
    } else if (strstr(filepath, ".css")) {
        type = "text/css";
    } else if (strstr(filepath, ".js")) {
        type = "application/javascript";
    } else if (strstr(filepath, ".json")) {
        type = "application/json";
    } else if (strstr(filepath, ".png")) {
        type = "image/png";
    } else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        type = "image/jpeg";
    } else if (strstr(filepath, ".svg")) {
        type = "image/svg+xml";
    } else if (strstr(filepath, ".ttf")) {
        type = "font/ttf";
    }

    httpd_resp_set_type(req, type);
}

/**
 * @brief 向 HTTP 响应发送 404 未找到错误
 *
 * @param req HTTP 请求句柄
 * @return esp_err_t 错误码
 */
static esp_err_t send_not_found(httpd_req_t *req) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "File not found", HTTPD_RESP_USE_STRLEN);
}

/**
 * @brief HTTP GET 请求处理函数 - 用于提供静态文件服务
 *
 * 此函数处理对静态文件的 GET 请求，支持：
 * - 目录请求自动路由到 index.html
 * - 路径安全检查（防止目录遍历攻击）
 * - 自动 Content-Type 设置
 *
 * @param req HTTP 请求句柄
 * @return esp_err_t 错误码
 */
static esp_err_t file_get_handler(httpd_req_t *req) {
    file_server_data_t *fs = (file_server_data_t *)req->user_ctx;
    if (fs == NULL) {
        return ESP_FAIL;
    }

    char filepath[ESP_VFS_PATH_MAX + 32];

    // 检查路径安全性，防止目录遍历攻击
    if (strstr(req->uri, "..")) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    }

    size_t path_len = snprintf(filepath, sizeof(filepath), "%s%s", fs->base_path, req->uri);
    if (path_len >= sizeof(filepath)) {
        return httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
    }

    struct stat file_stat;

    // 尝试获取文件统计信息，如果失败则尝试路由到 index.html
    if (stat(filepath, &file_stat) == -1) {
        const bool request_root = (req->uri[strlen(req->uri) - 1] == '/');
        if (request_root || strcmp(req->uri, "/") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/index.html", fs->base_path);
        }
        if (stat(filepath, &file_stat) == -1) {
            return send_not_found(req);
        }
    }

    if (S_ISDIR(file_stat.st_mode)) {
        // 如果请求的是目录，则路由到 index.html
        snprintf(filepath, sizeof(filepath), "%s/index.html", fs->base_path);
        if (stat(filepath, &file_stat) == -1) {
            return send_not_found(req);
        }
    }

    set_content_type_from_file(req, filepath);

    // 打开文件并发送内容
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        return send_not_found(req);
    }

    char chunk[FILE_READ_CHUNK];
    ssize_t read_bytes;

    // 分块读取和发送文件内容
    while ((read_bytes = read(fd, chunk, sizeof(chunk))) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            close(fd);
            return ESP_FAIL;
        }
    }

    close(fd);
    httpd_resp_send_chunk(req, NULL, 0); // 发送终止块
    return ESP_OK;
}

/**
 * @brief 复制 JSON 中的字符串字段到目标缓冲区
 *
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param json_item JSON 对象项指针
 */
static void copy_string_field(char *dest, size_t dest_size, const cJSON *json_item) {
    if (cJSON_IsString(json_item) && json_item->valuestring) {
        strlcpy(dest, json_item->valuestring, dest_size);
    }
}

/**
 * @brief HTTP GET 请求处理函数 - 获取设备配置信息
 *
 * 返回 JSON 格式的设备配置，包括：
 * - 设备名称
 * - WiFi 配置
 * - 显示设置
 * - IP 定位配置
 * - 天气 API 配置
 *
 * @param req HTTP 请求句柄
 * @return esp_err_t 错误码
 */
static esp_err_t config_get_handler(httpd_req_t *req) {
    sys_config_t cfg;
    config_manager_get_config(&cfg);

    // 创建 JSON 根对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }

    // 创建各个配置子对象
    cJSON *wifi = cJSON_CreateObject();
    cJSON *display = cJSON_CreateObject();
    cJSON *ip_location = cJSON_CreateObject();
    cJSON *weather = cJSON_CreateObject();

    // 添加设备名称
    cJSON_AddStringToObject(root, "device_name", cfg.device_name);

    // 添加 WiFi 配置
    cJSON_AddStringToObject(wifi, "ssid", cfg.wifi.ssid);
    cJSON_AddStringToObject(wifi, "password", cfg.wifi.password);
    cJSON_AddItemToObject(root, "wifi", wifi);

    // 添加显示配置
    cJSON_AddNumberToObject(display, "fast_refresh_count", cfg.display.fast_refresh_count);
    cJSON_AddNumberToObject(display, "dither_mode", cfg.display.dither_mode);
    cJSON_AddItemToObject(root, "display", display);

    // 添加 IP 定位配置
    cJSON_AddStringToObject(ip_location, "id", cfg.ip_location.id);
    cJSON_AddStringToObject(ip_location, "key", cfg.ip_location.key);
    cJSON_AddItemToObject(root, "ip_location", ip_location);

    // 添加天气 API 配置
    cJSON_AddStringToObject(weather, "city", cfg.weather.city);
    cJSON_AddStringToObject(weather, "api_host", cfg.weather.api_host);
    cJSON_AddStringToObject(weather, "api_key", cfg.weather.api_key);
    cJSON_AddItemToObject(root, "weather", weather);

    // 将 JSON 对象转换为字符串
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build JSON");
    }

    // 设置响应类型并发送 JSON 数据
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return ret;
}

/**
 * @brief HTTP POST 请求处理函数 - 更新设备配置信息
 *
 * 接收 JSON 格式的配置数据，更新设备的各项配置，并保存到持久存储。
 * 支持部分更新，只有提供的字段才会被更新。
 *
 * @param req HTTP 请求句柄
 * @return esp_err_t 错误码
 */
static esp_err_t config_post_handler(httpd_req_t *req) {
    // 检查请求体大小
    if (req->content_len >= MAX_JSON_BODY) {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
    }

    // 读取 JSON 请求体
    char buf[MAX_JSON_BODY] = {0};
    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
        }
        received += ret;
    }
    buf[received] = '\0';

    // 解析 JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // 获取当前配置
    sys_config_t cfg;
    config_manager_get_config(&cfg);

    cJSON *item = NULL;

    // 更新设备名称
    item = cJSON_GetObjectItemCaseSensitive(root, "device_name");
    copy_string_field(cfg.device_name, sizeof(cfg.device_name), item);

    // 更新 WiFi 配置
    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    if (cJSON_IsObject(wifi)) {
        copy_string_field(cfg.wifi.ssid, sizeof(cfg.wifi.ssid),
                          cJSON_GetObjectItemCaseSensitive(wifi, "ssid"));
        copy_string_field(cfg.wifi.password, sizeof(cfg.wifi.password),
                          cJSON_GetObjectItemCaseSensitive(wifi, "password"));
    }

    // 更新显示配置
    cJSON *display = cJSON_GetObjectItemCaseSensitive(root, "display");
    if (cJSON_IsObject(display)) {
        item = cJSON_GetObjectItemCaseSensitive(display, "fast_refresh_count");
        if (cJSON_IsNumber(item)) {
            cfg.display.fast_refresh_count = item->valueint;
        }
        item = cJSON_GetObjectItemCaseSensitive(display, "dither_mode");
        if (cJSON_IsNumber(item)) {
            cfg.display.dither_mode = (dither_mode_t)item->valueint;
        }
    }

    // 更新 IP 定位配置
    cJSON *ip_location = cJSON_GetObjectItemCaseSensitive(root, "ip_location");
    if (cJSON_IsObject(ip_location)) {
        copy_string_field(cfg.ip_location.id, sizeof(cfg.ip_location.id),
                          cJSON_GetObjectItemCaseSensitive(ip_location, "id"));
        copy_string_field(cfg.ip_location.key, sizeof(cfg.ip_location.key),
                          cJSON_GetObjectItemCaseSensitive(ip_location, "key"));
    }

    // 更新天气 API 配置
    cJSON *weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
    if (cJSON_IsObject(weather)) {
        copy_string_field(cfg.weather.city, sizeof(cfg.weather.city),
                          cJSON_GetObjectItemCaseSensitive(weather, "city"));
        copy_string_field(cfg.weather.api_host, sizeof(cfg.weather.api_host),
                          cJSON_GetObjectItemCaseSensitive(weather, "api_host"));
        copy_string_field(cfg.weather.api_key, sizeof(cfg.weather.api_key),
                          cJSON_GetObjectItemCaseSensitive(weather, "api_key"));
    }

    cJSON_Delete(root);

    // 保存更新的配置
    esp_err_t err = config_manager_save_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }

    // 返回成功响应
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

/**
 * @brief 启动 HTTP 网络服务器
 *
 * 初始化并启动 HTTP 服务器，注册以下请求处理函数：
 * - GET  /api/config      - 获取设备配置
 * - POST /api/config      - 更新设备配置
 * - GET  /{*}               - 提供静态文件服务
 *
 * @param base_path 文件服务器的基础路径，若为 NULL 则使用 "/flash"
 * @return esp_err_t 错误码，ESP_OK 表示成功
 */
esp_err_t webserver_start(const char *base_path) {
    // 检查服务器是否已启动
    if (s_server != NULL) {
        return ESP_OK;
    }

    // 配置 HTTP 服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    // 启动 HTTP 服务器
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    // 分配文件服务器上下文
    s_fs_data = calloc(1, sizeof(file_server_data_t));
    if (s_fs_data == NULL) {
        ESP_LOGE(TAG, "No memory for file server context");
        webserver_stop();
        return ESP_ERR_NO_MEM;
    }

    // 设置文件服务器基础路径
    const char *root = (base_path != NULL) ? base_path : "/flash";
    strlcpy(s_fs_data->base_path, root, sizeof(s_fs_data->base_path));

    // 注册 API 处理函数
    httpd_uri_t api_get = {
        .uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler, .user_ctx = NULL};
    httpd_uri_t api_post = {.uri = "/api/config",
                            .method = HTTP_POST,
                            .handler = config_post_handler,
                            .user_ctx = NULL};

    // 注册文件服务处理函数
    httpd_uri_t file_get = {
        .uri = "/*", .method = HTTP_GET, .handler = file_get_handler, .user_ctx = s_fs_data};

    // 添加处理函数到服务器
    httpd_register_uri_handler(s_server, &api_get);
    httpd_register_uri_handler(s_server, &api_post);
    httpd_register_uri_handler(s_server, &file_get);

    return ESP_OK;
}

/**
 * @brief 停止 HTTP 网络服务器
 *
 * 关闭 HTTP 服务器并释放相关资源。如果服务器未启动，此函数无操作。
 */
void webserver_stop() {
    // 停止 HTTP 服务器
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }

    // 释放文件服务器数据结构
    if (s_fs_data) {
        free(s_fs_data);
        s_fs_data = NULL;
    }
}
