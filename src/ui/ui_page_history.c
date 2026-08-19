/**
 * @file    ui_page_history.c
 * @brief   历史数据页模块:从 data.txt 读取并展示最近 20 条记录
 *
 * @details 历史页布局:
 *          - 单个卡片(card_w x card_h),内含表格
 *          - 表格 1 列,行数等于实际读到的记录数(最多 20)
 *          - 每行格式: "序号: 时间,温度,湿度"
 *
 *          数据来源:
 *          - 启动时:write_history_data() 读取 ./data.txt 全部内容,
 *            通过环形缓冲保留最近 20 条,填充表格;
 *          - 运行时:update_history_data() 后台线程监听 cond_var,
 *            收到新数据时通过 request_history_refresh 推到主线程,
 *            由 process_pending_events -> display_history 刷新表格对应行。
 *
 *          环形缓冲算法:
 *          读文件时使用 lines[i % 20] 循环写入,
 *          hand 记录最旧记录的偏移,最终按 hand→hand+19 顺序显示。
 *
 * @note    文件不存在时显示 "no history data" 提示。
 *          lien 为全局变量,记录已填充行数(初始为 0)。
 */

#include "ui_page_history.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

static lv_obj_t *tbl = NULL;          /* 历史数据表格 */
static uint16_t row_cnt = 0;          /* 表格实际行数 */
static lv_obj_t *card = NULL;         /* 历史页主卡片 */
static u_int8_t lien = 0;             /* 已填充的表格行数(全局,跨函数共享) */

/*====================================================*
 *  历史数据页
 *====================================================*/
extern void* update_history_data(void*arg);

/**
 * @brief   创建历史数据页
 * @param   parent  父页面容器(pages[PAGE_HISTORY])
 *
 * @details 工作流程:
 *          1. 创建主卡片(card_w x card_h,白色圆角阴影);
 *          2. 添加标题 "history data";
 *          3. 调用 write_history_data(card) 从 ./data.txt 读取并填充表格;
 *          4. 启动 update_history_data 后台线程监听新数据。
 *
 * @note    若 data.txt 不存在,write_history_data 会显示提示文本,
 *          但后台线程仍会启动,后续有新数据时通过 display_history 重建表格。
 */
void create_history_page(lv_obj_t *parent)
{

    card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, card_w, card_h);
    lv_obj_set_pos(card, card_x, card_y);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 卡片标题 */
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "history data");
    lv_obj_set_style_text_font(title, FONT_CJK, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_DARK, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 从 ./data.txt 读取并填充表格 */
    write_history_data(card);
    /* 启动后台线程监听新数据 */
    pthread_t th_history;
    pthread_create(&th_history, NULL, update_history_data, NULL);
}

/**
 * @brief   写入历史数据(读取 ./data.txt 最近 20 条)
 * @param   card  历史页主卡片对象
 *
 * @details 工作流程:
 *          1. 打开 ./data.txt 只读,失败则在卡片上显示提示并返回;
 *          2. 逐行读取到 lines[i % 20] 环形缓冲(最多保留 20 条);
 *          3. 去除每行末尾 '\n';
 *          4. 计算 hand(最旧记录偏移)与 row_cnt(实际行数);
 *          5. 创建表格(1 列,row_cnt 行);
 *          6. 从 lien 开始填充表格,按时间顺序(旧→新)排列。
 *
 * 环形缓冲说明:
 *   假设文件有 25 条记录,则 lines[0..4] 保存第 21~25 条(覆盖旧值),
 *   hand=5 表示最旧记录在 lines[5],按 5,6,...,4,0,1,2,3,4 顺序显示。
 *
 * @note    表格宽度 SCR_W-36,高度 CONTENT_H-40,底部居中对齐。
 *          lien 是全局变量,记录已填充行数,供 update_history_data 续填。
 */
void write_history_data(lv_obj_t *card)
{
    FILE *fp_hist = fopen("./data.txt", "r");
    int  hand = 0;
    if(fp_hist == NULL)
    {
        /* 文件不存在:显示提示文本 */
        lv_obj_t *tip = lv_label_create(card);
        lv_label_set_text(tip, "no history data");
        lv_obj_set_style_text_font(tip, FONT_CJK, 0);
        lv_obj_set_style_text_color(tip, COLOR_TEXT_GRAY, 0);
        lv_obj_align(tip, LV_ALIGN_CENTER, 0, 20);
        return;
    }
    /* 读文件(环形缓冲,保留最近 20 条) */
    char lines[20][128];
    row_cnt = 0;

    for(uint16_t i = 0; ; i++)
    {
        if(fgets(lines[i % 20], sizeof(lines[i % 20]), fp_hist) == NULL) break;

        if(strlen(lines[i % 20]) > 0 && (lines[i % 20][strlen(lines[i % 20]) - 1] == '\n'))
            lines[i % 20][strlen(lines[i % 20]) - 1] = '\0';
        if(row_cnt < 20) row_cnt++;
        if(i > 19) hand = (hand + 1) % 20;
    }
    fclose(fp_hist);

    /* 创建实际显示行数表格 */
    if(row_cnt == 0) row_cnt = 1;
    tbl = lv_table_create(card);
    lv_obj_set_size(tbl, SCR_W - 36, CONTENT_H - 40);
    lv_obj_align(tbl, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_table_set_col_cnt(tbl, 1);
    lv_table_set_row_cnt(tbl, row_cnt);
    lv_table_set_col_width(tbl, 0, SCR_W - 36);
    /* 填充表格数据(从 lien 开始,按时间顺序) */
    for(; lien < row_cnt; lien++)
    {
        if(lines[(hand + lien) % 20][0]) lv_table_set_cell_value_fmt(tbl, lien, 0, "%d: %s", lien + 1, lines[(hand + lien) % 20]);
        else                          lv_table_set_cell_value(tbl, lien, 0, "-");
    }

    lv_obj_set_style_text_font(tbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(tbl, COLOR_TEXT_GRAY, 0);
}

/**
 * @brief   更新历史数据(后台线程,实时数据更新)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 从 lien 开始环形写入 lines[i % 20];
 *          2. 阻塞等待 cond_var 信号(get_temp_humi 发出);
 *          3. 读共享变量 mock_temp/mock_humi;
 *          4. 格式化 "时间,温度,湿度" 字符串;
 *          5. 通过 request_history_refresh 推到主线程刷新表格。
 *
 * @note    本线程不直接调 LVGL API,通过 pending 队列中转。
 *          内层 for 循环条件 i=(i+1)%20 在数据未满 20 条时
 *          会持续递增行号,实际刷新由主线程按行号定位。
 *          去除 '\n' 的逻辑与 write_history_data 一致。
 */
void* update_history_data(void*arg)
{
    pthread_detach(pthread_self());
    char lines[20][128];
    char time_str[32]={0x00};
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    while(1)
    {
        memset(lines, 0, sizeof(lines));
        for(uint16_t i = lien; ; i=(i+1)%20)
        {
            pthread_mutex_lock(&cond_mutex);
            pthread_cond_wait(&cond_var, &cond_mutex);
            pthread_mutex_unlock(&cond_mutex);

            pthread_rwlock_rdlock(&th_rwlock);
            float temp = mock_temp;
            float humidity = mock_humi;
            pthread_rwlock_unlock(&th_rwlock);

            t = time(NULL);
            tm = localtime(&t);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
            sprintf(lines[i ], "%s,%.1f,%.1f\n", time_str, temp, humidity);

            if(strlen(lines[i ]) > 0 && (lines[i][strlen(lines[i]) - 1] == '\n'))
            lines[i][strlen(lines[i]) - 1] = '\0';

            /* 填充表格数据(通过 pending 队列交给主线程) */
            if(lines[i][0])               request_history_refresh(i, lines[i]);
            else                          request_history_refresh(i, "-");
        }
    }
    return NULL;
}

/**
 * @brief   显示历史数据(主线程调用,由 process_pending_events 触发)
 * @param   i     数据行号(0~19)
 * @param   lines 该行的数据字符串
 *
 * @details 若表格尚未创建(tbl==NULL,即初始 data.txt 不存在),
 *          先调用 write_history_data 重建表格。
 *          然后通过 lv_table_set_cell_value_fmt 更新指定行。
 *
 * @note    本函数在主线程被调用,无需加锁。
 *          行号 i 与 update_history_data 中的环形索引一致。
 */
void display_history(int i, char *lines)
{

    if(tbl == NULL) write_history_data(card);
    lv_table_set_cell_value_fmt(tbl, i, 0, "%d: %s", i + 1, lines);
    return;
}
