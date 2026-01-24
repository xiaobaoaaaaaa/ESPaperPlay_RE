/**
 * @file lv_port_indev.c
 * @brief LVGL 输入设备驱动实现
 */

#include "driver/i2c.h"
#include "esp_log.h"

#include "lv_port_indev.h"
#include "lvgl.h"
#include "touch.h"

#define TAG "lv_port_indev"

// 显示屏分辨率
#ifndef MY_DISP_HOR_RES
#define MY_DISP_HOR_RES 200
#endif

#ifndef MY_DISP_VER_RES
#define MY_DISP_VER_RES 200
#endif

// ============================================================================
// 私有变量
// ============================================================================

// 触摸输入设备指针
static lv_indev_t *indev_touchpad = NULL;

// 触摸数据结构体
typedef struct {
    int32_t x;       // X 坐标
    int32_t y;       // Y 坐标
    bool is_pressed; // 是否按下
} touch_data_t;

// 全局触摸数据对象
static touch_data_t g_touch_data = {
    .x = 0,
    .y = 0,
    .is_pressed = false,
};

// ============================================================================
// 私有函数
// ============================================================================

/**
 * @brief 触摸板读取回调函数
 *
 * 由 LVGL 调用，用于读取当前触摸状态
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
        if (x < MY_DISP_HOR_RES && y < MY_DISP_VER_RES) {
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

// ============================================================================
// 公共 API
// ============================================================================

void lv_port_indev_init(void) {
    ESP_LOGI(TAG, "Initializing LVGL input device (touchpad)");

    // 创建输入设备对象
    indev_touchpad = lv_indev_create();
    // 设置输入设备类型为指针设备
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    // 设置输入设备读取回调函数
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);

    ESP_LOGI(TAG, "LVGL input device initialized successfully");
}

lv_indev_t *lv_port_indev_get_touchpad(void) { return indev_touchpad; }
