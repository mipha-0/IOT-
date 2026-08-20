/**
 * @file    ui_common.c
 * @brief   UI 共享中心实现:全局状态定义 + 跨线程事件队列
 *
 * @details 该文件定义了所有跨模块共享的全局变量(控件句柄、状态、锁),
 *          并实现了线程安全的 UI 更新机制(pending event 队列)。
 *
 *          pending event 机制:
 *          - 后台线程通过 push_pending_event/request_*_refresh 推入数据;
 *          - 主线程 lv_timer 周期调用 process_pending_events 消费;
 *          - 保证所有 LVGL API 调用都在主线程,避免线程安全问题。
 *
 *          队列实现:
 *          - 16 槽位数组,used 标记占用,满则丢弃新事件;
 *          - 单 mutex 保护,消费时短暂释放锁以调用 LVGL。
 *
 *          依赖关系:
 *          - update_event_page() / display_threshold() / display_history() / update_home_page() / update_timer_page()
 *            通过 extern 声明,实际定义在 ui_page_*.c
 */

#include "ui_common.h"
#include <stdlib.h>

/*===================== 共享状态定义 =====================*/
lv_obj_t *pages[PAGE_COUNT];             /* 4 个页面容器 */
lv_obj_t *nav_btns[PAGE_COUNT];          /* 4 个导航按钮 */
uint8_t   cur_page = 0xFF;               /* 当前页面(0xFF 表示未初始化,首次 switch_page 必然切换) */

/* 主页控件句柄 */
lv_obj_t *home_temp_lbl;                 /* 温度数值标签 */
lv_obj_t *home_humi_lbl;                 /* 湿度数值标签 */
lv_obj_t *home_chart;                    /* 温湿度曲线图 */
lv_chart_series_t *ser_temp;             /* 温度曲线序列 */
lv_chart_series_t *ser_humi;             /* 湿度曲线序列 */
lv_obj_t *timer_lbl;                     /* 时间显示标签 */

/* 共享数据与状态(初始值,运行时由后台线程更新) */
float     mock_temp = 23.5f;             /* 当前温度,初值 23.5°C */
float     mock_humi = 40.0f;             /* 当前湿度,初值 40% */
uint8_t   connect_event = OFF;           /* TCP 连接状态,初值未连接 */
uint8_t   LED_status = ON;                /* LED 开关状态,初值开 */
uint8_t   BEEP_status = OFF;              /* 蜂鸣器开关状态,初值关 */
uint8_t   device_status = ON;             /* 设备运行状态,初值正常 */

/* 跨线程同步原语(在 ui_manager.c::create_ui 中初始化) */
pthread_rwlock_t th_rwlock;               /* 主线程读写锁(保护温湿度) */
pthread_rwlock_t threshold_rwlock;        /* 温湿度阈值读写锁 */
pthread_rwlock_t history_rwlock;         /* 历史数据读写锁 */
pthread_mutex_t cond_mutex;              /* 条件变量互斥锁 */
pthread_cond_t cond_var;                 /* 条件变量(新数据通知) */
pthread_mutex_t led_mutex;               /* LED 控制互斥锁 */
pthread_cond_t led_cond;                 /* LED 控制唤醒条件变量 */
pthread_mutex_t beep_mutex;              /* BEEP 控制互斥锁 */
pthread_cond_t beep_cond;                /* BEEP 控制唤醒条件变量 */
pthread_mutex_t status_mutex;            /* 保护 LED_status/BEEP_status/connect_event/device_status */

/*===================== 线程安全 UI 更新机制 =====================*/
/* 声明外部函数避免头文件循环依赖
 * 这五个函数分别在 ui_page_event.c / ui_page_settings.c / ui_page_history.c / ui_page_home.c 中定义
 */
extern void update_event_page(char *event);
extern void display_threshold(void);
extern void display_history(int i, char *buf);
extern void update_home_ui(float temp, float humi, bool temp_alarm, bool humi_alarm);
extern void update_system_status(void);

static pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;  /* 保护 pending 队列的互斥锁(静态初始化) */
static bool threshold_refresh_pending = false;                     /* 阈值刷新请求标志 */
static bool history_refresh_pending = false;                       /* 历史页刷新请求标志 */
static bool system_status_refresh_pending = false;                 /* 系统状态卡片刷新请求标志 */

/**
 * @brief 待处理事件项
 * @note  used=true 表示该槽位已被占用,text 保存事件文本
 */
typedef struct
{
    bool  used;
    char  text[128];
} pending_event_t;

/**
 * @brief 历史数据更新事件
 * @note  i 为行号,lines 为该行数据字符串
 */
typedef struct
{
    int i;
    char lines[128];
} history_event_t;
static history_event_t history_event = {0, ""};          /* 历史更新事件(覆盖式) */

/**
 * @brief 主页刷新事件(覆盖式)
 * @note  后台线程通过 request_home_refresh 写入,主线程 process_pending_events 消费
 */
typedef struct
{
    bool  used;
    float temp;
    float humi;
    bool  temp_alarm;
    bool  humi_alarm;
} home_refresh_t;
static home_refresh_t home_refresh = {false, 0, 0, false, false};

static pending_event_t pending_events[MAX_PENDING_EVENTS]; /* 待处理事件环形队列 */

/**
 * @brief   后台线程调用:将事件文本推入待处理队列(线程安全)
 * @param   text  事件文本(长度上限 127,超出截断)
 *
 * @details 遍历 pending_events 数组,找到第一个 used=false 的槽位写入。
 *          队列满时新事件被静默丢弃(不阻塞,不报错)。
 *
 * @note    strncpy 后手动添加 '\0' 保证字符串终止。
 *          调用方无需持锁,函数内部加锁。
 */
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

/**
 * @brief   后台线程调用:请求主线程刷新历史数据卡片
 * @param   i     数据行号
 * @param   lines 该行数据字符串
 *
 * @details 覆盖式更新 history_event,设置 history_refresh_pending 标志。
 *          主线程下次调用 process_pending_events 时会消费此事件。
 *
 * @note    使用 strcpy 而非 strncpy,调用方需保证 lines 长度 < 128。
 */
void request_history_refresh(int i, char *lines)
{
    pthread_mutex_lock(&pending_mutex);
    history_event.i = i;
    strcpy(history_event.lines, lines);
    history_refresh_pending = true;
    pthread_mutex_unlock(&pending_mutex);
}

/**
 * @brief   主线程 lv_timer 调用:消费所有待处理事件,更新 UI
 * @param   timer  LVGL 定时器句柄(未使用)
 *
 * @details 工作流程:
 *          1. 持锁扫描 pending_events,取出所有 used=true 的事件;
 *          2. 每取出一个,临时释放锁,调用 update_event_page() 更新 UI;
 *             (LVGL API 在持锁期间调用会延长锁持有时间,影响后台线程推入)
 *          3. 检查 threshold_refresh_pending,如为 true 则调 display_threshold();
 *          4. 检查 home_refresh.used,如为 true 则调 update_home_ui();
 *          5. 检查 system_status_refresh_pending,如为 true 则调 update_system_status();
 *          6. 检查 history_refresh_pending,如为 true 则调 display_history()。
 *
 * @note    update_event_page 内部根据事件文本内容(alarm/command/system)
 *          分支更新统计卡片,因此事件文本格式需与 update_event_page 约定一致。
 */
void process_pending_events(lv_timer_t *timer)
{
    char buf[128];
    bool need_threshold_refresh = false;
    bool need_home_refresh = false;
    bool need_status_refresh = false;
    home_refresh_t hr_copy;

    pthread_mutex_lock(&pending_mutex);
    for(int i = 0; i < MAX_PENDING_EVENTS; i++)
    {
        if(pending_events[i].used)
        {
            strcpy(buf, pending_events[i].text);
            pending_events[i].used = false;
            /* 临时释放锁:LVGL 调用可能耗时,避免长时间持锁 */
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
    if(home_refresh.used)
    {
        home_refresh.used = false;
        hr_copy = home_refresh;
        need_home_refresh = true;
    }
    if(system_status_refresh_pending)
    {
        system_status_refresh_pending = false;
        need_status_refresh = true;
    }
    pthread_mutex_unlock(&pending_mutex);

    if(need_threshold_refresh)
    {
        display_threshold();
    }
    if(need_home_refresh)
    {
        update_home_ui(hr_copy.temp, hr_copy.humi, hr_copy.temp_alarm, hr_copy.humi_alarm);
    }
    if(need_status_refresh)
    {
        need_status_refresh = false;
        update_system_status();
    }
    if(history_refresh_pending)
    {
        history_refresh_pending = false;
        display_history(history_event.i, history_event.lines);
        history_event.i = 0;
        strcpy(history_event.lines, "");
    }

}

/**
 * @brief   后台线程调用:请求主线程刷新阈值显示
 * @details 仅置位 threshold_refresh_pending 标志,
 *          实际刷新由主线程 process_pending_events 完成。
 */
void request_threshold_refresh(void)
{
    pthread_mutex_lock(&pending_mutex);
    threshold_refresh_pending = true;
    pthread_mutex_unlock(&pending_mutex);
}

/**
 * @brief   后台线程调用:请求主线程刷新系统状态卡片
 * @details 仅置位 system_status_refresh_pending 标志,
 *          实际刷新由主线程 process_pending_events 完成。
 *          用于设备状态由报警恢复正常时:不上报事件,只更新显示文字。
 */
void request_system_status_refresh(void)
{
    pthread_mutex_lock(&pending_mutex);
    system_status_refresh_pending = true;
    pthread_mutex_unlock(&pending_mutex);
}

/**
 * @brief   后台线程调用:请求主线程刷新主页温湿度数值与颜色
 * @param   temp        当前温度
 * @param   humi        当前湿度
 * @param   temp_alarm  温度是否超阈值(决定数值颜色)
 * @param   humi_alarm  湿度是否超阈值(决定数值颜色)
 *
 * @details 覆盖式更新 home_refresh,设置 used 标志。
 *          主线程下次调用 process_pending_events 时会消费此事件。
 *
 * @note    调用方无需持锁,函数内部加锁。
 *          该函数只投递 UI 刷新请求,device_status 与 alarm 事件的更新
 *          应由调用方在调用前完成(参见 update_home_data)。
 */
void request_home_refresh(float temp, float humi, bool temp_alarm, bool humi_alarm)
{
    pthread_mutex_lock(&pending_mutex);
    home_refresh.used = true;
    home_refresh.temp = temp;
    home_refresh.humi = humi;
    home_refresh.temp_alarm = temp_alarm;
    home_refresh.humi_alarm = humi_alarm;
    pthread_mutex_unlock(&pending_mutex);
}

/*===================== 公共辅助函数 =====================*/
/**
 * @brief   创建一个数据卡片(主页温湿度大字体显示)
 * @param   parent        父对象
 * @param   x,y           卡片坐标
 * @param   name          卡片名称(如 "temp"、"humi")
 * @param   unit          单位文本(如 "C"、"%RH")
 * @param   value_color   数值文字颜色
 * @param   out_value_lbl  输出参数,返回数值标签句柄(可为 NULL)
 * @return  创建的卡片对象指针
 *
 * @details 卡片布局(330x200):
 *          - 左上:卡片名称(FONT_CJK,灰色)
 *          - 中间:大字体数值(FONT_BIG,自定义颜色,初始 "--.-")
 *          - 数值右下:单位文本(FONT_CJK,灰色)
 *          样式:圆角 12,无边框,16px 黑色阴影 10% 透明度。
 */
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

    /* 卡片名称(左上角) */
    lv_obj_t *name_lbl = lv_label_create(card);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_font(name_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(name_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 20, 15);

    /* 大字体数值(居中偏左) */
    lv_obj_t *val_lbl = lv_label_create(card);
    lv_label_set_text(val_lbl, "--.-");
    lv_obj_set_style_text_font(val_lbl, FONT_BIG, 0);
    lv_obj_set_style_text_color(val_lbl, value_color, 0);
    lv_obj_align(val_lbl, LV_ALIGN_LEFT_MID, 30, 15);

    /* 单位(贴在数值右下) */
    lv_obj_t *unit_lbl = lv_label_create(card);
    lv_label_set_text(unit_lbl, unit);
    lv_obj_set_style_text_font(unit_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(unit_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 15, 0);

    if(out_value_lbl) *out_value_lbl = val_lbl;
    return card;
}

/**
 * @brief   创建一个通用卡片(用于事件页统计卡片等)
 * @param   parent        父对象
 * @param   x,y           卡片坐标
 * @param   w,h           卡片宽高
 * @param   title         卡片标题
 * @param   bg            卡片背景色
 * @param   title_color   标题文字颜色
 * @param   title_font    标题字体
 * @return  创建的卡片对象指针
 *
 * @details 卡片样式:
 *          - 圆角 10,无边框,8px 黑色阴影 10% 透明度
 *          - 内边距 10px,不可滚动
 *          - 标题位于左上角
 *          返回的卡片对象可在其上继续添加子控件。
 */
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
