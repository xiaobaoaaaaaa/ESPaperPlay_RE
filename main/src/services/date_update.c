/**
 * @file date_update.c
 * @brief 日期和时间更新服务模块
 *
 * 本模块定期检查系统时间，当日期、时间或星期发生变化时更新相应的系统变量。
 * 支持功能：
 * - 检测日期变化（年月日）
 * - 检测时间变化（时分）
 * - 检测星期变化
 * - 通过定时器每秒执行一次检测
 *
 * @author
 * @date YYYY-MM-DD
 */

#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vars.h"

#include "config_manager.h"
#include "date_update.h"

/** @brief 上次记录的年份 */
static int last_year = -1;
/** @brief 上次记录的月份 */
static int last_month = -1;
/** @brief 上次记录的日期 */
static int last_day = -1;
/** @brief 上次记录的小时 */
static int last_hour = -1;
/** @brief 上次记录的分钟 */
static int last_minute = -1;
/** @brief 上次记录的星期 */
static int last_weekday = -1;

/**
 * @brief 更新日期和时间
 *
 * 检查当前系统时间，如果与上次记录的值不同，则更新相应的系统变量。
 * 定期被定时器回调函数调用。
 */
void date_update() {
    time_t now = time(NULL);
    struct tm timeinfo;

    // 获取当前时间
    time(&now);
    localtime_r(&now, &timeinfo);

    char buffer[35];

    // 检测日期变更（年月日）
    if (timeinfo.tm_year != last_year || timeinfo.tm_mon != last_month ||
        timeinfo.tm_mday != last_day) {
        last_year = timeinfo.tm_year;
        last_month = timeinfo.tm_mon;
        last_day = timeinfo.tm_mday;

        // 更新日期变量为 "YYYY年MM月DD日" 格式
        snprintf(buffer, sizeof(buffer), "%04d年%02d月%02d日", timeinfo.tm_year + 1900,
                 timeinfo.tm_mon + 1, timeinfo.tm_mday);
        set_var_current_date(buffer);
    }

    // 检测时间变更（时分）
    if (timeinfo.tm_hour != last_hour || timeinfo.tm_min != last_minute) {
        last_hour = timeinfo.tm_hour;
        last_minute = timeinfo.tm_min;

        // 更新时间变量为 "HH:MM" 格式
        snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        set_var_current_time(buffer);
    }

    // 检测星期变更
    if (timeinfo.tm_wday != last_weekday) {
        last_weekday = timeinfo.tm_wday;

        // 中文星期名称数组
        static const char *weekdays[] = {"星期日", "星期一", "星期二", "星期三",
                                         "星期四", "星期五", "星期六"};
        set_var_current_weekday(weekdays[timeinfo.tm_wday]);
    }
}

/**
 * @brief 初始化日期更新服务
 *
 * 创建一个定时器，每秒调用一次 date_update() 函数来检测和更新日期时间信息。
 */
void date_update_init() {
    // 创建一个定时器，每秒钟调用一次 date_update 函数
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &date_update,   /**< 定时器回调函数 */
        .name = "date_update_timer" /**< 定时器名称 */
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    // 启动周期定时器（1000 * 1000 微秒 = 1 秒）
    esp_timer_start_periodic(periodic_timer, 1000 * 1000);
}