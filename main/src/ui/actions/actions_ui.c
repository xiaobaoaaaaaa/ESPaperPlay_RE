#include "actions.h"
#include "eez-flow.h"
#include "vars.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi.h"

void action_change_to_previous_screen(lv_event_t *e) {
    // 获取gesture方向
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_event_get_indev(e));
    if (dir == LV_DIR_RIGHT) {
        // 左滑，返回上一个屏幕，使用 eez flow 屏幕栈管理
        ESP_LOGI("screen_change", "Popping screen with eez_flow");
        eez_flow_pop_screen(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0);
    }
}

void action_get_signal_strength(lv_event_t *e) {
    if (is_wifi_connected()) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            switch (ap_info.rssi) {
            case -60 ... 0:
                set_var_wifi_signal(4);
                break;
            case -67 ... - 61:
                set_var_wifi_signal(3);
                break;
            case -75 ... - 68:
                set_var_wifi_signal(2);
                break;

            default:
                set_var_wifi_signal(1);
                break;
            }
        } else {
            set_var_wifi_signal(0); // 获取信号强度失败，设为0
        }
    } else {
        set_var_wifi_signal(0); // WiFi未连接，设为0
    }
}