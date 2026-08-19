#ifndef UI_PAGE_EVENT_H
#define UI_PAGE_EVENT_H

#include "lvgl/lvgl.h"


/* 创建事件响应页面：左侧事件表格 + 右侧统计卡片 */
extern void create_event_page(lv_obj_t *parent);

/* 更新事件响应页面 */
extern void update_event_page(char *event);

/* 更新系统状态卡片 */
extern void update_system_status(void);

#endif /* UI_PAGE_EVENT_H */
