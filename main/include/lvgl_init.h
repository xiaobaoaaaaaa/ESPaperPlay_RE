/**
 * @file lvgl_init.h
 * @brief LVGL 初始化入口接口
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

// 重新导出子模块接口，方便使用
#include "dither.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

/**
 * @brief 初始化电子墨水屏显示
 *
 * 初始化电子墨水屏驱动、LVGL 显示和输入设备
 */
void lvgl_init_epaper_display(void);

/**
 * @brief 获取 LVGL 互斥锁
 * @return LVGL 互斥锁句柄
 */
SemaphoreHandle_t lvgl_get_mutex(void);