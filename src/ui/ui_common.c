#include "ui_common.h"
#include <stdlib.h>

/*===================== 共享状态定义 =====================*/
lv_obj_t *pages[PAGE_COUNT];             /* 4 个页面容器 */
lv_obj_t *nav_btns[PAGE_COUNT];          /* 4 个导航按钮 */
uint8_t   cur_page = 0xFF;               /* 当前页面（0xFF 表示未初始化 */

/* 主页控件句柄 */
lv_obj_t *home_temp_lbl;                 /* 温度数值 */
lv_obj_t *home_humi_lbl;                 /* 湿度数值 */
lv_obj_t *home_chart;                    /* 温湿度曲线图 */
lv_chart_series_t *ser_temp;             /* 温度曲线 */
lv_chart_series_t *ser_humi;             /* 湿度曲线 */
lv_obj_t *timer_lbl;                     /* 时间显示 */

/* mock 数据与锁 */
float     mock_temp = 23.5f;
float     mock_humi = 40.0f;
uint8_t   connect_event = OFF;    /* 连接状态 */
uint8_t   LED_status = ON;                /* LED 状态 */
uint8_t   BEEP_status = OFF;               /* BEEP 状态 */
uint8_t   device_status = ON;                /* 设备 状态 */

pthread_rwlock_t th_rwlock;               /* 主线程读写锁 */
pthread_rwlock_t threshold_rwlock;       /* 温湿度阈值读写锁 */
pthread_rwlock_t history_rwlock;          /* 历史数据读写锁 */
pthread_mutex_t cond_mutex;             /* 条件变量互斥锁 */
pthread_cond_t cond_var;                /* 条件变量 */
pthread_mutex_t led_mutex;              /* LED 控制互斥锁 */
pthread_cond_t led_cond;                /* LED 控制条件变量 */
pthread_mutex_t beep_mutex;             /* BEEP 控制互斥锁 */
pthread_cond_t beep_cond;               /* BEEP 控制条件变量 */
pthread_mutex_t status_mutex;           /* 保护 LED_status/BEEP_status/connect_event/device_status */

/*===================== 线程安全 UI 更新机制 =====================*/
/* 声明外部函数避免头文件循环依赖 */
extern void update_event_page(char *event);
extern void display_threshold(void);
extern void display_history(int i, char *buf);

static pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool threshold_refresh_pending = false;
static bool history_refresh_pending = false;

typedef struct
{
    bool  used;
    char  text[128];
} pending_event_t;
typedef struct
{
    int i;
    char lines[128];
} history_event_t;
static history_event_t history_event = {0, ""};
static pending_event_t pending_events[MAX_PENDING_EVENTS];

/* 后台线程调用：将事件文本推入待处理队列（线程安全） */
void push_pending_event(const char *text)
{
    pthread_mutex_lock(&pending_mutex);
    for(int i = 0; i < MAX_PENDING_EVENTS; i++)
    {
        if(!pending_events[i].used)
        {
            pending_events[i].used = true;
            strncpy(pending_events[i].text, text, sizeof(pending_events[i].text) - 1);
            pending_events[i].text[sizeof(pending_events[i].text) - 1] = '\0';
            break;
        }
    }
    pthread_mutex_unlock(&pending_mutex);
}
/* 后台线程调用：请求主线程刷新历史数据卡片 */
void request_history_refresh(int i, char *lines)
{
    pthread_mutex_lock(&pending_mutex);
    history_event.i = i;
    strcpy(history_event.lines, lines);
    history_refresh_pending = true;
    pthread_mutex_unlock(&pending_mutex);
}
/* 主线程 lv_timer 调用：消费所有待处理事件，更新事件页 + 阈值显示 + 历史页 */
void process_pending_events(lv_timer_t *timer)
{
    char buf[128];
    bool need_threshold_refresh = false;

    pthread_mutex_lock(&pending_mutex);
    for(int i = 0; i < MAX_PENDING_EVENTS; i++)
    {
        if(pending_events[i].used)
        {
            strcpy(buf, pending_events[i].text);
            pending_events[i].used = false;
            pthread_mutex_unlock(&pending_mutex);
            update_event_page(buf);
            pthread_mutex_lock(&pending_mutex);
        }
    }
    if(threshold_refresh_pending)
    {
        threshold_refresh_pending = false;
        need_threshold_refresh = true;
    }
    pthread_mutex_unlock(&pending_mutex);

    if(need_threshold_refresh)
    {
        display_threshold();
    }
    if(history_refresh_pending)
    {
        history_refresh_pending = false;
        display_history(history_event.i, history_event.lines);
        history_event.i = 0;
        strcpy(history_event.lines, "");
    }

}

/* 后台线程调用：请求主线程刷新阈值显示 */
void request_threshold_refresh(void)
{
    pthread_mutex_lock(&pending_mutex);
    threshold_refresh_pending = true;
    pthread_mutex_unlock(&pending_mutex);
}

/*===================== 公共辅助函数 =====================*/
/* 创建一个数据卡片，返回卡片对象，数值标签句柄通过 out_value_lbl 返回 */
lv_obj_t *create_data_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           const char *name, const char *unit,
                           lv_color_t value_color, lv_obj_t **out_value_lbl)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 330, 200);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 卡片名称 */
    lv_obj_t *name_lbl = lv_label_create(card);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_font(name_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(name_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 20, 15);

    /* 大字体数值 */
    lv_obj_t *val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, "--.-");
    lv_obj_set_style_text_font(val_lbl, FONT_BIG, 0);
    lv_obj_set_style_text_color(val_lbl, value_color, 0);
    lv_obj_align(val_lbl, LV_ALIGN_LEFT_MID, 30, 15);

    /* 单位（贴在数值右下） */
    lv_obj_t *unit_lbl = lv_label_create(card);
    lv_label_set_text(unit_lbl, unit);
    lv_obj_set_style_text_font(unit_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(unit_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 15, 0);

    if(out_value_lbl) *out_value_lbl = val_lbl;
    return card;
}
/* 创建一个数据卡片，返回卡片对象，数值标签句柄通过 out_value_lbl 返回 */
lv_obj_t *make_card(lv_obj_t *parent,
                           lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h,
                           const char *title,
                           lv_color_t bg, lv_color_t title_color,
                           const lv_font_t *title_font)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, bg, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 8, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, title_font, 0);
    lv_obj_set_style_text_color(lbl, title_color, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    return card;
}
