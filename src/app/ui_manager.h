#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl/lvgl.h"

/* 创建整个界面：顶部导航栏 + 4 个页面，默认显示主页
 * 同时初始化读写锁、时间标签、定时刷新时间 */
void create_ui(void);

#endif /* UI_MANAGER_H */
