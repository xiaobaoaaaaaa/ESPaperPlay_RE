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
#include "ssd1681_waveshare_1in54_lut.h"

#include "lv_demos.h"
#include "lvgl.h"

#include "dither.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl_init.h"
#include "touch.h"

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

static int fast_refresh_count = 0;

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

        if (fast_refresh_count < 5) {
            fast_refresh_count++;
            // 设置快速刷新 LUT
            static uint8_t fast_refresh_lut[] = SSD1681_WAVESHARE_1IN54_V2_LUT_FAST_REFRESH_KEEP;
            ESP_ERROR_CHECK(epaper_panel_set_custom_lut(s_panel_handle, fast_refresh_lut, 159));
        } else {
            fast_refresh_count = 0;
        }

        // 发送黑色位图
        epaper_panel_set_bitmap_color(s_panel_handle, SSD1681_EPAPER_BITMAP_BLACK);
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, MY_DISP_HOR_RES, MY_DISP_VER_RES,
                                  lv_port_disp_get_fb());

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

    // 初始化 UI Demo
    lv_demo_widgets();

    // 创建 LVGL 定时器任务
    xTaskCreate(lvgl_timer_task, "lvgl_task", 8192, NULL, 10, NULL);

    // 创建屏幕刷新任务
    xTaskCreate(lvgl_screen_refresh_task, "lvgl_refresh", 4096, NULL, 9, NULL);

    ESP_LOGI(TAG, "LVGL initialization complete");
}
