/**
 * @file    ui_manager.h
 * @brief   UI 框架初始化模块对外接口
 *
 * @details 声明 create_ui() 函数,该函数在 main.c 启动后台线程前调用,
 *          负责构建整个应用的可视框架(导航栏 + 4 个页面)。
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl/lvgl.h"

/**
 * @brief  创建整个界面:顶部导航栏 + 4 个页面,默认显示主页
 *
 * @details 同时初始化读写锁、时间标签、定时刷新时间。
 *          调用前需保证 LVGL 已初始化(lv_init + 显示/输入设备注册)。
 *          必须在所有后台线程启动前调用。
 */
void create_ui(void);

#endif /* UI_MANAGER_H */
