/*
 * screen_stack.h
 *
 * 屏幕栈管理模块
 * 用于管理LVGL屏幕的栈式结构，支持屏幕的压入、弹出、查看等操作
 *
 * 功能描述：
 * - 提供动态可扩展的屏幕栈
 * - 支持屏幕压入和弹出操作
 * - 使用SPIRAM存储以减少内部RAM占用
 * - 自动管理内存分配和重新分配
 *
 * 使用示例：
 *   // 初始化屏幕栈
 *   screen_stack_init();
 *
 *   // 保存当前屏幕到栈
 *   screen_stack_push();
 *
 *   // 获取栈顶屏幕
 *   lv_obj_t *screen = NULL;
 *   screen_stack_peek(&screen);
 *
 *   // 弹出屏幕
 *   screen_stack_pop();
 */

#ifndef SCREEN_STACK_H
#define SCREEN_STACK_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 屏幕栈数据结构
 *
 * 全局屏幕栈，用于存储LVGL屏幕对象指针
 */
extern lv_obj_t *screen_stack;

/**
 * @brief 屏幕栈当前大小
 *
 * 表示当前分配的屏幕栈容量（能容纳的最大屏幕数）
 */
extern int screen_stack_size;

/**
 * @brief 屏幕栈顶指针
 *
 * 指向栈中最后一个有效元素的索引
 * 值为-1表示栈为空
 */
extern int screen_stack_top;

/**
 * @brief 初始化屏幕栈
 *
 * 分配初始内存并初始化屏幕栈。必须在使用其他屏幕栈函数前调用。
 * 初始容量为10个屏幕对象，不足时会自动扩展。
 * 内存从SPIRAM中分配。
 *
 * @return 无
 *
 * @note 如果内存分配失败，函数会断言并停止执行
 */
void screen_stack_init(void);

/**
 * @brief 重新分配屏幕栈内存
 *
 * 调整屏幕栈的容量。如果新大小小于当前大小，则不做任何操作。
 * 自动处理内存分配和复制。
 * 内存从SPIRAM中分配。
 *
 * @param size 新的屏幕栈容量（能容纳的屏幕数量）
 *
 * @return 无
 *
 * @note 如果内存分配失败，函数会断言并停止执行
 * @note 只能扩展，不能缩小
 */
void screen_stack_realloc(int size);

/**
 * @brief 将指定屏幕压入栈
 *
 * 将外部提供的屏幕对象指针保存到屏幕栈中。
 * 如果栈满，自动扩展栈的容量（增加5个单位）。
 *
 * @param screen 要压入栈的屏幕对象指针
 *
 * @return 无
 *
 * @note 调用前确保屏幕栈已初始化（调用screen_stack_init）
 * @note screen参数不能为NULL
 */
void screen_stack_push(lv_obj_t *screen);

/**
 * @brief 从栈中弹出屏幕
 *
 * 返回栈顶屏幕对象指针，并将栈顶指针向下移动一位。
 * 如果栈为空，返回NULL。
 *
 * @return 栈顶屏幕对象指针，如果栈为空则返回NULL
 *
 * @note 调用后栈顶指针会移动
 * @note 栈为空时返回NULL
 */
lv_obj_t *screen_stack_pop(void);

/**
 * @brief 获取栈顶屏幕而不移除
 *
 * 返回当前栈顶的屏幕对象指针，不修改栈的结构。
 * 如果栈为空，输出参数设置为NULL。
 *
 * @param out_screen 输出参数，指向存储栈顶屏幕指针的位置
 *
 * @return 无
 *
 * @note 调用前必须确保out_screen指针有效
 * @note 栈为空时，*out_screen被设置为NULL
 */
void screen_stack_peek(lv_obj_t **out_screen);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_STACK_H
