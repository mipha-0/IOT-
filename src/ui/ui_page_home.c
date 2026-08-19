#include "ui_page_home.h"
#include "ui_common.h"
#include "ui_page_settings.h"
#include "ui_page_event.h"
#include "../drivers/hw_ctrl.h"
#include <time.h>
#include <stdio.h>

/*====================================================*
 *  主页：左侧 温度/湿度 大字体卡片 + 右侧 温湿度曲线图
 *====================================================*/
void create_home_page(lv_obj_t *parent)
{
    /* 左侧：温度卡片（上） */
    create_data_card(parent, 8, 8, "temp", "C", COLOR_TEMP, &home_temp_lbl);

    /* 左侧：湿度卡片（下） */
    create_data_card(parent, 8, 218, "humi", "%RH", COLOR_HUMI, &home_humi_lbl);

    /* 右侧：曲线图卡片 */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 446, 414);
    lv_obj_set_pos(card, 346, 8);
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
    lv_label_set_text(title, "temp & humi curve");
    lv_obj_set_style_text_font(title, FONT_CJK, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_DARK, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 曲线图 */
    home_chart = lv_chart_create(card);
    lv_obj_set_size(home_chart, 426, 360);
    lv_obj_align(home_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(home_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(home_chart, CHART_POINTS);
    /* 温度用左轴 0~50，湿度用右轴 0~100 */
    lv_chart_set_range(home_chart, LV_CHART_AXIS_PRIMARY_Y,   0, 50);
    lv_chart_set_range(home_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
    lv_chart_set_div_line_count(home_chart, 5, 5);
    lv_chart_set_update_mode(home_chart, LV_CHART_UPDATE_MODE_SHIFT);   /* 平移：新数据追加，旧数据左移 */
    lv_obj_set_style_border_width(home_chart, 0, 0);
    lv_obj_set_style_radius(home_chart, 0, 0);

    ser_temp = lv_chart_add_series(home_chart, COLOR_TEMP, LV_CHART_AXIS_PRIMARY_Y);
    ser_humi = lv_chart_add_series(home_chart, COLOR_HUMI, LV_CHART_AXIS_SECONDARY_Y);

    /* 预填充曲线，避免初始空白 */
    for(uint16_t i = 0; i < CHART_POINTS; i++)
    {
        lv_chart_set_next_value(home_chart, ser_temp, (lv_coord_t)mock_temp);
        lv_chart_set_next_value(home_chart, ser_humi, (lv_coord_t)mock_humi);
    }

    /* 初始数值 */
    update_home_data(mock_temp, mock_humi);
    
    /* 刷新主页温湿度数值与曲线 */
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, mock_data_update, NULL);
}

/* 更新主页温湿度数值 */
void update_home_data(float temp, float humidity)
{

    if(home_temp_lbl)
    {
        pthread_rwlock_rdlock(&threshold_rwlock);
        float threshold = temp_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);
        lv_label_set_text_fmt(home_temp_lbl, "%.1f", temp);
        if(temp > threshold)
        {

            char event[128]={0};
            sprintf(event, "alarm:Temp alert triggered, temp: %.1f", temp);
            printf("%s\n", event);
            device_status = OFF;
            update_system_status();
            update_event_page(event);
            lv_obj_set_style_text_color(home_temp_lbl, COLOR_TEXT_RED, 0);
        }
        else
        {
            if(device_status == OFF)
            {
            device_status = ON;
            update_system_status();
            }
            lv_obj_set_style_text_color(home_temp_lbl, COLOR_TEXT_GREEN, 0);
        }
    }
    if(home_humi_lbl)
    {
        pthread_rwlock_rdlock(&threshold_rwlock);
        float threshold = humi_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);
        lv_label_set_text_fmt(home_humi_lbl, "%.1f", humidity);
        if(humidity > threshold)
        {
            char event[128]={0};
            sprintf(event, "alarm:Humi alert triggered, humi: %.1f", humidity);
            printf("%s\n", event);
            device_status = OFF;
            update_system_status();
            update_event_page(event);
            lv_obj_set_style_text_color(home_humi_lbl, COLOR_TEXT_RED, 0);
        }
        else
        {
            if(device_status == OFF)
            {
            device_status = ON;
            update_system_status();
            }
            lv_obj_set_style_text_color(home_humi_lbl, COLOR_TEXT_GREEN, 0);
            
        }
    }
}
/*定时刷新主页温湿度数值与曲线*/
void* mock_data_update(void* arg)
{
    pthread_detach(pthread_self());
    while(1)
    {
        /* 等待 get_temp_humi 发出的新数据信号 */
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_wait(&cond_var, &cond_mutex);
        pthread_mutex_unlock(&cond_mutex);

        pthread_rwlock_rdlock(&th_rwlock);
        float temp = mock_temp;
        float humidity = mock_humi;
        pthread_rwlock_unlock(&th_rwlock);
        update_home_data(temp, humidity);
        /* 推入曲线图 */
        if(home_chart && ser_temp && ser_humi)
        {
            lv_chart_set_next_value(home_chart, ser_temp, (lv_coord_t)temp);
            lv_chart_set_next_value(home_chart, ser_humi, (lv_coord_t)humidity);
        }
    }   
}
