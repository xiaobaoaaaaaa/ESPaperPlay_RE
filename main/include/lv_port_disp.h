/**
 * @file lv_port_disp.h
 * @brief LVGL 显示驱动接口
 */
#pragma once

#include "lvgl.h"

/**
 * @brief 初始化 LVGL 显示驱动
 *
 * 创建显示对象并设置缓冲区和刷新回调函数
 */
void lv_port_disp_init(void);

/**
 * @brief 获取显示对象指针
 * @return 显示对象指针
 */
lv_display_t *lv_port_disp_get(void);

/**
 * @brief 获取虚拟帧缓冲指针
 * @return 虚拟帧缓冲指针（1bpp）
 */
uint8_t *lv_port_disp_get_fb(void);

/**
 * @brief 获取帧缓冲大小
 * @return 帧缓冲大小（字节）
 */
size_t lv_port_disp_get_fb_size(void);

/**
 * @brief 检查屏幕是否需要刷新
 * @return true 需要刷新，false 不需要
 */
bool lv_port_disp_needs_refresh(void);

/**
 * @brief 清除屏幕刷新标志
 */
void lv_port_disp_clear_refresh_flag(void);
