/**
 * @file    ui_navbar.h
 * @brief   顶部导航栏模块对外接口
 *
 * @details 声明导航栏创建与页面切换接口。
 */
#ifndef UI_NAVBAR_H
#define UI_NAVBAR_H

#include "lvgl/lvgl.h"

/**
 * @brief   创建顶部导航栏(标题 + 4 个页面切换按钮)
 * @param   parent  父对象(通常为屏幕根对象)
 * @return  创建的导航栏对象指针
 */
lv_obj_t *create_top_bar(lv_obj_t *parent);

/**
 * @brief   切换到指定页面
 * @param   page_id  目标页面 ID(PAGE_HOME/PAGE_EVENT/PAGE_HISTORY/PAGE_SETTINGS)
 * @note    已在当前页则跳过,在主线程调用
 */
void switch_page(uint8_t page_id);

#endif /* UI_NAVBAR_H */
