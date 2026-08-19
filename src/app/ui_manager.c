#include "ui_manager.h"
#include "ui_common.h"
#include "ui_navbar.h"
#include "ui_page_home.h"
#include "ui_page_event.h"
#include "ui_page_history.h"
#include "ui_page_settings.h"
#include "../drivers/hw_ctrl.h"
#include <time.h>

/* 时间标签定时刷新回调（每秒） */
static void time_update_cb(lv_timer_t *timer)
{
    char time_str[32]={0x00};
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    lv_label_set_text(timer_lbl, time_str);
}

/* 初始化时间标签 */
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
void create_ui(void)
{
    pthread_rwlock_init(&th_rwlock, NULL);
    pthread_rwlock_init(&threshold_rwlock, NULL);
    pthread_mutex_init(&cond_mutex, NULL);
    pthread_cond_init(&cond_var, NULL);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_SCR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 顶部导航栏 */
    create_top_bar(scr);

    /* 内容区容器（承载 4 个页面） */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCR_W, CONTENT_H);
    lv_obj_set_pos(content, 0, NAV_H);
    lv_obj_set_style_bg_color(content, COLOR_SCR_BG, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建 4 个全屏页面容器，默认全部隐藏 */
    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        pages[i] = lv_obj_create(content);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], SCR_W, CONTENT_H);
        lv_obj_set_pos(pages[i], 0, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    create_home_page(pages[PAGE_HOME]);
    create_event_page(pages[PAGE_EVENT]);
    create_history_page(pages[PAGE_HISTORY]);
    create_settings_page(pages[PAGE_SETTINGS]);
    timer_init();
    /* 默认显示主页 */
    switch_page(PAGE_HOME);
    lv_timer_create(time_update_cb, 1000, NULL);
}
