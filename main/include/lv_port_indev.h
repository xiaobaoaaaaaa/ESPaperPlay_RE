/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备驱动接口
 */
#pragma once

#include "lvgl.h"

/**
 * @brief 初始化 LVGL 输入设备（触摸屏）
 *
 * 创建触摸输入设备并设置读取回调函数
 */
void lv_port_indev_init(void);

/**
 * @brief 获取触摸输入设备指针
 * @return 触摸输入设备指针
 */
lv_indev_t *lv_port_indev_get_touchpad(void);
