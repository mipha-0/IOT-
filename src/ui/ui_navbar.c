/**
 * @file    ui_navbar.c
 * @brief   顶部导航栏模块
 *
 * @details 创建主屏幕顶部导航栏(高度 NAV_H=50px),包含:
 *          - 左侧:系统标题 "embedded system"
 *          - 右侧:4 个页面切换按钮(Home/Event/History/Settings)
 *
 *          选中按钮高亮(背景色 COLOR_NAV_ACTIVE 完全不透明),
 *          未选中按钮透明,通过 switch_page() 统一管理切换状态。
 *
 *          点击回调 nav_btn_event_cb 通过遍历 nav_btns[] 比对控件指针
 *          确定被点击的页面索引,调用 switch_page() 切换。
 */

#include "ui_navbar.h"
#include "ui_common.h"

/* 导航按钮文字 */
static const char *nav_titles[PAGE_COUNT] =
{
    "Home", "Event", "History", "Settings"
};

/**
 * @brief   导航按钮点击回调
 * @param   e  LVGL 事件对象
 *
 * @details 通过事件目标找到被点击的按钮控件,
 *          遍历 nav_btns[] 比对指针确定索引,
 *          调用 switch_page() 切换到对应页面。
 *
 * @note    使用指针比对而非 user_data,简化逻辑。
 */
static void nav_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        if(nav_btns[i] == btn)
        {
            switch_page(i);
            return;
        }
    }
}

/*====================================================*
 *  顶部导航栏
 *====================================================*/
/**
 * @brief   创建顶部导航栏
 * @param   parent  父对象(通常为屏幕根对象)
 * @return  创建的导航栏对象指针
 *
 * @details 布局:
 *          - 整栏:SCR_W x NAV_H,位于屏幕顶部 (0,0)
 *          - 左侧标题:字体 FONT_CJK,白色,左对齐 padding 15
 *          - 右侧按钮区:4 个 100x36 按钮,从右向左排列
 *          按钮样式:圆角 6,无边框无阴影,
 *          默认透明(LV_OPA_TRANSP),背景色设为 COLOR_NAV_ACTIVE
 *          (通过透明度实现高亮/未高亮效果)。
 *          将按钮句柄存入 nav_btns[] 供 switch_page() 使用。
 */
lv_obj_t *create_top_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, SCR_W, NAV_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧系统标题 */
    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "embedded system");
    lv_obj_set_style_text_font(title, FONT_CJK, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    /* 右侧 4 个导航按钮(从右向左排列) */
    lv_coord_t btn_w = 100;
    lv_coord_t btn_h = 36;
    lv_coord_t start_x = SCR_W - 10 - (btn_w * PAGE_COUNT);
    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, start_x + i * btn_w, (NAV_H - btn_h) / 2);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);   /* 默认透明 */
        lv_obj_set_style_bg_color(btn, COLOR_NAV_ACTIVE, 0);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, nav_titles[i]);
        lv_obj_set_style_text_font(lbl, FONT_CJK, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED, NULL);
        nav_btns[i] = btn;
    }

    return bar;
}

/*====================================================*
 *  页面切换
 *====================================================*/
/**
 * @brief   切换到指定页面
 * @param   page_id  目标页面 ID(PAGE_HOME/EVENT/HISTORY/SETTINGS)
 *
 * @details 工作流程:
 *          1. 越界检查:page_id >= PAGE_COUNT 直接返回;
 *          2. 已在当前页则跳过(避免重复刷新);
 *          3. 更新 cur_page;
 *          4. 遍历所有页面容器:
 *             - 目标页:清除 HIDDEN 标志(显示)
 *             - 其他页:添加 HIDDEN 标志(隐藏)
 *          5. 同步更新导航按钮高亮状态:
 *             - 选中按钮:LV_OPA_COVER(完全不透明)
 *             - 未选中按钮:LV_OPA_TRANSP(透明)
 *
 * @note    该函数应在主线程调用(UI 回调或 lv_timer 上下文)。
 */
void switch_page(uint8_t page_id)
{
    if(page_id >= PAGE_COUNT) return;
    if(page_id == cur_page) return;   /* 已在当前页,无需重复切换 */
    cur_page = page_id;

    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        bool active = (i == page_id);
        if(active) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        /* 选中按钮高亮,未选中透明 */
        lv_obj_set_style_bg_opa(nav_btns[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}
