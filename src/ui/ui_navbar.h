#ifndef UI_NAVBAR_H
#define UI_NAVBAR_H

#include "lvgl/lvgl.h"

/* 创建顶部导航栏（标题 + 4 个页面切换按钮） */
lv_obj_t *create_top_bar(lv_obj_t *parent);

/* 切换到指定页面（按 PAGE_HOME/PAGE_REALTIME/...） */
void switch_page(uint8_t page_id);

#endif /* UI_NAVBAR_H */
