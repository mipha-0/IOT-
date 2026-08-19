/**
 * @file    ui_page_home.c
 * @brief   主页模块:温湿度卡片 + 实时曲线图
 *
 * @details 主页布局:
 *          - 左侧上:温度大字体卡片(330x200)
 *          - 左侧下:湿度大字体卡片(330x200)
 *          - 右侧 :温湿度曲线图卡片(446x414)
 *
 *          曲线图:
 *          - 30 个数据点(CHART_POINTS),平移模式(LV_CHART_UPDATE_MODE_SHIFT)
 *          - 温度用左 Y 轴(0~50),湿度用右 Y 轴(0~100)
 *          - 5x5 网格分割线
 *
 *          数据更新链:
 *          get_temp_humi(线程) → cond_var → mock_data_update(线程)
 *          → update_home_data() + lv_chart_set_next_value()
 *
 *          报警逻辑:
 *          update_home_data 内根据阈值判断温度/湿度是否超限,
 *          异常态每次采集均推送 alarm 事件 + device_status=OFF,数值变红;
 *          恢复正常时不报事件,仅刷新系统状态卡片显示 + device_status=ON,
 *          数值变绿。严格区分异常上报与状态更新。
 */

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
/**
 * @brief   创建主页
 * @param   parent  父页面容器(pages[PAGE_HOME])
 *
 * @details 工作流程:
 *          1. 创建温度卡片(8,8),输出句柄存入 home_temp_lbl;
 *          2. 创建湿度卡片(8,218),输出句柄存入 home_humi_lbl;
 *          3. 创建曲线图卡片(346,8,446x414):
 *             - 标题 "temp & humi curve"
 *             - chart 控件 426x360
 *             - 温度左轴 0~50,湿度右轴 0~100
 *             - 5x5 网格线,平移模式
 *             - 添加温度序列(红)与湿度序列(蓝)
 *          4. 预填充曲线避免初始空白(用 mock_temp/mock_humi 填 30 个点);
 *          5. 调用 update_home_data 显示初始数值;
 *          6. 启动 mock_data_update 后台线程监听新数据。
 *
 * @note    曲线图初始化时使用共享变量初值(23.5/40.0),
 *          实际数据由后台线程后续更新。
 */
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

    /* 启动后台线程:监听新数据并刷新主页温湿度数值与曲线 */
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, mock_data_update, NULL);
}

/**
 * @brief   后台线程调用:阈值判定 + 异常上报 + 状态刷新请求
 * @param   temp      当前温度
 * @param   humidity  当前湿度
 *
 * @details 工作流程(全部在后台线程完成,不调用任何 LVGL API):
 *          1. 读取阈值(temp_threshold / humi_threshold,读锁);
 *          2. 判定 temp_alarm / humi_alarm;
 *          3. 计算新 device_status = (任一超限) ? OFF : ON;
 *          4. status_mutex 保护下比较并更新 device_status;
 *          5. 上报规则(异常上报 vs 状态更新严格分离):
 *             - 异常态(temp/humi 任一超限):每次数据采集均通过
 *               push_pending_event 推送 "alarm:..." 事件,主线程消费时
 *               会写入事件表格 + 刷新统计卡片 + update_system_status();
 *             - 恢复正常(翻转 OFF→ON):不上报事件,仅通过
 *               request_system_status_refresh() 请求主线程刷新系统状态
 *               卡片显示文字,保持界面与实际运行状态一致。
 *          6. request_home_refresh 投递主页 UI 刷新请求(覆盖式)。
 *
 * @note    本函数线程安全,可在后台线程直接调用。
 *          LVGL 调用全部由主线程 process_pending_events 完成。
 */
void update_home_data(float temp, float humidity)
{
    bool temp_alarm = false;
    bool humi_alarm = false;

    pthread_rwlock_rdlock(&threshold_rwlock);
    float temp_thr = temp_threshold;
    float humi_thr  = humi_threshold;
    pthread_rwlock_unlock(&threshold_rwlock);

    temp_alarm = (temp >= temp_thr);
    humi_alarm = (humidity >= humi_thr);

    uint8_t new_status = (temp_alarm || humi_alarm) ? OFF : ON;
    bool recovered = false;

    pthread_mutex_lock(&status_mutex);
    if(device_status != new_status)
    {
        device_status = new_status;
        /* 在报警恢复正常时记录:需要刷新系统状态卡片 */
        recovered = (new_status == ON);
    }
    pthread_mutex_unlock(&status_mutex);

    if(temp_alarm || humi_alarm)
    {
        /* 异常态:每次采集超限均上报一次报警事件 */
        char event[128] = {0};
        if(temp_alarm)
            snprintf(event, sizeof(event),
                     "alarm:Temp alert triggered, temp: %.1f", temp);
        else
            snprintf(event, sizeof(event),
                     "alarm:Humi alert triggered, humi: %.1f", humidity);
        printf("%s\n", event);
        push_pending_event(event);
    }
    else if(recovered)
    {
        /* 请求刷新系统状态卡片显示文字 */
        request_system_status_refresh();
    }

    request_home_refresh(temp, humidity, temp_alarm, humi_alarm);
}

/**
 * @brief   主线程调用:更新主页温湿度数值显示与颜色(纯 LVGL 调用)
 * @param   temp        当前温度
 * @param   humi        当前湿度
 * @param   temp_alarm  温度是否超阈值(红)/ 否则(绿)
 * @param   humi_alarm  湿度是否超阈值(红)/ 否则(绿)
 *
 * @note    仅在主线程 process_pending_events 中调用,保证 LVGL 线程安全。
 */
void update_home_ui(float temp, float humi, bool temp_alarm, bool humi_alarm)
{
    if(home_temp_lbl)
    {
        lv_label_set_text_fmt(home_temp_lbl, "%.1f", temp);
        lv_obj_set_style_text_color(home_temp_lbl,
            temp_alarm ? COLOR_TEXT_RED : COLOR_TEXT_GREEN, 0);
    }
    if(home_humi_lbl)
    {
        lv_label_set_text_fmt(home_humi_lbl, "%.1f", humi);
        lv_obj_set_style_text_color(home_humi_lbl,
            humi_alarm ? COLOR_TEXT_RED : COLOR_TEXT_GREEN, 0);
    }
}

/**
 * @brief   定时刷新主页温湿度数值与曲线(后台线程)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 阻塞等待 cond_var 信号(get_temp_humi 发出);
 *          2. 读共享变量 mock_temp/mock_humi(读锁);
 *          3. 调用 update_home_data() 更新数值与报警状态;
 *          4. 调用 lv_chart_set_next_value() 追加曲线点。
 *  
 */
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
