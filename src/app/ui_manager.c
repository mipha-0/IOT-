/**
 * @file    ui_manager.c
 * @brief   UI 框架初始化模块
 *
 * @details 负责创建整个应用的可视框架:
 *          1. 初始化所有跨线程同步原语(读写锁/互斥锁/条件变量);
 *          2. 设置屏幕背景;
 *          3. 创建顶部导航栏(4 个页面切换按钮);
 *          4. 创建内容区容器,内含 4 个全屏页面容器(默认全部隐藏);
 *          5. 调用各页面构建函数填充内容;
 *          6. 创建右下角时间标签,并启动 1s 周期定时器刷新时间。
 *
 *          依赖关系:
 *          - 调用 ui_navbar::create_top_bar() 创建导航栏
 *          - 调用 ui_page_*::create_*_page() 创建各页面
 *          - 共享状态定义于 ui_common.c
 */

#include "ui_manager.h"
#include "ui_common.h"
#include "ui_navbar.h"
#include "ui_page_home.h"
#include "ui_page_event.h"
#include "ui_page_history.h"
#include "ui_page_settings.h"
#include "../drivers/hw_ctrl.h"
#include <time.h>

/**
 * @brief  时间标签定时刷新回调(每秒由 lv_timer 调用)
 * @param  timer  LVGL 定时器句柄(未使用,仅满足回调签名)
 *
 * @details 读取系统时间,格式化为 "YYYY-MM-DD HH:MM:SS" 字符串,
 *          更新 timer_lbl 控件显示。运行于主线程,无需加锁。
 */
static void time_update_cb(lv_timer_t *timer)
{
    char time_str[32]={0x00};
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    lv_label_set_text(timer_lbl, time_str);
}

/**
 * @brief  初始化时间标签
 *
 * @details 在主屏幕上创建时间标签,初始显示当前时间,
 *          字体使用 FONT_CJK(支持中英文),颜色为灰色,
 *          定位到屏幕右下角 (350, 450)。
 *          后续由 time_update_cb 每秒刷新。
 */
static void timer_init(void)
{
    timer_lbl = lv_label_create(lv_scr_act());
    time_t t = time(NULL);
    char time_str[32]={0x00};
    struct tm *tm = NULL;
    tm = localtime(&t);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    lv_label_set_text(timer_lbl, time_str);
    lv_obj_set_pos(timer_lbl, 350, 450);
    lv_obj_set_style_text_font(timer_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(timer_lbl, COLOR_TEXT_GRAY, 0);
}

/*====================================================*
 *  对外接口
 *====================================================*/
/**
 * @brief  创建整个界面:导航栏 + 4 个页面,默认显示主页
 *
 * @details 执行步骤:
 *          1. 初始化跨线程同步原语:
 *             - th_rwlock       : 保护 mock_temp/mock_humi
 *             - threshold_rwlock: 保护 temp_threshold/humi_threshold
 *             - cond_mutex+cond_var: 数据更新条件变量
 *          2. 设置屏幕背景色;
 *          3. 创建顶部导航栏;
 *          4. 创建内容区容器(SCR_W x CONTENT_H),位于导航栏下方;
 *          5. 在容器内创建 4 个全屏页面容器,默认全部隐藏;
 *          6. 调用 4 个页面的构建函数填充内容;
 *          7. 创建时间标签并启动 1s 定时器;
 *          8. 调用 switch_page(PAGE_HOME) 显示主页。
 *
 * @note   必须在所有后台线程启动前调用(main.c 中已保证),
 *         否则后台线程访问未初始化的锁将导致未定义行为。
 */
void create_ui(void)
{
    /* 初始化跨线程同步原语(必须在后台线程启动前完成) */
    pthread_rwlock_init(&th_rwlock, NULL);
    pthread_rwlock_init(&threshold_rwlock, NULL);
    pthread_rwlock_init(&history_rwlock, NULL);
    pthread_mutex_init(&cond_mutex, NULL);
    pthread_cond_init(&cond_var, NULL);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_SCR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 顶部导航栏(4 个页面切换按钮) */
    create_top_bar(scr);

    /* 内容区容器(承载 4 个页面,位于导航栏下方) */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCR_W, CONTENT_H);
    lv_obj_set_pos(content, 0, NAV_H);
    lv_obj_set_style_bg_color(content, COLOR_SCR_BG, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建 4 个全屏页面容器,默认全部隐藏 */
    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        pages[i] = lv_obj_create(content);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], SCR_W, CONTENT_H);
        lv_obj_set_pos(pages[i], 0, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* 分别构建 4 个页面的具体内容 */
    create_home_page(pages[PAGE_HOME]);
    create_event_page(pages[PAGE_EVENT]);
    create_history_page(pages[PAGE_HISTORY]);
    create_settings_page(pages[PAGE_SETTINGS]);
    timer_init();
    /* 默认显示主页 */
    switch_page(PAGE_HOME);
    /* 启动 1s 周期定时器刷新时间标签 */
    lv_timer_create(time_update_cb, 1000, NULL);
}
