/**
 * @file lvgl_init.c
 * @brief LVGL 初始化入口 - 整合显示、输入、定时器
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "epaper.h"

#include "lv_demos.h"
#include "lvgl.h"

#include "config_manager.h"
#include "dither.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl_init.h"
#include "touch.h"
#include "ui.h"

#define TAG "lvgl_init"

// LVGL 定时器周期（毫秒）
#define LVGL_TICK_PERIOD_MS 33

// 显示屏分辨率
#ifndef MY_DISP_HOR_RES
#define MY_DISP_HOR_RES 200
#endif

#ifndef MY_DISP_VER_RES
#define MY_DISP_VER_RES 200
#endif

// 局刷计数器和阈值
static int fast_refresh_count = 0;
static int max_fast_refresh_count = 30;

// ============================================================================
// 私有变量
// ============================================================================

// LVGL 线程互斥锁
static SemaphoreHandle_t lvgl_mutex = NULL;

// ============================================================================
// 私有函数
// ============================================================================

/**
 * @brief 屏幕刷新线程
 *
 * 每 0.5 秒检查屏幕刷新标志，如果需要刷新则发送帧缓冲到屏幕
 */
static void lvgl_screen_refresh_task(void *param) {
    const TickType_t delay_ticks = pdMS_TO_TICKS(500);

    while (1) {
        vTaskDelay(delay_ticks);

        if (!lv_port_disp_needs_refresh())
            continue;

        ESP_LOGI(TAG, "Screen refresh task: sending full virtual framebuffer");

        // 打开屏幕
        esp_lcd_panel_disp_on_off(s_panel_handle, true);

        // 根据局刷计数决定刷新模式
        if (fast_refresh_count < max_fast_refresh_count) {
            fast_refresh_count++;
            // 使用内置 LUT 局刷模式
            ESP_LOGI(TAG, "Partial refresh (%d/%d)", fast_refresh_count, max_fast_refresh_count);
            epaper_panel_set_refresh_mode(s_panel_handle, true); // 局刷
        } else {
            fast_refresh_count = 0;
            // 使用全刷模式重置屏幕
            ESP_LOGI(TAG, "Full refresh (reset screen)");
            epaper_panel_set_refresh_mode(s_panel_handle, false); // 全刷
        }

        uint8_t *virtual_fb = lv_port_disp_get_fb();

        // 发送黑色位图
        epaper_panel_set_bitmap_color(s_panel_handle, SSD1681_EPAPER_BITMAP_BLACK);
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, MY_DISP_HOR_RES, MY_DISP_VER_RES,
                                  virtual_fb);

        // 发送红色位图（与黑色位图相同）
        // SSD1681 使用内置 LUT 局刷时，需要同时写入两个 VRAM
        epaper_panel_set_bitmap_color(s_panel_handle, SSD1681_EPAPER_BITMAP_RED);
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, MY_DISP_HOR_RES, MY_DISP_VER_RES,
                                  virtual_fb);

        // 刷新并关闭屏幕
        epaper_panel_refresh_screen(s_panel_handle);
        esp_lcd_panel_disp_on_off(s_panel_handle, false);

        lv_port_disp_clear_refresh_flag();
    }
}

/**
 * @brief 增加 LVGL 系统时钟滴答
 *
 * 由定时器回调函数定期调用，更新 LVGL 内部时钟
 */
static void increase_lvgl_tick(void *arg) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
    ui_tick();
    xSemaphoreGive(lvgl_mutex);
}

/**
 * @brief LVGL 定时器任务
 *
 * 处理 LVGL 定时器事件并更新 UI
 */
static void lvgl_timer_task(void *param) {
    while (1) {
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        lv_timer_handler();
        xSemaphoreGive(lvgl_mutex);

        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

// ============================================================================
// 公共 API
// ============================================================================

SemaphoreHandle_t lvgl_get_mutex(void) { return lvgl_mutex; }

void lvgl_init_epaper_display(void) {
    ESP_LOGI(TAG, "Initializing LVGL for e-paper display");

    // 创建 LVGL 互斥锁
    lvgl_mutex = xSemaphoreCreateMutex();

    // 获取系统配置
    sys_config_t sys_config;
    config_manager_get_config(&sys_config);
    max_fast_refresh_count = sys_config.display.fast_refresh_count;
    dither_set_mode(sys_config.display.dither_mode);

    // 初始化电子墨水屏硬件
    esp_err_t ret = epaper_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "epaper_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化触摸屏硬件
    touch_init();

    // 初始化 LVGL 库
    lv_init();

    // 初始化显示驱动
    lv_port_disp_init();

    // 初始化输入设备
    lv_port_indev_init();

    // 配置 LVGL 系统时钟定时器
    ESP_LOGI(TAG, "Setting up LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {.callback = &increase_lvgl_tick,
                                                          .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // 初始化 UI
    ui_init();

    // 创建 LVGL 定时器任务
    xTaskCreate(lvgl_timer_task, "lvgl_task", 8192, NULL, 10, NULL);

    // 创建屏幕刷新任务
    xTaskCreate(lvgl_screen_refresh_task, "lvgl_refresh", 4096, NULL, 9, NULL);

    ESP_LOGI(TAG, "LVGL initialization complete");
}
