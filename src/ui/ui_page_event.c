/**
 * @file    ui_page_event.c
 * @brief   事件页模块:事件表格 + 3 个统计卡片
 *
 * @details 事件页布局:
 *          - 左侧:事件表格(LEFT_W=520 宽,30 行 2 列)
 *            列 0:时间(TIME_COL_W=170)
 *            列 1:事件描述
 *          - 右侧:3 个统计卡片(纵向排列,各 CARD_H=132 高)
 *            卡片1:报警状态 - 今日报警累计
 *            卡片2:命令统计 - 今日命令数 + 最近命令时间
 *            卡片3:系统状态 - 设备状态 + 连接状态
 *
 *          事件写入逻辑:
 *          - 表格行号 even_line 从 1 开始,写满 30 行后回绕到 1;
 *          - 根据事件文本内容(alarm/command/system)分别更新对应统计卡片。
 *
 *          该模块的函数由主线程调用(process_pending_events 或主页更新流程),
 *          无需考虑线程安全。
 */

#include "ui_common.h"
#include "ui_page_event.h"

#define TABLE_ROW_MAX 30   /* 表格最大行数（含表头） */

static lv_obj_t *table      = NULL;   /* 事件表格 */
static lv_obj_t *c1_sub_lbl = NULL;   /* 卡片1 - 报警累计子标签 */
static lv_obj_t *c2_val_lbl  = NULL;   /* 卡片2 - 命令数主标签 */
static lv_obj_t *c2_sub_lbl  = NULL;   /* 卡片2 - 最近命令时间子标签 */
static lv_obj_t *c3_val  = NULL;      /* 卡片3 - 设备状态主标签 */
static lv_obj_t *c3_val_lbl  = NULL;   /* 卡片3 - 连接状态子标签 */



static int alarm_times   = 0;          /* 今日报警累计次数 */
static int command_times = 0;          /* 今日命令累计次数 */
static int even_line     = 1;          /* 下一条事件写入行号(从 1 开始,0 为表头) */
static char recent_time[16] = {0x00};  /* 最近一次命令时间字符串 */

/**
 * @brief   创建事件页
 * @param   parent  父页面容器(pages[PAGE_EVENT])
 *
 * @details 布局:
 *          1. 创建根容器(SCR_W x CONTENT_H),灰色背景;
 *          2. 创建左侧事件表格:
 *             - 2 列:时间(170) + 事件(剩余宽度)
 *             - 30 行(含表头)
 *             - 表头 "time" / "event"
 *             - 样式:白底,深灰文字,底部边框分割
 *          3. 创建右侧 3 个统计卡片(通过 make_card 创建);
 *          4. 在每个卡片内添加子标签显示具体数据。
 *
 * @note    卡片初始显示为 0/默认值,后续由 update_event_page 更新。
 */
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

/**
 * @brief   更新事件表格与统计卡片
 * @param   event  事件文本字符串
 *
 * @details 工作流程:
 *          1. 越界保护:table 为 NULL 直接返回;
 *          2. even_line 回绕(even_line % TABLE_ROW_MAX);
 *          3. 获取当前时间作为表格时间列;
 *          4. 写入表格第 even_line 行;
 *          5. even_line++;
 *          6. 根据事件文本内容更新统计卡片:
 *             - 含 "alarm"  : alarm_times++ 更新卡片1;
 *             - 含 "command": command_times++ 记录时间更新卡片2;
 *             - 含 "system" : 读 connect_event 更新卡片3。
 *
 * 事件文本约定:
 *   - "alarm:..."     : 报警事件
 *   - "command:..."   : 命令事件(来自网络)
 *   - "system:..."    : 系统事件(连接状态变化)
 *
 * @note    该函数在主线程被调用(process_pending_events 或 update_home_data)。
 *          event 字符串会被 strcpy 到表格单元格,长度无显式限制,
 *          调用方应保证长度合理(<100 字符)。
 */
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
        /* device_status 可能由后台线程在推送 alarm 事件时同步翻转,
         * 此处刷新系统状态卡片以反映最新状态 */
        update_system_status();
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

/**
 * @brief   更新系统状态卡片(设备运行状态)
 *
 * @details 根据 device_status 全局变量更新卡片3的主标签:
 *          - ON (正常) : 显示 "normal",绿色
 *          - OFF(报警) : 显示 "alarm",红色
 *
 * @note    在主线程被调用(由 update_event_page 的 alarm 分支触发,
 *          或主页/网络路径显式调用)。读 device_status 时加 status_mutex
 *          保证与后台线程写并发安全。
 */
void update_system_status(void)
{
    pthread_mutex_lock(&status_mutex);
    uint8_t cur_status = device_status;
    pthread_mutex_unlock(&status_mutex);

    lv_label_set_text(c3_val, device_status_str(cur_status));
    lv_obj_set_style_text_color(c3_val, cur_status == ON ? COLOR_OK : COLOR_ALARM, 0);
}
