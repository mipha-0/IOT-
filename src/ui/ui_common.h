#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

/*===================== 页面 ID =====================*/
enum
{
    PAGE_HOME = 0,
    PAGE_EVENT,
    PAGE_HISTORY,
    PAGE_SETTINGS,
    PAGE_COUNT
};

/*===================== 颜色方案 =====================*/
#define COLOR_NAV_BG        lv_color_hex(0x1F3A5F)   /* 导航栏底色 */
#define COLOR_NAV_ACTIVE    lv_color_hex(0x3E6FA3)   /* 选中按钮底色 */
#define COLOR_SCR_BG        lv_color_hex(0xF0F2F5)   /* 屏幕底色 */
#define COLOR_CARD_BG       lv_color_hex(0xFFFFFF)   /* 卡片底色 */
#define COLOR_TEMP          lv_color_hex(0xE8554E)   /* 温度色 */
#define COLOR_HUMI          lv_color_hex(0x2E86DE)   /* 湿度色 */
#define COLOR_TEXT_DARK     lv_color_hex(0x333333)   /* 主文字色 */
#define COLOR_TEXT_GRAY     lv_color_hex(0x8A8A8A)   /* 次文字色 */
#define COLOR_TEXT_RED      lv_color_hex(0xFF0000)   /* 红色文字色 */
#define COLOR_TEXT_GREEN    lv_color_hex(0x00FF00)   /* 绿色文字色 */

#define COLOR_ALARM         lv_color_hex(0xE8554E)    /* 报警红 */
#define COLOR_OK            lv_color_hex(0x2ECC71)    /* 正常绿 */
#define COLOR_DIV           lv_color_hex(0xE0E0E0)   /* 分割线 */

/*===================== 字体 =====================*/
#define FONT_CJK    &lv_font_simsun_16_cjk   /* 中英文文本 */
#define FONT_BIG    &lv_font_montserrat_16   /* 大字体数值 */

/*===================== 布局常量 =====================*/
#define SCR_W           800
#define SCR_H           480
#define NAV_H           50
#define CONTENT_H       (SCR_H - NAV_H)
#define CHART_POINTS    30   /* 曲线数据点数 */
#define PAD           8
#define GAP           8
#define LEFT_W        520                          /* 左侧表格宽 */
#define RIGHT_W       SCR_W - PAD * 2 - GAP - LEFT_W /* 右侧卡片区宽 256 */
#define RIGHT_X       PAD + LEFT_W + GAP           /* 右侧起始 x = 536 */
#define AREA_H        CONTENT_H - PAD * 2          /* 414 */
#define CARD_H        (AREA_H - GAP * 2) / 3        /* 132 */
#define TIME_COL_W    170                          /* 表格时间列宽 */
#define card_x       8  
#define card_y       8
#define card_w       SCR_W - 16
#define card_h       CONTENT_H - 16
#define ON    1                      /* 连接状态：已连接 */
#define OFF   0                      /* 连接状态：未连接 */
#define connect_event_str(connect_event) (connect_event == ON ? "connected" : "disconnect")
#define LED_status_str(LED_status) (LED_status == ON ? "ON" : "OFF")
#define BEEP_status_str(BEEP_status) (BEEP_status == ON ? "ON" : "OFF")
#define device_status_str(device_status) (device_status == ON ? "normal" : "alarm")
/*===================== 共享状态（在 ui_common.c 中定义） =====================*/
extern lv_obj_t *pages[PAGE_COUNT];          /* 4 个页面容器 */
extern lv_obj_t *nav_btns[PAGE_COUNT];       /* 4 个导航按钮 */
extern uint8_t   cur_page;                   /* 当前页面 */

/* 主页控件句柄 */
extern lv_obj_t *home_temp_lbl;              /* 温度数值 */
extern lv_obj_t *home_humi_lbl;              /* 湿度数值 */
extern lv_obj_t *home_chart;                 /* 温湿度曲线图 */
extern lv_chart_series_t *ser_temp;          /* 温度曲线 */
extern lv_chart_series_t *ser_humi;          /* 湿度曲线 */
extern lv_obj_t *timer_lbl;                  /* 时间显示 */

/* mock 数据与锁 */
extern float     mock_temp;
extern float     mock_humi;
extern uint8_t   connect_event;             /* 连接状态 */
extern uint8_t   LED_status;                /* LED 状态 */
extern uint8_t   BEEP_status;               /* BEEP 状态 */
extern uint8_t   device_status;                /* 设备 状态 */
extern pthread_rwlock_t history_rwlock;
extern pthread_rwlock_t th_rwlock;
extern pthread_mutex_t cond_mutex;
extern pthread_cond_t cond_var;
extern pthread_rwlock_t threshold_rwlock;
extern pthread_mutex_t led_mutex;
extern pthread_cond_t led_cond;
extern pthread_mutex_t beep_mutex;
extern pthread_cond_t beep_cond;
extern pthread_mutex_t status_mutex;   /* 保护 LED_status/BEEP_status/connect_event/device_status */

/*温湿度结构体*/
typedef struct temp_humi_t
{
    float temp;
    float humi;
} temp_humi_t;
/*===================== 线程安全 UI 更新机制 =====================*/
/* 后台线程不能直接操作 LVGL 控件（LVGL 非线程安全）。
   后台线程通过 push_pending_event() 推入事件字符串，
   由主线程的 lv_timer 定期调用 process_pending_events() 消费。 */
#define MAX_PENDING_EVENTS 16

/* 后台线程调用：将事件文本推入待处理队列（线程安全） */
extern void push_pending_event(const char *text);

/* 主线程 lv_timer 调用：消费所有待处理事件，更新事件页 + 阈值显示 */
extern void process_pending_events(lv_timer_t *timer);

/* 后台线程调用：请求主线程刷新阈值显示（设置标志位） */
extern void request_threshold_refresh(void);

/* 后台线程调用：请求主线程刷新历史数据页（有新数据到来） */
extern void request_history_refresh(int i, char *lines);

/*===================== 公共辅助函数 =====================*/
/* 创建一个数据卡片，返回卡片对象，数值标签句柄通过 out_value_lbl 返回 */
extern lv_obj_t *create_data_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           const char *name, const char *unit,
                           lv_color_t value_color, lv_obj_t **out_value_lbl);

extern lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h,
                      const char *title,
                      lv_color_t bg, lv_color_t title_color,
                      const lv_font_t *title_font);
#endif /* UI_COMMON_H */
