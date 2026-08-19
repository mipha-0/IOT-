#ifndef UI_PAGE_HOME_H
#define UI_PAGE_HOME_H

#include "lvgl/lvgl.h"

/* 创建主页：左侧温湿度卡片 + 右侧曲线图 */
void create_home_page(lv_obj_t *parent);

/* 更新主页温湿度数值显示 */
void update_home_data(float temp, float humidity);

/* 定时刷新主页温湿度数值与曲线（由 lv_timer 周期调用） */
void* mock_data_update(void* arg);
#endif /* UI_PAGE_HOME_H */
