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

#define TAG "webserver"
#define MAX_JSON_BODY 1024
#define FILE_READ_CHUNK 1024

typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 16];
} file_server_data_t;

static httpd_handle_t s_server = NULL;
static file_server_data_t *s_fs_data = NULL;

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

static esp_err_t send_not_found(httpd_req_t *req) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "File not found", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    file_server_data_t *fs = (file_server_data_t *)req->user_ctx;
    if (fs == NULL) {
        return ESP_FAIL;
    }

    char filepath[ESP_VFS_PATH_MAX + 32];
    if (strstr(req->uri, "..")) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    }

    size_t path_len = snprintf(filepath, sizeof(filepath), "%s%s", fs->base_path, req->uri);
    if (path_len >= sizeof(filepath)) {
        return httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
    }

    struct stat file_stat;
    if (stat(filepath, &file_stat) == -1) {
        // Fallback to index.html when requesting root or folder
        const bool request_root = (req->uri[strlen(req->uri) - 1] == '/');
        if (request_root || strcmp(req->uri, "/") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/index.html", fs->base_path);
        }
        if (stat(filepath, &file_stat) == -1) {
            return send_not_found(req);
        }
    }

    if (S_ISDIR(file_stat.st_mode)) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", fs->base_path);
        if (stat(filepath, &file_stat) == -1) {
            return send_not_found(req);
        }
    }

    set_content_type_from_file(req, filepath);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        return send_not_found(req);
    }

    char chunk[FILE_READ_CHUNK];
    ssize_t read_bytes;
    while ((read_bytes = read(fd, chunk, sizeof(chunk))) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            close(fd);
            return ESP_FAIL;
        }
    }

    close(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static void copy_string_field(char *dest, size_t dest_size, const cJSON *json_item) {
    if (cJSON_IsString(json_item) && json_item->valuestring) {
        strlcpy(dest, json_item->valuestring, dest_size);
    }
}

static esp_err_t config_get_handler(httpd_req_t *req) {
    sys_config_t cfg;
    config_manager_get_config(&cfg);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }

    cJSON *wifi = cJSON_CreateObject();
    cJSON *display = cJSON_CreateObject();
    cJSON *ip_location = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "device_name", cfg.device_name);

    cJSON_AddStringToObject(wifi, "ssid", cfg.wifi.ssid);
    cJSON_AddStringToObject(wifi, "password", cfg.wifi.password);
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON_AddNumberToObject(display, "fast_refresh_count", cfg.display.fast_refresh_count);
    cJSON_AddNumberToObject(display, "dither_mode", cfg.display.dither_mode);
    cJSON_AddItemToObject(root, "display", display);

    cJSON_AddStringToObject(ip_location, "id", cfg.ip_location.id);
    cJSON_AddStringToObject(ip_location, "key", cfg.ip_location.key);
    cJSON_AddItemToObject(root, "ip_location", ip_location);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build JSON");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return ret;
}

static esp_err_t config_post_handler(httpd_req_t *req) {
    if (req->content_len >= MAX_JSON_BODY) {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
    }

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

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    sys_config_t cfg;
    config_manager_get_config(&cfg);

    cJSON *item = NULL;

    item = cJSON_GetObjectItemCaseSensitive(root, "device_name");
    copy_string_field(cfg.device_name, sizeof(cfg.device_name), item);

    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    if (cJSON_IsObject(wifi)) {
        copy_string_field(cfg.wifi.ssid, sizeof(cfg.wifi.ssid),
                          cJSON_GetObjectItemCaseSensitive(wifi, "ssid"));
        copy_string_field(cfg.wifi.password, sizeof(cfg.wifi.password),
                          cJSON_GetObjectItemCaseSensitive(wifi, "password"));
    }

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

    cJSON *ip_location = cJSON_GetObjectItemCaseSensitive(root, "ip_location");
    if (cJSON_IsObject(ip_location)) {
        copy_string_field(cfg.ip_location.id, sizeof(cfg.ip_location.id),
                          cJSON_GetObjectItemCaseSensitive(ip_location, "id"));
        copy_string_field(cfg.ip_location.key, sizeof(cfg.ip_location.key),
                          cJSON_GetObjectItemCaseSensitive(ip_location, "key"));
    }

    cJSON_Delete(root);

    esp_err_t err = config_manager_save_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t webserver_start(const char *base_path) {
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    s_fs_data = calloc(1, sizeof(file_server_data_t));
    if (s_fs_data == NULL) {
        ESP_LOGE(TAG, "No memory for file server context");
        webserver_stop();
        return ESP_ERR_NO_MEM;
    }

    const char *root = (base_path != NULL) ? base_path : "/flash";
    strlcpy(s_fs_data->base_path, root, sizeof(s_fs_data->base_path));

    httpd_uri_t api_get = {
        .uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler, .user_ctx = NULL};
    httpd_uri_t api_post = {.uri = "/api/config",
                            .method = HTTP_POST,
                            .handler = config_post_handler,
                            .user_ctx = NULL};
    httpd_uri_t file_get = {
        .uri = "/*", .method = HTTP_GET, .handler = file_get_handler, .user_ctx = s_fs_data};

    httpd_register_uri_handler(s_server, &api_get);
    httpd_register_uri_handler(s_server, &api_post);
    httpd_register_uri_handler(s_server, &file_get);

    return ESP_OK;
}

void webserver_stop() {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    if (s_fs_data) {
        free(s_fs_data);
        s_fs_data = NULL;
    }
}
