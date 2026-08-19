#ifndef UI_PAGE_HISTORY_H
#define UI_PAGE_HISTORY_H

#include "lvgl/lvgl.h"

/* 创建历史数据页（卡片 + 读取 data.txt 填表） */
void create_history_page(lv_obj_t *parent);

/* 将历史数据写入指定卡片（读取 ./data.txt 最近 20 条，最新在顶部） */
void write_history_data(lv_obj_t *card);

/* 更新历史数据卡片 */
void update_history_card(lv_obj_t *card, int i, char *lines);


#endif /* UI_PAGE_HISTORY_H */
