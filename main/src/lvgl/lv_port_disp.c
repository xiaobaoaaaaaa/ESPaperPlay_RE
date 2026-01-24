/**
 * @file lv_port_disp.c
 * @brief LVGL 显示驱动实现
 */

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "dither.h"
#include "lv_port_disp.h"
#include "lvgl.h"

#define TAG "lv_port_disp"

// 显示屏水平分辨率默认值
#ifndef MY_DISP_HOR_RES
#define MY_DISP_HOR_RES 200
#endif

// 显示屏垂直分辨率默认值
#ifndef MY_DISP_VER_RES
#define MY_DISP_VER_RES 200
#endif

// 每个像素的字节数（跟随 LVGL 当前 NATIVE color format）
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_NATIVE))

// ============================================================================
// 私有变量
// ============================================================================

// 显示缓冲区指针
static void *buf1 = NULL, *buf2 = NULL;

// LVGL 显示对象指针
static lv_display_t *disp = NULL;

// 虚拟全屏帧缓冲（1bpp）
static uint8_t *virtual_fb = NULL;

// 屏幕刷新标志
static volatile bool screen_needs_refresh = false;

// ============================================================================
// 私有函数
// ============================================================================

/**
 * @brief 显示刷新回调函数
 *
 * 此函数由 LVGL 调用，用于将渲染的图像数据刷新到电子墨水屏
 */
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map) {
    int height = area->y2 - area->y1 + 1;
    int width = area->x2 - area->x1 + 1;

    if (virtual_fb == NULL) {
        ESP_LOGW(TAG, "virtual_fb is NULL, cannot copy area");
        lv_display_flush_ready(disp_drv);
        return;
    }

    // 使用抖动模块转换像素数据
    dither_convert_area(px_map, virtual_fb, area->x1, area->y1, width, height, MY_DISP_HOR_RES,
                        BYTE_PER_PIXEL);

    // 标记屏幕需要刷新
    screen_needs_refresh = true;

    // 通知 LVGL 此次区域 flush 已处理完成
    lv_display_flush_ready(disp_drv);
}

// ============================================================================
// 公共 API
// ============================================================================

void lv_port_disp_init(void) {
    ESP_LOGI(TAG, "Initializing LVGL display");

    // 创建显示对象
    disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display creation failed");
        return;
    }

    // 跟随 LVGL 的 NATIVE color format
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);

    // 设置显示刷新回调函数
    lv_display_set_flush_cb(disp, disp_flush);

    // 计算虚拟全屏帧缓冲大小（SSD1681：1bpp）
    size_t virt_size = MY_DISP_HOR_RES * MY_DISP_VER_RES / 8;
    // 给 LVGL 分配渲染缓冲
    size_t lv_buf_size = (size_t)MY_DISP_HOR_RES * (size_t)MY_DISP_VER_RES * (size_t)BYTE_PER_PIXEL;

    // 分配缓冲区
    buf1 = heap_caps_malloc(lv_buf_size, MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(lv_buf_size, MALLOC_CAP_DMA);
    virtual_fb = heap_caps_malloc(virt_size, MALLOC_CAP_DMA);

    if (!buf1 || !buf2 || !virtual_fb) {
        ESP_LOGE(TAG, "Display buffer allocation failed (buf1=%p virtual_fb=%p)", buf1, virtual_fb);
        if (buf1)
            heap_caps_free(buf1);
        if (buf2)
            heap_caps_free(buf2);
        if (virtual_fb)
            heap_caps_free(virtual_fb);
        buf1 = NULL;
        buf2 = NULL;
        virtual_fb = NULL;
        return;
    }

    // 清空虚拟缓冲：SSD1681 默认 invert=false 时，0 表示 WHITE
    memset(virtual_fb, 0x00, virt_size);

    // 将 LVGL 使用的缓冲注册进 LVGL
    lv_display_set_buffers(disp, buf1, buf2, lv_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "LVGL display initialized successfully");
}

lv_display_t *lv_port_disp_get(void) { return disp; }

uint8_t *lv_port_disp_get_fb(void) { return virtual_fb; }

bool lv_port_disp_needs_refresh(void) { return screen_needs_refresh; }

void lv_port_disp_clear_refresh_flag(void) { screen_needs_refresh = false; }
