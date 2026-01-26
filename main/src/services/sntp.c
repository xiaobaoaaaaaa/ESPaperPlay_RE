#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sntp.h"

static const char *TAG = "sntp";

/**
 * @brief 获取网络时间
 *
 * 通过SNTP协议从NTP服务器获取当前时间并同步到系统
 */
void obtain_time(void);

/**
 * @brief 初始化SNTP服务
 *
 * 配置并启动SNTP客户端以同步网络时间
 */
static void initialize_sntp(void);

/**
 * @brief 时间同步回调函数
 *
 * 当系统时间通过SNTP同步后被调用
 * @param tv 同步后的时间值
 */
void time_sync_notification_cb(struct timeval *tv) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "Time synced from NTP server, current time: %s", asctime(&timeinfo));
}

/**
 * @brief 初始化并同步系统时间
 *
 * 等待WiFi连接成功后通过NTP同步时间
 */
void time_init(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    obtain_time();
    time(&now);
    localtime_r(&now, &timeinfo);

    setenv("TZ", "CST-8", 1);
    tzset();
}

/**
 * @brief 通过SNTP获取网络时间
 *
 * 初始化SNTP客户端并等待时间同步完成，最多重试10次
 */
void obtain_time(void) {
    initialize_sntp();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time sync (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    setenv("TZ", "CST-8", 1);
    tzset();
}

/**
 * @brief 初始化SNTP配置
 *
 * 设置SNTP操作模式为轮询模式，配置NTP服务器地址和同步回调函数
 */
static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}