#include "ui_common.h"
#include "ui_page_event.h"

#define TABLE_ROW_MAX 30   /* 表格最大行数（含表头） */

static lv_obj_t *table      = NULL;   /* 事件表格 */
static lv_obj_t *c1_sub_lbl = NULL;   /* 卡片1 - 报警累计 */
static lv_obj_t *c2_val_lbl  = NULL;   /* 卡片2 - 命令数 */
static lv_obj_t *c2_sub_lbl  = NULL;   /* 卡片2 - 最近命令时间 */
static lv_obj_t *c3_val  = NULL;   /* 卡片3 - 设备状态 */
static lv_obj_t *c3_val_lbl  = NULL;   /* 卡片3 - 连接状态 */



static int alarm_times   = 0;          /* 今日报警累计 */
static int command_times = 0;          /* 今日命令累计 */
static int even_line     = 1;          /* 下一条事件写入行号 */
static char recent_time[16] = {0x00};  /* 最近一次命令时间字符串 */

void create_event_page(lv_obj_t *parent)
{

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, SCR_W, CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COLOR_SCR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    table = lv_table_create(root);
    lv_obj_set_size(table, LEFT_W, AREA_H);
    lv_obj_set_pos(table, PAD, PAD);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, TABLE_ROW_MAX);
    lv_table_set_col_width(table, 0, TIME_COL_W);
    lv_table_set_col_width(table, 1, LEFT_W - TIME_COL_W);
    /* 表头 */
    lv_table_set_cell_value(table, 0, 0, "time");
    lv_table_set_cell_value(table, 0, 1, "event");
    /* 表格样式 */
    lv_obj_set_style_bg_color(table, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(table, FONT_CJK, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, COLOR_TEXT_DARK, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(table, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);

    /*--------------------------------------------------
     * 右侧：3 个统计卡片（1/3 宽，纵向排列）
     *--------------------------------------------------*/

    /* 卡片1 - 报警状态:今日累计 */
    lv_obj_t *c1 = make_card(root, RIGHT_X, PAD, RIGHT_W, CARD_H,
                             "alarm status", COLOR_CARD_BG, COLOR_TEXT_GRAY, FONT_CJK);

    c1_sub_lbl = lv_label_create(c1);
    lv_label_set_text_fmt(c1_sub_lbl, "today: %d", alarm_times);
    lv_obj_set_style_text_font(c1_sub_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(c1_sub_lbl, COLOR_ALARM, 0);
    lv_obj_align(c1_sub_lbl, LV_ALIGN_LEFT_MID, 0, 6);

    /* 卡片2 - 命令统计：今日命令数 + 最近一次时间 */
    lv_obj_t *c2 = make_card(root, RIGHT_X, PAD + CARD_H + GAP, RIGHT_W, CARD_H,
                             "command statistics", COLOR_CARD_BG, COLOR_TEXT_GRAY, FONT_CJK);

    c2_val_lbl = lv_label_create(c2);
    lv_label_set_text(c2_val_lbl, "today: 0");
    lv_obj_set_style_text_font(c2_val_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(c2_val_lbl, COLOR_OK, 0);
    lv_obj_align(c2_val_lbl, LV_ALIGN_LEFT_MID, 0, 6);
    c2_sub_lbl = lv_label_create(c2);
    lv_label_set_text(c2_sub_lbl, "recent: 00:00:00");
    lv_obj_set_style_text_font(c2_sub_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(c2_sub_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_align(c2_sub_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* 卡片3 - 系统状态：运行状态文字 + 连接说明 */
    lv_obj_t *c3 = make_card(root, RIGHT_X, PAD + (CARD_H + GAP) * 2, RIGHT_W, CARD_H,
                             "system status", COLOR_CARD_BG, COLOR_TEXT_GRAY, FONT_CJK);

    c3_val = lv_label_create(c3);
    lv_label_set_text(c3_val, device_status_str(device_status));
    lv_obj_set_style_text_font(c3_val, FONT_CJK, 0);
    lv_obj_set_style_text_color(c3_val, device_status == ON ? COLOR_OK : COLOR_ALARM, 0);
    lv_obj_align(c3_val, LV_ALIGN_LEFT_MID, 0, 6);

    c3_val_lbl = lv_label_create(c3);
    lv_label_set_text(c3_val_lbl, connect_event_str(connect_event));
    lv_obj_set_style_text_font(c3_val_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(c3_val_lbl, connect_event == ON ? COLOR_OK : COLOR_ALARM, 0);
    lv_obj_align(c3_val_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
}

/* 更新事件表格与统计卡片 */
void update_event_page(char *event)
{
    if(!table) return;
    even_line = even_line % TABLE_ROW_MAX;

    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    lv_table_set_cell_value(table, even_line, 0, time_str);
    lv_table_set_cell_value(table, even_line, 1, event);
    even_line++;
    /* 同步刷新右侧统计卡片*/
    if(strstr(event, "alarm"))
    {
        alarm_times++;
        lv_label_set_text_fmt(c1_sub_lbl, "today: %d", alarm_times);
    }
    /* 命令类事件：调用方需把时间字符串通过 recent_time 传入并 command_times++*/
    if(strstr(event, "command"))
    {
        command_times++;
        strcpy(recent_time, time_str);
        lv_label_set_text_fmt(c2_val_lbl, "today: %d", command_times);
        lv_label_set_text_fmt(c2_sub_lbl, "recent: %s", recent_time);
    }
    if(strstr(event, "system"))
    {
        pthread_mutex_lock(&status_mutex);
        uint8_t cur_connect = connect_event;
        pthread_mutex_unlock(&status_mutex);
        lv_label_set_text(c3_val_lbl, connect_event_str(cur_connect));
        lv_obj_set_style_text_color(c3_val_lbl, cur_connect == ON ? COLOR_OK : COLOR_ALARM, 0);
    }
    
}
/* 更新系统状态卡片 */
void update_system_status(void)
{
    lv_label_set_text(c3_val, device_status_str(device_status));
    lv_obj_set_style_text_color(c3_val, device_status == OFF ? COLOR_OK : COLOR_ALARM, 0);
}