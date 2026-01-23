#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "config_manager.h"
#include "wifi.h"

#define TAG "wifi_sta"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESPTOUCH_DONE_BIT BIT2
#define MAXIMUM_RETRY 5
#define SMARTCONFIG_TIMEOUT_MS 120000 // 120 秒

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/**
 * @brief SmartConfig 事件处理函数
 *
 * 处理 SmartConfig 过程中的各种事件，如扫描完成、发现信道、获取到 SSID 和密码等。
 */
static void smartconfig_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                      void *event_data) {
    if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);

        // Save to system config
        sys_config_t sys_config;
        config_manager_get_config(&sys_config);
        memset(sys_config.wifi.ssid, 0, sizeof(sys_config.wifi.ssid));
        memset(sys_config.wifi.password, 0, sizeof(sys_config.wifi.password));
        memcpy(sys_config.wifi.ssid, ssid, strlen((char *)ssid));
        memcpy(sys_config.wifi.password, password, strlen((char *)password));
        config_manager_save_config(&sys_config);

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        s_retry_num = 0; // 重置重试次数
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

/**
 * @brief WiFi STA 事件处理函数
 *
 * 处理 WiFi STA 连接过程中的事件，如启动、断开连接、获取 IP 等。
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        sys_config_t sys_config;
        config_manager_get_config(&sys_config);
        if (strcmp(sys_config.wifi.ssid, "DefaultSSID") != 0 && strlen(sys_config.wifi.ssid) > 0) {
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 启动 SmartConfig
 */
static void start_smartconfig(void) {
    s_retry_num = 0;
    ESP_ERROR_CHECK(
        esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
}

/**
 * @brief 初始化WiFi STA模式
 *
 * 1. 初始化网络接口和事件循环。
 * 2. 尝试从系统配置中读取 WiFi 配置。
 * 3. 如果有配置，尝试连接 WiFi。
 * 4. 如果连接失败或没有配置，启动 SmartConfig 等待配网。
 */
void wifi_init(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Initializing WiFi in STA mode...");

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    sys_config_t sys_config;
    config_manager_get_config(&sys_config);

    bool is_configured =
        (strcmp(sys_config.wifi.ssid, "DefaultSSID") != 0 && strlen(sys_config.wifi.ssid) > 0);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (is_configured) {
        wifi_config_t wifi_config = {.sta = {
                                         .threshold.authmode = WIFI_AUTH_OPEN,
                                         .pmf_cfg = {.capable = true, .required = false},
                                     }};
        memcpy(wifi_config.sta.ssid, sys_config.wifi.ssid, strlen(sys_config.wifi.ssid));
        memcpy(wifi_config.sta.password, sys_config.wifi.password,
               strlen(sys_config.wifi.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    if (is_configured) {
        ESP_LOGI(TAG, "Connecting to saved SSID: %s", sys_config.wifi.ssid);
        EventBits_t bits =
            xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                                pdFALSE, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            config_manager_get_config(&sys_config); // Refresh to get latest credentials
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", sys_config.wifi.ssid,
                     sys_config.wifi.password);
            return; // 连接成功，直接返回
        } else {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, starting SmartConfig...",
                     sys_config.wifi.ssid);
            // 连接失败，继续执行下面的 SmartConfig 逻辑
        }
    } else {
        ESP_LOGI(TAG, "No valid config found, starting SmartConfig...");
    }

    // 启动 SmartConfig
    start_smartconfig();

    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, pdFALSE,
                            pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        config_manager_get_config(&sys_config); // SmartConfig may have updated credentials
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", sys_config.wifi.ssid,
                 sys_config.wifi.password);
    } else if (bits & ESPTOUCH_DONE_BIT) {
        ESP_LOGI(TAG, "SmartConfig done");
        esp_smartconfig_stop();
    }
}