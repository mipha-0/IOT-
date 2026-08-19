#include "ui_page_history.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

static lv_obj_t *tbl = NULL;
static uint16_t row_cnt = 0;
static lv_obj_t *card = NULL;
static u_int8_t lien = 0;
/*====================================================*
 *  历史数据页
 *====================================================*/
extern void* update_history_data(void*arg);

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

    write_history_data(card);
    pthread_t th_history;
    pthread_create(&th_history, NULL, update_history_data, NULL);
}

/*写入历史数据（最近20条）*/
void write_history_data(lv_obj_t *card)
{
    FILE *fp_hist = fopen("./data.txt", "r");
    int  hand = 0;
    if(fp_hist == NULL)
    {
        lv_obj_t *tip = lv_label_create(card);
        lv_label_set_text(tip, "no history data");
        lv_obj_set_style_text_font(tip, FONT_CJK, 0);
        lv_obj_set_style_text_color(tip, COLOR_TEXT_GRAY, 0);
        lv_obj_align(tip, LV_ALIGN_CENTER, 0, 20);
        return;
    }
    /* 读文件 */
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
    /* 填充表格数据 */
    for(; lien < row_cnt; lien++)
    {
        if(lines[(hand + lien) % 20][0]) lv_table_set_cell_value_fmt(tbl, lien, 0, "%d: %s", lien + 1, lines[(hand + lien) % 20]);
        else                          lv_table_set_cell_value(tbl, lien, 0, "-");
    }

    lv_obj_set_style_text_font(tbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(tbl, COLOR_TEXT_GRAY, 0);
}/*更新历史数据(实时数据更新)*/
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

            /* 填充表格数据 */
            if(lines[i][0])               request_history_refresh(i, lines[i]);
            else                          request_history_refresh(i, "-");
        }
    }
    return NULL;
}
void display_history(int i, char *lines)
{

    if(tbl == NULL) write_history_data(card);
    lv_table_set_cell_value_fmt(tbl, i, 0, "%d: %s", i + 1, lines);
    return;
}