/**
 * @file    ui_page_event.h
 * @brief   事件页模块对外接口
 *
 * @details 声明事件页创建、事件更新与系统状态更新接口。
 */
#ifndef UI_PAGE_EVENT_H
#define UI_PAGE_EVENT_H

#include "lvgl/lvgl.h"


/**
 * @brief   创建事件响应页面:左侧事件表格 + 右侧 3 个统计卡片
 * @param   parent  父页面容器
 */
extern void create_event_page(lv_obj_t *parent);

/**
 * @brief   更新事件响应页面:写入新事件到表格 + 刷新统计卡片
 * @param   event  事件文本,约定前缀 "alarm:"/"command:"/"system:"
 * @note    事件文本内容决定更新哪个统计卡片
 */
extern void update_event_page(char *event);

/**
 * @brief   更新系统状态卡片(设备运行状态)
 * @note    根据 device_status 全局变量刷新卡片3显示
 */
extern void update_system_status(void);

#endif /* UI_PAGE_EVENT_H */
