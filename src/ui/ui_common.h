#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

/*===================== 页面 ID =====================*/
/**
 * @brief 页面 ID 枚举
 * @note  PAGE_COUNT 必须放最后,表示页面总数,用于遍历 pages[]/nav_btns[]
 */
enum
{
    PAGE_HOME = 0,    /* 主页:温湿度卡片 + 曲线图 */
    PAGE_EVENT,       /* 事件页:事件表格 + 统计卡片 */
    PAGE_HISTORY,     /* 历史页:data.txt 表格展示 */
    PAGE_SETTINGS,    /* 设置页:MQTT 配置 + 阈值 + 开关 */
    PAGE_COUNT        /* 页面总数(必须在最后) */
};

/*===================== 颜色方案 =====================*/
/* 全局颜色统一在此定义,确保 UI 风格一致 */
#define COLOR_NAV_BG        lv_color_hex(0x1F3A5F)   /* 导航栏底色:深蓝 */
#define COLOR_NAV_ACTIVE    lv_color_hex(0x3E6FA3)   /* 选中按钮底色:中蓝 */
#define COLOR_SCR_BG        lv_color_hex(0xF0F2F5)   /* 屏幕底色:浅灰 */
#define COLOR_CARD_BG       lv_color_hex(0xFFFFFF)   /* 卡片底色:白 */
#define COLOR_TEMP          lv_color_hex(0xE8554E)   /* 温度色:橙红 */
#define COLOR_HUMI          lv_color_hex(0x2E86DE)    /* 湿度色:蓝 */
#define COLOR_TEXT_DARK     lv_color_hex(0x333333)   /* 主文字色:深灰 */
#define COLOR_TEXT_GRAY     lv_color_hex(0x8A8A8A)   /* 次文字色:中灰 */
#define COLOR_TEXT_RED      lv_color_hex(0xFF0000)   /* 红色文字色 */
#define COLOR_TEXT_GREEN    lv_color_hex(0x00FF00)   /* 绿色文字色 */

#define COLOR_ALARM         lv_color_hex(0xE8554E)    /* 报警红 */
#define COLOR_OK            lv_color_hex(0x2ECC71)    /* 正常绿 */
#define COLOR_DIV           lv_color_hex(0xE0E0E0)   /* 分割线:浅灰 */

/*===================== 字体 =====================*/
#define FONT_CJK    &lv_font_simsun_16_cjk   /* 中英文文本字体 */
#define FONT_BIG    &lv_font_montserrat_16   /* 大字体数值(数字) */

/*===================== 布局常量 =====================*/
/* 屏幕分辨率 800x480,与硬件 LCD 一致 */
#define SCR_W           800                       /* 屏幕宽 */
#define SCR_H           480                       /* 屏幕高 */
#define NAV_H           50                        /* 导航栏高 */
#define CONTENT_H       (SCR_H - NAV_H)           /* 内容区高 = 430 */
#define CHART_POINTS    30                        /* 曲线数据点数 */
#define PAD           8                            /* 通用内边距 */
#define GAP           8                            /* 通用元素间距 */
#define LEFT_W        520                          /* 左侧表格宽 */
#define RIGHT_W       SCR_W - PAD * 2 - GAP - LEFT_W /* 右侧卡片区宽 256 */
#define RIGHT_X       PAD + LEFT_W + GAP           /* 右侧起始 x = 536 */
#define AREA_H        CONTENT_H - PAD * 2          /* 内容区可用高 414 */
#define CARD_H        (AREA_H - GAP * 2) / 3        /* 单个卡片高 132 */
#define TIME_COL_W    170                          /* 表格时间列宽 */
#define card_x       8                              /* 历史卡片 x */
#define card_y       8                              /* 历史卡片 y */
#define card_w       SCR_W - 16                     /* 历史卡片宽 */
#define card_h       CONTENT_H - 16                 /* 历史卡片高 */
#define ON    1                      /* 连接状态:已连接 / 设备开启 */
#define OFF   0                      /* 连接状态:未连接 / 设备关闭 */
/* 状态字符串化宏:供事件页卡片显示 */
#define connect_event_str(connect_event) (connect_event == ON ? "connected" : "disconnect")
#define LED_status_str(LED_status) (LED_status == ON ? "ON" : "OFF")
#define BEEP_status_str(BEEP_status) (BEEP_status == ON ? "ON" : "OFF")
#define device_status_str(device_status) (device_status == ON ? "normal" : "alarm")

/*===================== 共享状态（在 ui_common.c 中定义） =====================*/
/* 这些全局变量由 ui_common.c 定义,各模块通过 extern 引用 */
extern lv_obj_t *pages[PAGE_COUNT];          /* 4 个页面容器 */
extern lv_obj_t *nav_btns[PAGE_COUNT];       /* 4 个导航按钮 */
extern uint8_t   cur_page;                   /* 当前页面 */

/* 主页控件句柄 */
extern lv_obj_t *home_temp_lbl;              /* 温度数值标签 */
extern lv_obj_t *home_humi_lbl;              /* 湿度数值标签 */
extern lv_obj_t *home_chart;                 /* 温湿度曲线图 */
extern lv_chart_series_t *ser_temp;          /* 温度曲线序列 */
extern lv_chart_series_t *ser_humi;          /* 湿度曲线序列 */
extern lv_obj_t *timer_lbl;                  /* 时间显示标签 */

/* 共享数据与状态(由后台线程写,主线程读;需配合同步原语) */
extern float     mock_temp;                  /* 当前温度(HTTP 拉取) */
extern float     mock_humi;                  /* 当前湿度(HTTP 拉取) */
extern uint8_t   connect_event;             /* TCP 连接状态:ON/OFF */
extern uint8_t   LED_status;                /* LED 开关状态:ON/OFF */
extern uint8_t   BEEP_status;               /* 蜂鸣器开关状态:ON/OFF */
extern uint8_t   device_status;                /* 设备运行状态:ON=正常/OFF=报警 */

/* 跨线程同步原语 */
extern pthread_rwlock_t history_rwlock;       /* 历史数据读写锁(预留) */
extern pthread_rwlock_t th_rwlock;            /* 保护 mock_temp/mock_humi */
extern pthread_mutex_t cond_mutex;            /* 配合 cond_var 使用的互斥锁 */
extern pthread_cond_t cond_var;               /* 数据更新条件变量(新数据通知) */
extern pthread_rwlock_t threshold_rwlock;    /* 保护 temp_threshold/humi_threshold */
extern pthread_mutex_t led_mutex;             /* 配合 led_cond 使用的互斥锁 */
extern pthread_cond_t led_cond;               /* LED 控制唤醒条件变量 */
extern pthread_mutex_t beep_mutex;            /* 配合 beep_cond 使用的互斥锁 */
extern pthread_cond_t beep_cond;              /* 蜂鸣器控制唤醒条件变量 */
extern pthread_mutex_t status_mutex;          /* 保护 LED_status/BEEP_status/connect_event/device_status */

/*温湿度结构体*/
/**
 * @brief 温湿度数据结构体
 * @note  用于 httprequest_weather() 返回值与跨函数传递
 */
typedef struct temp_humi_t
{
    float temp;   /* 温度(摄氏度) */
    float humi;   /* 湿度(百分比) */
} temp_humi_t;

/*===================== 线程安全 UI 更新机制 =====================*/
/* 后台线程不能直接操作 LVGL 控件（LVGL 非线程安全）。
   后台线程通过 push_pending_event() 推入事件字符串，
   由主线程的 lv_timer 定期调用 process_pending_events() 消费。 */
#define MAX_PENDING_EVENTS 16   /* 待处理事件队列容量(环形缓冲,溢出丢弃) */

/**
 * @brief   后台线程调用:将事件文本推入待处理队列(线程安全)
 * @param   text  事件文本(将被复制到内部缓冲,长度上限 127)
 * @note    队列满时新事件被丢弃,不阻塞调用方
 */
extern void push_pending_event(const char *text);

/**
 * @brief   主线程 lv_timer 调用:消费所有待处理事件,更新事件页 + 阈值显示 + 历史页
 * @param   timer  LVGL 定时器句柄(未使用)
 * @note    200ms 周期由 main.c 创建定时器时设定
 */
extern void process_pending_events(lv_timer_t *timer);

/**
 * @brief   后台线程调用:请求主线程刷新阈值显示(设置标志位)
 * @note    仅置位标志,实际刷新由主线程 process_pending_events 完成
 */
extern void request_threshold_refresh(void);

/**
 * @brief   后台线程调用:请求主线程刷新历史数据页(有新数据到来)
 * @param   i     数据行号(0~19)
 * @param   lines 该行的数据字符串
 * @note    仅保留最新一次请求(覆盖式),非队列式
 */
extern void request_history_refresh(int i, char *lines);

/*===================== 公共辅助函数 =====================*/
/**
 * @brief   创建一个数据卡片(用于主页温湿度显示)
 * @param   parent       父对象
 * @param   x,y          卡片在父对象中的坐标
 * @param   name         卡片名称(如 "temp"、"humi")
 * @param   unit         单位文本(如 "C"、"%RH")
 * @param   value_color  数值文字颜色
 * @param   out_value_lbl 输出参数,返回数值标签句柄(可为 NULL)
 * @return  创建的卡片对象指针
 * @note    卡片固定 330x200 大小,样式自带圆角与阴影
 */
extern lv_obj_t *create_data_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           const char *name, const char *unit,
                           lv_color_t value_color, lv_obj_t **out_value_lbl);

/**
 * @brief   创建一个通用卡片(用于事件页统计卡片等)
 * @param   parent       父对象
 * @param   x,y          卡片坐标
 * @param   w,h          卡片宽高
 * @param   title        卡片标题
 * @param   bg           卡片背景色
 * @param   title_color  标题文字颜色
 * @param   title_font   标题字体
 * @return  创建的卡片对象指针(可在其上继续添加子控件)
 */
extern lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h,
                      const char *title,
                      lv_color_t bg, lv_color_t title_color,
                      const lv_font_t *title_font);
#endif /* UI_COMMON_H */
