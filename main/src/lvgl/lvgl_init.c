#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "epaper.h"
#include "ssd1681_waveshare_1in54_lut.h"

#include "lv_demos.h"
#include "lvgl_init.h"

#include "config_manager.h"
#include "touch.h"

#define TAG "lvgl_init"

// LVGL 定时器周期（毫秒）
#define LVGL_TICK_PERIOD_MS 33

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

// 触摸输入设备指针
static lv_indev_t *indev_touchpad = NULL;
// 显示缓冲区指针
static void *buf1 = NULL, *buf2 = NULL;
// LVGL 显示对象指针
lv_display_t *disp = NULL;
// LVGL线程互斥锁
SemaphoreHandle_t lvgl_mutex;

// 虚拟全屏帧缓冲（不含 LVGL 额外的调色板头部）
static uint8_t *virtual_fb = NULL; // 大小: MY_DISP_HOR_RES * MY_DISP_VER_RES / 8
// 当 LVGL 区域 flush 完成后，设置此标志，屏幕刷新线程在下一次周期将整个 virtual_fb 发送到面板
static volatile bool screen_needs_refresh = false;

// 快速刷新计数器，超过 max_partial_refresh_count 则触发全屏刷新
int fast_refresh_count = 0;

// Floyd-Steinberg 误差扩散抖动的误差缓冲区
// 需要两行误差缓冲：当前行和下一行
static int16_t *fs_error_cur = NULL;  // 当前行误差
static int16_t *fs_error_next = NULL; // 下一行误差

static inline uint8_t rgb332_to_luma_u8(uint8_t rgb332) {
    // RGB332: RRR GGG BB
    uint8_t r3 = (rgb332 >> 5) & 0x07;
    uint8_t g3 = (rgb332 >> 2) & 0x07;
    uint8_t b2 = (rgb332 >> 0) & 0x03;

    // Expand to 0..255
    uint8_t r = (uint8_t)((r3 * 255 + 3) / 7);
    uint8_t g = (uint8_t)((g3 * 255 + 3) / 7);
    uint8_t b = (uint8_t)((b2 * 255 + 1) / 3);

    // Integer luma (BT.601-ish): 0.299R + 0.587G + 0.114B
    return (uint8_t)((77u * r + 150u * g + 29u * b) >> 8);
}

static inline uint8_t lvgl_native_px_to_luma_u8(const uint8_t *px) {
#if LV_COLOR_DEPTH == 8
/* In this LVGL version, LV_COLOR_DEPTH==8 maps NATIVE to LV_COLOR_FORMAT_L8 by default.
 * If you modify LVGL so that NATIVE becomes RGB332 and you add LV_COLOR_FORMAT_RGB332,
 * this will automatically switch to RGB332->luma.
 */
#if defined(LV_COLOR_FORMAT_RGB332) && (LV_COLOR_FORMAT_NATIVE == LV_COLOR_FORMAT_RGB332)
    return rgb332_to_luma_u8(*px);
#else
    return *px; // L8: already a luminance
#endif
#elif LV_COLOR_DEPTH == 16
    // Assume RGB565 NATIVE (common). Convert to 8-bit luma.
    uint16_t c = ((uint16_t)px[0]) | ((uint16_t)px[1] << 8);
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5) & 0x3F;
    uint8_t b5 = (c >> 0) & 0x1F;
    uint8_t r = (uint8_t)((r5 * 255 + 15) / 31);
    uint8_t g = (uint8_t)((g6 * 255 + 31) / 63);
    uint8_t b = (uint8_t)((b5 * 255 + 15) / 31);
    return (uint8_t)((77u * r + 150u * g + 29u * b) >> 8);
#else
    // Fallback: treat first byte as luma.
    return *px;
#endif
}

/**
 * @brief 显示刷新回调函数
 *
 * 此函数由 LVGL 调用，用于将渲染的图像数据刷新到电子墨水屏
 *
 * @param disp_drv 显示驱动对象
 * @param area 需要刷新的区域
 * @param px_map 像素数据映射
 */
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map) {
    /*
     * 将 LVGL 提供的区域像素数据(像素数据在 px_map +palette_size) 按位拷贝到全局的虚拟帧缓冲
     * virtual_fb 上，并设置 screen_needs_refresh = true。 最后调用 lv_display_flush_ready 通知 LVGL
     * 本次 flush 已完成。
     */
    int height = area->y2 - area->y1 + 1;
    int width = area->x2 - area->x1 + 1;

    // LVGL 在 RGB332 模式下，px_map 即为区域像素数据首地址（无调色板头部）
    const uint8_t *src = px_map;

    if (virtual_fb == NULL) {
        ESP_LOGW(TAG, "virtual_fb is NULL, cannot copy area");
        lv_display_flush_ready(disp_drv);
        return;
    }

    const int screen_bytes_per_row = MY_DISP_HOR_RES / 8;
    const int area_pixels_per_row = width;

    // NATIVE(L8 / RGB332 / RGB565...) -> 1bpp (SSD1681): 1 means BLACK/RED, 0 means WHITE
    // Use Floyd-Steinberg error diffusion dithering for better quality grayscale rendering.
    // Floyd-Steinberg distributes quantization error to neighboring pixels:
    //        X   7/16
    //  3/16 5/16 1/16

    // 确保误差缓冲区已分配（宽度需要额外2个像素用于边界处理）
    const int err_buf_width = width + 2;
    if (fs_error_cur == NULL) {
        fs_error_cur = heap_caps_malloc(err_buf_width * sizeof(int16_t), MALLOC_CAP_DEFAULT);
        fs_error_next = heap_caps_malloc(err_buf_width * sizeof(int16_t), MALLOC_CAP_DEFAULT);
    }

    if (fs_error_cur == NULL || fs_error_next == NULL) {
        ESP_LOGW(TAG, "Floyd-Steinberg error buffer allocation failed, using simple threshold");
        // 回退到简单阈值
        for (int row = 0; row < height; ++row) {
            const uint8_t *src_row = src + row * area_pixels_per_row * BYTE_PER_PIXEL;
            uint8_t *dst_row = virtual_fb + (area->y1 + row) * screen_bytes_per_row;
            for (int x = 0; x < width; ++x) {
                int dst_x = area->x1 + x;
                const uint8_t *px = &src_row[x * BYTE_PER_PIXEL];
                uint8_t luma = lvgl_native_px_to_luma_u8(px);
                bool black = (luma < 128);
                int dst_byte = dst_x / 8;
                int dst_bit = 7 - (dst_x % 8);
                if (black)
                    dst_row[dst_byte] |= (1 << dst_bit);
                else
                    dst_row[dst_byte] &= ~(1 << dst_bit);
            }
        }
    } else {
        // 清空误差缓冲区
        memset(fs_error_cur, 0, err_buf_width * sizeof(int16_t));
        memset(fs_error_next, 0, err_buf_width * sizeof(int16_t));

        for (int row = 0; row < height; ++row) {
            const uint8_t *src_row = src + row * area_pixels_per_row * BYTE_PER_PIXEL;
            uint8_t *dst_row = virtual_fb + (area->y1 + row) * screen_bytes_per_row;

            // 清空下一行误差缓冲
            memset(fs_error_next, 0, err_buf_width * sizeof(int16_t));

            for (int x = 0; x < width; ++x) {
                int dst_x = area->x1 + x;
                const uint8_t *px = &src_row[x * BYTE_PER_PIXEL];
                uint8_t luma = lvgl_native_px_to_luma_u8(px);

                // 将误差加到当前像素（索引偏移1以处理左边界）
                int16_t old_pixel = (int16_t)luma + fs_error_cur[x + 1];

                // 限制到有效范围
                if (old_pixel < 0)
                    old_pixel = 0;
                if (old_pixel > 255)
                    old_pixel = 255;

                // 量化：决定是黑还是白
                uint8_t new_pixel = (old_pixel < 128) ? 0 : 255;
                bool black = (new_pixel == 0);

                // 计算量化误差
                int16_t quant_error = old_pixel - (int16_t)new_pixel;

                // Floyd-Steinberg 误差扩散
                // 向右: 7/16
                fs_error_cur[x + 2] += (quant_error * 7) >> 4;
                // 左下: 3/16
                fs_error_next[x] += (quant_error * 3) >> 4;
                // 正下: 5/16
                fs_error_next[x + 1] += (quant_error * 5) >> 4;
                // 右下: 1/16
                fs_error_next[x + 2] += (quant_error * 1) >> 4;

                // 写入目标帧缓冲
                int dst_byte = dst_x / 8;
                int dst_bit = 7 - (dst_x % 8);

                if (black)
                    dst_row[dst_byte] |= (1 << dst_bit);
                else
                    dst_row[dst_byte] &= ~(1 << dst_bit);
            }

            // 交换误差缓冲区指针
            int16_t *tmp = fs_error_cur;
            fs_error_cur = fs_error_next;
            fs_error_next = tmp;
        }
    }

    // 标记屏幕需要刷新，实际发送由屏幕刷新线程负责（整个虚拟缓冲一次性发送）
    screen_needs_refresh = true;

    /* 通知 LVGL 此次区域 flush 已处理完成 */
    lv_display_flush_ready(disp_drv);
}

/**
 * @brief 初始化 LVGL 显示驱动
 *
 * 创建显示对象并设置缓冲区和刷新回调函数
 */
void lv_port_disp_init(void) {
    ESP_LOGI(TAG, "Initializing LVGL display");

    // 创建显示对象
    disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display creation failed");
        return;
    }

    // 跟随 LVGL 的 NATIVE color format（你会在 menuconfig 中设置 LV_COLOR_DEPTH=8 等）
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);

    // 设置显示刷新回调函数
    lv_display_set_flush_cb(disp, disp_flush);

    // 计算虚拟全屏帧缓冲大小（SSD1681：1bpp）
    size_t virt_size = MY_DISP_HOR_RES * MY_DISP_VER_RES / 8;
    // 给 LVGL 分配渲染缓冲（跟随 BYTE_PER_PIXEL）
    size_t lv_buf_size = (size_t)MY_DISP_HOR_RES * (size_t)MY_DISP_VER_RES * (size_t)BYTE_PER_PIXEL;

    // 分配 LVGL 渲染缓冲（用于 LVGL 的渲染），以及持久化的虚拟帧缓冲用于积累整个屏幕像素
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
}

/**
 * @brief 屏幕刷新线程
 *
 * 每 0.5 秒检查 screen_needs_refresh 标志，如果为 true，将整个 virtual_fb 一次性发送到屏幕。
 */
static void lvgl_screen_refresh_task(void *param) {
    const TickType_t delay_ticks = pdMS_TO_TICKS(500);
    const size_t virt_size = MY_DISP_HOR_RES * MY_DISP_VER_RES / 8;

    while (1) {
        vTaskDelay(delay_ticks);

        if (!screen_needs_refresh)
            continue;

        ESP_LOGI(TAG, "Screen refresh task: sending full virtual framebuffer");

        // 打开屏幕
        esp_lcd_panel_disp_on_off(s_panel_handle, true);

        static uint8_t fast_refresh_lut[] = SSD1681_WAVESHARE_1IN54_V2_LUT_FAST_REFRESH_KEEP;
        ESP_ERROR_CHECK(epaper_panel_set_custom_lut(s_panel_handle, fast_refresh_lut, 159));

        // 发送黑色位图（直接发送 virtual_fb）
        epaper_panel_set_bitmap_color(s_panel_handle, SSD1681_EPAPER_BITMAP_BLACK);
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, MY_DISP_HOR_RES, MY_DISP_VER_RES,
                                  virtual_fb);

        // 刷新并关闭屏幕
        epaper_panel_refresh_screen(s_panel_handle);
        esp_lcd_panel_disp_on_off(s_panel_handle, false);

        screen_needs_refresh = false;
    }
}

/**
 * @brief 增加 LVGL 系统时钟滴答
 *
 * 由定时器回调函数定期调用，更新 LVGL 内部时钟
 *
 * @param arg 定时器参数（未使用）
 */
static void increase_lvgl_tick(void *arg) {
    // 获取 LVGL 互斥锁
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    lv_tick_inc(LVGL_TICK_PERIOD_MS);

    // 释放 LVGL 互斥锁
    xSemaphoreGive(lvgl_mutex);
}

/**
 * @brief LVGL 定时器任务
 *
 * 处理 LVGL 定时器事件并更新 UI
 *
 * @param param 任务参数（未使用）
 */
void lvgl_timer_task(void *param) {
    while (1) {
        // 获取 LVGL 互斥锁
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

        // 处理 LVGL 定时器事件
        lv_timer_handler();

        // 释放 LVGL 互斥锁
        xSemaphoreGive(lvgl_mutex);

        // 延迟一个周期
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

// 触摸数据结构体
typedef struct {
    int32_t x;               // X 坐标
    int32_t y;               // Y 坐标
    bool is_pressed;         // 是否按下
    SemaphoreHandle_t mutex; // 互斥锁，用于保护数据访问
} touch_data_t;

// 全局触摸数据对象
static touch_data_t g_touch_data = {
    .x = 0,
    .y = 0,
    .is_pressed = false,
};

/**
 * @brief 触摸板读取回调函数
 *
 * 由 LVGL 调用，用于读取当前触摸状态
 *
 * @param indev_drv 输入设备驱动对象
 * @param data 输入设备数据结构体
 */
static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data) {
    ctp_tp_t ctp;
    i2c_ctp_FTxxxx_read_all(I2C_NUM_0, &ctp);

    // 如果检测到触摸点
    if (ctp.tp_num > 0) {
        // 转换触摸屏 X 轴坐标
        int32_t x = ((int16_t)(ctp.tp[0].x - 160) - 319) * -1;
        int32_t y = ctp.tp[0].y; // Y 轴坐标

        // 检查坐标是否在屏幕范围内
        if (x < 200 && y < 200) {
            g_touch_data.x = x;
            g_touch_data.y = y;
            g_touch_data.is_pressed = true;
        } else {
            g_touch_data.is_pressed = false;
        }
    } else {
        g_touch_data.is_pressed = false;
    }

    // 设置触摸状态（按下或释放）
    data->state = g_touch_data.is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    // 设置触摸点坐标
    data->point.x = g_touch_data.x;
    data->point.y = g_touch_data.y;
}

/**
 * @brief 初始化 LVGL 输入设备（触摸屏）
 *
 * 创建触摸输入设备并设置读取回调函数
 */
void lv_port_indev_init(void) {
    // 创建输入设备对象
    indev_touchpad = lv_indev_create();
    // 设置输入设备类型为指针设备
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    // 设置输入设备读取回调函数
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}

/**
 * @brief 初始化电子墨水屏显示
 *
 * 初始化电子墨水屏驱动、LVGL 显示和输入设备
 */
void lvgl_init_epaper_display(void) {
    // 创建 LVGL 互斥锁
    lvgl_mutex = xSemaphoreCreateMutex();

    // 初始化电子墨水屏硬件
    esp_err_t ret = epaper_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "epaper_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化触摸屏
    touch_init();

    // 初始化 LVGL 库
    lv_init();
    // 初始化显示驱动
    lv_port_disp_init();
    // 初始化输入设备
    lv_port_indev_init();

    ESP_LOGI(TAG, "Setting up LVGL tick timer");
    // 配置 LVGL 系统时钟定时器
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick, // 定时器回调函数
        .name = "lvgl_tick"              // 定时器名称
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // 初始化 UI
    lv_demo_widgets();

    // 创建 LVGL 定时器任务
    xTaskCreate(lvgl_timer_task, "lvgl_task", 8192, NULL, 10, NULL);

    // 创建屏幕刷新任务（每 0.5s 检查一次 screen_needs_refresh）
    xTaskCreate(lvgl_screen_refresh_task, "lvgl_refresh", 4096, NULL, 9, NULL);
}
