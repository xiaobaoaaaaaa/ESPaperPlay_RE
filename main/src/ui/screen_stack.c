/*
 * screen_stack.c
 *
 * 屏幕栈实现文件
 * 提供屏幕对象的栈式管理功能，用于LVGL应用中的屏幕导航历史记录
 */

#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdbool.h>

#include "screens.h"

/** @brief 屏幕栈数组指针 */
lv_obj_t **screen_stack = NULL;

/** @brief 当前分配的屏幕栈容量 */
int screen_stack_size = 0;

/** @brief 屏幕栈顶指针，-1表示栈为空 */
int screen_stack_top = -1;

/**
 * @brief 重新分配屏幕栈内存
 *
 * 首次调用时分配初始内存。后续调用时，如果请求的大小超过当前大小，
 * 则进行内存扩展。内存从SPIRAM中分配，以节省宝贵的内部RAM。
 *
 * @param size 新请求的栈容量（屏幕对象数量）
 *
 * @note 该函数不能缩小栈大小，只能保持或扩展
 * @note 内存分配失败时会触发断言
 */
void screen_stack_realloc(int size) {
    if (screen_stack == NULL) {
        // 第一次分配：初始化屏幕栈
        screen_stack = heap_caps_malloc(sizeof(lv_obj_t *) * size, MALLOC_CAP_SPIRAM);
        if (screen_stack == NULL) {
            ESP_LOGE("screen_stack_realloc", "Failed to allocate memory for screen stack");
            assert(0);
        }
        screen_stack_size = size;
        screen_stack_top = -1;
    }

    if (size > screen_stack_size) {
        // 需要扩展栈的容量
        ESP_LOGI("screen_stack", "Expanding stack: %d -> %d", screen_stack_size, size);
        lv_obj_t **new_stack =
            heap_caps_realloc(screen_stack, sizeof(lv_obj_t *) * size, MALLOC_CAP_SPIRAM);
        if (new_stack == NULL) {
            ESP_LOGE("screen_stack_realloc", "Failed to reallocate memory for screen stack");
            assert(0);
        }
        screen_stack = new_stack;
        screen_stack_size = size;
    }
}

/**
 * @brief 从栈顶弹出一个屏幕
 *
 * 返回栈顶屏幕对象指针，并将栈顶指针向下移动一位。
 * 如果栈为空，返回NULL。
 *
 * @return 栈顶屏幕指针，栈为空时返回NULL
 *
 * @note 此函数会改变栈的状态，移动栈顶指针
 */
lv_obj_t *screen_stack_pop() {
    if (screen_stack_top >= 0) {
        // 栈非空，返回栈顶元素并弹出
        return screen_stack[screen_stack_top--];
    }

    ESP_LOGW("screen_stack", "Pop failed: stack is empty");
    return NULL;
}

/**
 * @brief 将指定屏幕压入栈
 *
 * 将外部提供的屏幕对象指针保存到屏幕栈中。
 * 如果栈满（栈顶到达容量-1），会自动扩展栈容量，每次增加5个单位。
 *
 * @param screen 要压入栈的屏幕对象指针
 *
 * @note 调用前需确保屏幕栈已初始化（调用screen_stack_init）
 * @note screen参数不能为NULL
 *
 * 使用流程：
 * 1. 检查是否需要扩展栈
 * 2. 移动栈顶指针向上
 * 3. 在新位置存储指定的屏幕
 */
void screen_stack_push(lv_obj_t *screen) {
    if (screen_stack_top >= screen_stack_size - 1) {
        // 栈满，需要扩展
        screen_stack_realloc(screen_stack_size + 5);
    }
    screen_stack_top++;
    screen_stack[screen_stack_top] = screen;
}

/**
 * @brief 查看栈顶屏幕而不移除
 *
 * 获取当前栈顶的屏幕对象指针，但不修改栈的状态。
 * 如果栈为空，输出参数指向NULL。
 *
 * @param out_screen 输出参数指针，指向存储栈顶屏幕的位置
 *
 * @note 调用前必须确保out_screen指针有效
 * @note 此函数不改变栈的结构，可安全调用多次
 *
 * 使用示例：
 *   lv_obj_t *screen = NULL;
 *   screen_stack_peek(&screen);
 *   if (screen != NULL) {
 *       // 栈非空，可以使用screen
 *   }
 */
void screen_stack_peek(lv_obj_t **out_screen) {
    if (screen_stack_top >= 0) {
        // 栈非空，返回栈顶元素
        *out_screen = screen_stack[screen_stack_top];
    } else {
        // 栈为空
        *out_screen = NULL;
        ESP_LOGW("screen_stack", "Peek: stack is empty");
    }
}

/**
 * @brief 初始化屏幕栈
 *
 * 该函数必须在应用启动时调用，在使用其他屏幕栈功能之前。
 * 它会分配初始的屏幕栈内存（初始容量为10个屏幕对象）。
 * 内存从SPIRAM中分配以节省内部RAM。
 *
 * 初始化步骤：
 * 1. 分配初始栈内存（容量10）
 * 2. 检查内存分配是否成功
 *
 * @note 如果内存分配失败，函数会记录错误并断言
 * @note 此函数应该在main函数或应用初始化代码中调用
 *
 * 使用示例：
 *   void app_main(void) {
 *       screen_stack_init();  // 初始化屏幕栈
 *       // ... 其他初始化代码 ...
 *   }
 */
void screen_stack_init() {
    ESP_LOGI("screen_stack", "Initializing screen stack...");
    screen_stack_realloc(10);
    if (screen_stack == NULL) {
        ESP_LOGE("screen_stack_init", "Failed to allocate memory for screen stack");
        assert(0);
    }
}