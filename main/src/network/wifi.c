/**
 * @file wifi.c
 * @brief WiFi STA 模式初始化和连接管理模块
 *
 * 本模块处理 WiFi STA 模式的配置和连接，支持：
 * - 从配置存储中恢复已保存的 WiFi 凭证
 * - SmartConfig 自动配网功能
 * - WiFi 连接状态管理和重试机制
 *
 * 工作流程：
 * 1. 检查是否有已保存的 WiFi 配置
 * 2. 如果有配置，尝试连接到保存的 WiFi 网络
 * 3. 如果没有配置或连接失败，启动 SmartConfig 等待用户配网
 *
 * @author
 * @date YYYY-MM-DD
 */

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
#include <stdlib.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "config_manager.h"
#include "vars.h"
#include "wifi.h"

/** @brief 日志标签 */
#define TAG "wifi_sta"

/** @defgroup WIFI_EVENT_BITS WiFi 事件位定义 */
/** @{ */
#define WIFI_CONNECTED_BIT BIT0 /**< WiFi 已连接事件位 */
#define WIFI_FAIL_BIT BIT1      /**< WiFi 连接失败事件位 */
#define ESPTOUCH_DONE_BIT BIT2  /**< SmartConfig 完成事件位 */
/** @} */

#define MAXIMUM_RETRY 5               /**< 最大重试次数 */
#define SMARTCONFIG_TIMEOUT_MS 120000 /**< SmartConfig 超时时间（毫秒）120秒 */

/** @brief WiFi 事件组句柄 */
static EventGroupHandle_t s_wifi_event_group;
/** @brief 当前重试次数 */
static int s_retry_num = 0;

// WiFi 状态变量
bool wifi_connected = false;

/**
 * @brief SmartConfig 事件处理函数
 *
 * 处理 SmartConfig 过程中的各种事件：
 * - 扫描完成
 * - 发现信道
 * - 获取到 SSID 和密码
 * - 发送确认完成
 *
 * @param arg 事件处理函数参数
 * @param event_base 事件基础类型（SC_EVENT）
 * @param event_id 事件 ID
 * @param event_data 事件数据
 */
static void smartconfig_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                      void *event_data) {
    if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        // SmartConfig 获取到 SSID 和密码
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t *wifi_config = calloc(1, sizeof(wifi_config_t));
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};

        if (wifi_config == NULL) {
            ESP_LOGE(TAG, "Failed to allocate wifi_config");
            return;
        }

        // 清空并配置 WiFi 配置结构
        memcpy(wifi_config->sta.ssid, evt->ssid, sizeof(wifi_config->sta.ssid));
        memcpy(wifi_config->sta.password, evt->password, sizeof(wifi_config->sta.password));
        wifi_config->sta.threshold.authmode = WIFI_AUTH_OPEN;
        wifi_config->sta.pmf_cfg.capable = true;
        wifi_config->sta.pmf_cfg.required = false;

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);

        // 保存配置到系统存储
        sys_config_t *sys_config = calloc(1, sizeof(sys_config_t));
        if (sys_config == NULL) {
            ESP_LOGE(TAG, "Failed to allocate sys_config");
            free(wifi_config);
            return;
        }
        config_manager_get_config(sys_config);
        memset(sys_config->wifi.ssid, 0, sizeof(sys_config->wifi.ssid));
        memset(sys_config->wifi.password, 0, sizeof(sys_config->wifi.password));
        memcpy(sys_config->wifi.ssid, ssid, strlen((char *)ssid));
        memcpy(sys_config->wifi.password, password, strlen((char *)password));
        esp_err_t save_err = config_manager_save_config(sys_config);
        if (save_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save WiFi config: %s", esp_err_to_name(save_err));
        }

        // 断开现有连接并设置新的 WiFi 配置
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
        s_retry_num = 0; // 重置重试次数
        esp_wifi_connect();

        free(sys_config);
        free(wifi_config);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        // SmartConfig 已发送确认
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

/**
 * @brief WiFi STA 事件处理函数
 *
 * 处理 WiFi STA 连接过程中的事件：
 * - STA 启动
 * - STA 断开连接（含重试机制）
 * - 获取到 IP 地址
 *
 * @param arg 事件处理函数参数
 * @param event_base 事件基础类型（WIFI_EVENT 或 IP_EVENT）
 * @param event_id 事件 ID
 * @param event_data 事件数据
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // STA 启动，尝试连接到已保存的 WiFi 网络
        sys_config_t *sys_config = calloc(1, sizeof(sys_config_t));
        if (sys_config == NULL) {
            ESP_LOGE(TAG, "Failed to allocate sys_config");
            return;
        }

        config_manager_get_config(sys_config);
        if (strcmp(sys_config->wifi.ssid, "DefaultSSID") != 0 &&
            strlen(sys_config->wifi.ssid) > 0) {
            esp_wifi_connect();
        }
        free(sys_config);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected = false;
        // WiFi 断开连接，进行重试
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            // 重试次数已达上限
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        set_var_wifi_signal(0); // 设置信号强度为0，表示断开连接
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        // 成功获取 IP 地址
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // 重置重试计数
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 启动 SmartConfig 配网
 *
 * 初始化 SmartConfig 并启动等待用户通过手机应用进行配网。
 */
static void start_smartconfig(void) {
    s_retry_num = 0;
    // 注册 SmartConfig 事件处理函数
    ESP_ERROR_CHECK(
        esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));
    // 设置 SmartConfig 类型为 ESPTOUCH
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    // 创建并启动 SmartConfig
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
}

/**
 * @brief 获取 WiFi 连接状态
 *
 * @return true 如果已连接，否则返回 false
 */
bool is_wifi_connected() { return wifi_connected; }

/**
 * @brief 初始化 WiFi STA 模式
 *
 * 初始化 WiFi 网络栈，配置为 STA 模式，然后：
 * 1. 尝试从系统配置中恢复已保存的 WiFi 配置并连接
 * 2. 如果没有配置或连接失败，启动 SmartConfig 等待用户配网
 */
void wifi_init(void) {
    // 设置日志级别
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Initializing WiFi in STA mode...");

    // 创建 WiFi 事件组
    s_wifi_event_group = xEventGroupCreate();

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 创建默认的 WiFi STA 网络接口
    esp_netif_create_default_wifi_sta();

    // 初始化 WiFi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册 WiFi 事件处理函数
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // 获取系统配置
    sys_config_t sys_config;
    config_manager_get_config(&sys_config);

    // 检查是否有有效的 WiFi 配置
    bool is_configured =
        (strcmp(sys_config.wifi.ssid, "DefaultSSID") != 0 && strlen(sys_config.wifi.ssid) > 0);

    // 设置 WiFi 模式为 STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 如果有配置，应用该配置
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

    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 尝试连接到已保存的 WiFi 网络
    if (is_configured) {
        ESP_LOGI(TAG, "Connecting to saved SSID: %s", sys_config.wifi.ssid);
        // 等待连接成功或失败
        EventBits_t bits =
            xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                                pdFALSE, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            // 连接成功
            config_manager_get_config(&sys_config);
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", sys_config.wifi.ssid,
                     sys_config.wifi.password);
            return; // 连接成功，直接返回
        } else {
            // 连接失败，继续执行下面的 SmartConfig 逻辑
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, starting SmartConfig...",
                     sys_config.wifi.ssid);
        }
    } else {
        // 没有有效的配置，启动 SmartConfig
        ESP_LOGI(TAG, "No valid config found, starting SmartConfig...");
    }

    // 启动 SmartConfig 进行配网
    start_smartconfig();

    // 等待 SmartConfig 完成或 WiFi 连接成功
    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, pdFALSE,
                            pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        // WiFi 已连接
        config_manager_get_config(&sys_config);
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", sys_config.wifi.ssid,
                 sys_config.wifi.password);
    } else if (bits & ESPTOUCH_DONE_BIT) {
        // SmartConfig 已完成
        ESP_LOGI(TAG, "SmartConfig done");
        esp_smartconfig_stop();
    }
}