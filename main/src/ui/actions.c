#include "actions.h"
#include "vars.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "yiyan.h"

#define YIYAN_INTERVAL_MS (3 * 60 * 1000) // 3分钟

TaskHandle_t get_yiyan_task_handle = NULL;
void get_yiyan_task(void *pvParameters) {
    while (1) {
        // 获取一言
        char *yiyan_str = NULL;
        esp_err_t ret = get_yiyan(&yiyan_str);
        if (ret == ESP_OK && yiyan_str != NULL) {
            set_var_yiyan(yiyan_str);
            free(yiyan_str);
        } else {
            set_var_yiyan("获取一言失败");
            ESP_LOGE("get_yiyan_task", "get_yiyan failed with error: %s", esp_err_to_name(ret));
        }

        // 等待3分钟或收到立即执行的通知
        // ulTaskNotifyTake 会阻塞直到收到通知或超时
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(YIYAN_INTERVAL_MS));
    }

    // 正常情况下不会执行到这里，但为了安全起见保留
    get_yiyan_task_handle = NULL;
    vTaskDelete(NULL);
}

void action_get_yiyan(lv_event_t *e) {
    if (get_yiyan_task_handle == NULL) {
        // 任务不存在，创建新任务
        xTaskCreate(get_yiyan_task, "get_yiyan_task", 4096, NULL, 5, &get_yiyan_task_handle);
    } else {
        // 任务已存在，通知任务立即执行
        xTaskNotifyGive(get_yiyan_task_handle);
    }
}