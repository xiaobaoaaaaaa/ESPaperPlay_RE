/**
 * @file dither.c
 * @brief 抖动算法实现 - 支持 Bayer、Floyd-Steinberg、Stucki 三种算法
 */

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "dither.h"
#include "lvgl.h"

#define TAG "dither"

// ============================================================================
// 私有变量
// ============================================================================

// 当前抖动模式（硬编码配置，后续可改为从 NVS 读取）
static dither_mode_t g_dither_mode = DITHER_MODE_STUCKI;

// 记录禁用抖动前的模式，用于恢复
static dither_mode_t g_last_dither_mode = DITHER_MODE_STUCKI;

// 误差扩散抖动的误差缓冲区（按需分配）
// Floyd-Steinberg 需要 2 行，Stucki 需要 3 行
static int16_t *dither_error_lines[3] = {NULL, NULL, NULL};
static int dither_error_line_count = 0;
static int dither_error_line_width = 0;

// Bayer 8x8 ordered dithering matrix, values in [0, 63]
static const uint8_t s_bayer8x8[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21},
};

// ============================================================================
// 颜色转换辅助函数
// ============================================================================

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

// ============================================================================
// 抖动算法实现
// ============================================================================

/**
 * @brief 简单阈值（无抖动）
 */
static void dither_threshold(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1, int width,
                             int height, int screen_width, int bytes_per_pixel) {
    const int screen_bytes_per_row = screen_width / 8;

    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
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
}

/**
 * @brief Bayer 8x8 有序抖动
 */
static void dither_bayer(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1, int width,
                         int height, int screen_width, int bytes_per_pixel) {
    const int screen_bytes_per_row = screen_width / 8;

    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;
        int y = area_y1 + row;

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
            uint8_t luma = lvgl_native_px_to_luma_u8(px);

            // 阈值范围 0..255，加小偏移减少固定图案
            uint8_t thr = (uint8_t)(s_bayer8x8[y & 7][dst_x & 7] * 4 + 2);
            bool black = (luma < thr);

            int dst_byte = dst_x / 8;
            int dst_bit = 7 - (dst_x % 8);

            if (black)
                dst_row[dst_byte] |= (1 << dst_bit);
            else
                dst_row[dst_byte] &= ~(1 << dst_bit);
        }
    }
}

/**
 * @brief Floyd-Steinberg 误差扩散抖动
 */
static void dither_floyd_steinberg(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1,
                                   int width, int height, int screen_width, int bytes_per_pixel) {
    const int screen_bytes_per_row = screen_width / 8;
    const int err_buf_width = width + 2;
    const int required_lines = 2;

    // 按需分配或重新分配误差缓冲区
    if (dither_error_line_count < required_lines || dither_error_line_width < err_buf_width) {
        dither_free_buffers();
        for (int i = 0; i < required_lines; i++) {
            dither_error_lines[i] =
                heap_caps_malloc(err_buf_width * sizeof(int16_t), MALLOC_CAP_DEFAULT);
        }
        dither_error_line_count = required_lines;
        dither_error_line_width = err_buf_width;
    }

    if (dither_error_lines[0] == NULL || dither_error_lines[1] == NULL) {
        ESP_LOGW(TAG, "Floyd-Steinberg buffer alloc failed, fallback to threshold");
        goto fallback;
    }

    int16_t *err_cur = dither_error_lines[0];
    int16_t *err_next = dither_error_lines[1];

    memset(err_cur, 0, err_buf_width * sizeof(int16_t));
    memset(err_next, 0, err_buf_width * sizeof(int16_t));

    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;

        memset(err_next, 0, err_buf_width * sizeof(int16_t));

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
            uint8_t luma = lvgl_native_px_to_luma_u8(px);

            int16_t old_pixel = (int16_t)luma + err_cur[x + 1];
            old_pixel = (old_pixel < 0) ? 0 : (old_pixel > 255) ? 255 : old_pixel;

            uint8_t new_pixel = (old_pixel < 128) ? 0 : 255;
            bool black = (new_pixel == 0);
            int16_t quant_error = old_pixel - (int16_t)new_pixel;

            // 误差扩散: 7/16, 3/16, 5/16, 1/16
            err_cur[x + 2] += (quant_error * 7) >> 4;
            err_next[x] += (quant_error * 3) >> 4;
            err_next[x + 1] += (quant_error * 5) >> 4;
            err_next[x + 2] += (quant_error * 1) >> 4;

            int dst_byte = dst_x / 8;
            int dst_bit = 7 - (dst_x % 8);

            if (black)
                dst_row[dst_byte] |= (1 << dst_bit);
            else
                dst_row[dst_byte] &= ~(1 << dst_bit);
        }

        // 交换缓冲区
        int16_t *tmp = err_cur;
        err_cur = err_next;
        err_next = tmp;
    }
    return;

fallback:
    // 简单阈值回退
    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
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
}

/**
 * @brief Stucki 误差扩散抖动
 */
static void dither_stucki(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1, int width,
                          int height, int screen_width, int bytes_per_pixel) {
    const int screen_bytes_per_row = screen_width / 8;
    const int err_buf_width = width + 4; // 需要更宽的边界
    const int required_lines = 3;

    // 按需分配或重新分配误差缓冲区
    if (dither_error_line_count < required_lines || dither_error_line_width < err_buf_width) {
        dither_free_buffers();
        for (int i = 0; i < required_lines; i++) {
            dither_error_lines[i] =
                heap_caps_malloc(err_buf_width * sizeof(int16_t), MALLOC_CAP_DEFAULT);
        }
        dither_error_line_count = required_lines;
        dither_error_line_width = err_buf_width;
    }

    if (dither_error_lines[0] == NULL || dither_error_lines[1] == NULL ||
        dither_error_lines[2] == NULL) {
        ESP_LOGW(TAG, "Stucki buffer alloc failed, fallback to threshold");
        goto fallback;
    }

    int16_t *err_row0 = dither_error_lines[0]; // 当前行
    int16_t *err_row1 = dither_error_lines[1]; // 下一行
    int16_t *err_row2 = dither_error_lines[2]; // 下两行

    memset(err_row0, 0, err_buf_width * sizeof(int16_t));
    memset(err_row1, 0, err_buf_width * sizeof(int16_t));
    memset(err_row2, 0, err_buf_width * sizeof(int16_t));

    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;

        memset(err_row2, 0, err_buf_width * sizeof(int16_t));

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
            uint8_t luma = lvgl_native_px_to_luma_u8(px);

            // 索引偏移 2 以处理左边界
            int idx = x + 2;
            int16_t old_pixel = (int16_t)luma + err_row0[idx];
            old_pixel = (old_pixel < 0) ? 0 : (old_pixel > 255) ? 255 : old_pixel;

            uint8_t new_pixel = (old_pixel < 128) ? 0 : 255;
            bool black = (new_pixel == 0);
            int16_t err = old_pixel - (int16_t)new_pixel;

            // Stucki 误差扩散 (使用定点数近似)
            // 8/42*64≈12, 4/42*64≈6, 2/42*64≈3, 1/42*64≈2

            // 当前行: X, +1, +2
            err_row0[idx + 1] += (err * 12) >> 6; // 8/42
            err_row0[idx + 2] += (err * 6) >> 6;  // 4/42

            // 下一行: -2, -1, 0, +1, +2
            err_row1[idx - 2] += (err * 3) >> 6; // 2/42
            err_row1[idx - 1] += (err * 6) >> 6; // 4/42
            err_row1[idx] += (err * 12) >> 6;    // 8/42
            err_row1[idx + 1] += (err * 6) >> 6; // 4/42
            err_row1[idx + 2] += (err * 3) >> 6; // 2/42

            // 下两行: -2, -1, 0, +1, +2
            err_row2[idx - 2] += (err * 2) >> 6; // 1/42
            err_row2[idx - 1] += (err * 3) >> 6; // 2/42
            err_row2[idx] += (err * 6) >> 6;     // 4/42
            err_row2[idx + 1] += (err * 3) >> 6; // 2/42
            err_row2[idx + 2] += (err * 2) >> 6; // 1/42

            int dst_byte = dst_x / 8;
            int dst_bit = 7 - (dst_x % 8);

            if (black)
                dst_row[dst_byte] |= (1 << dst_bit);
            else
                dst_row[dst_byte] &= ~(1 << dst_bit);
        }

        // 轮转缓冲区
        int16_t *tmp = err_row0;
        err_row0 = err_row1;
        err_row1 = err_row2;
        err_row2 = tmp;
    }
    return;

fallback:
    // 简单阈值回退
    for (int row = 0; row < height; ++row) {
        const uint8_t *src_row = src + row * width * bytes_per_pixel;
        uint8_t *dst_row = dst + (area_y1 + row) * screen_bytes_per_row;

        for (int x = 0; x < width; ++x) {
            int dst_x = area_x1 + x;
            const uint8_t *px = &src_row[x * bytes_per_pixel];
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
}

// ============================================================================
// 公共 API
// ============================================================================

void dither_set_mode(dither_mode_t mode) {
    if (mode != g_dither_mode) {
        dither_free_buffers();
        g_dither_mode = mode;
        // 如果设置的是有效抖动模式，记录下来
        if (mode != DITHER_MODE_NONE) {
            g_last_dither_mode = mode;
        }
        ESP_LOGI(TAG, "Dither mode changed to %d", mode);
    }
}

dither_mode_t dither_get_mode(void) { return g_dither_mode; }

bool dither_is_enabled(void) { return g_dither_mode != DITHER_MODE_NONE; }

void dither_set_enabled(bool enable) {
    if (enable) {
        // 恢复上次使用的抖动模式
        if (g_dither_mode == DITHER_MODE_NONE) {
            dither_set_mode(g_last_dither_mode);
        }
    } else {
        // 禁用抖动
        dither_set_mode(DITHER_MODE_NONE);
    }
}

void dither_convert_area(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1, int width,
                         int height, int screen_width, int bytes_per_pixel) {
    switch (g_dither_mode) {
    case DITHER_MODE_NONE:
        // 简单阈值，无抖动
        dither_threshold(src, dst, area_x1, area_y1, width, height, screen_width, bytes_per_pixel);
        break;
    case DITHER_MODE_BAYER:
        dither_bayer(src, dst, area_x1, area_y1, width, height, screen_width, bytes_per_pixel);
        break;
    case DITHER_MODE_FLOYD_STEINBERG:
        dither_floyd_steinberg(src, dst, area_x1, area_y1, width, height, screen_width,
                               bytes_per_pixel);
        break;
    case DITHER_MODE_STUCKI:
        dither_stucki(src, dst, area_x1, area_y1, width, height, screen_width, bytes_per_pixel);
        break;
    default:
        dither_threshold(src, dst, area_x1, area_y1, width, height, screen_width, bytes_per_pixel);
        break;
    }
}

void dither_free_buffers(void) {
    for (int i = 0; i < 3; i++) {
        if (dither_error_lines[i]) {
            heap_caps_free(dither_error_lines[i]);
            dither_error_lines[i] = NULL;
        }
    }
    dither_error_line_count = 0;
    dither_error_line_width = 0;
}
