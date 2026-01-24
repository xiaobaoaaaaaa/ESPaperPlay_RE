/**
 * @file dither.h
 * @brief 抖动算法模块 - 用于将灰度图像转换为 1bpp 黑白图像
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 抖动算法模式枚举
 */
typedef enum {
    DITHER_MODE_BAYER,           ///< Bayer 8x8 有序抖动 - 最快，无额外内存
    DITHER_MODE_FLOYD_STEINBERG, ///< Floyd-Steinberg 误差扩散 - 中等性能，效果好
    DITHER_MODE_STUCKI,          ///< Stucki 误差扩散 - 最慢，效果最好
} dither_mode_t;

/**
 * @brief 设置抖动模式
 * @param mode 抖动模式
 */
void dither_set_mode(dither_mode_t mode);

/**
 * @brief 获取当前抖动模式
 * @return 当前抖动模式
 */
dither_mode_t dither_get_mode(void);

/**
 * @brief 将灰度像素数据转换为 1bpp 位图
 *
 * @param src 源像素数据（LVGL NATIVE 格式）
 * @param dst 目标 1bpp 帧缓冲
 * @param area_x1 区域左上角 X 坐标
 * @param area_y1 区域左上角 Y 坐标
 * @param width 区域宽度
 * @param height 区域高度
 * @param screen_width 屏幕总宽度（用于计算目标偏移）
 * @param bytes_per_pixel 每像素字节数
 */
void dither_convert_area(const uint8_t *src, uint8_t *dst, int area_x1, int area_y1, int width,
                         int height, int screen_width, int bytes_per_pixel);

/**
 * @brief 释放抖动算法使用的内存
 */
void dither_free_buffers(void);
