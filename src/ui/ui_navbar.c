#include "ui_navbar.h"
#include "ui_common.h"

/* 导航按钮文字 */
static const char *nav_titles[PAGE_COUNT] =
{
    "Home", "Event", "History", "Settings"
};

/* 导航按钮点击回调：找出被点击的按钮并切换页面 */
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

    /* 右侧 4 个导航按钮 */
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
void switch_page(uint8_t page_id)
{
    if(page_id >= PAGE_COUNT) return;
    if(page_id == cur_page) return;   /* 已在当前页，无需重复切换 */
    cur_page = page_id;

    for(uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        bool active = (i == page_id);
        if(active) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        /* 选中按钮高亮，未选中透明 */
        lv_obj_set_style_bg_opa(nav_btns[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}
