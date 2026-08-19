/**
 * @file    ui_page_home.h
 * @brief   主页模块对外接口
 *
 * @details 声明主页创建、数值更新与后台刷新线程接口。
 */
#ifndef UI_PAGE_HOME_H
#define UI_PAGE_HOME_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/**
 * @brief   创建主页:左侧温湿度卡片 + 右侧曲线图
 * @param   parent  父页面容器
 * @note    创建完成后会启动 mock_data_update 后台线程
 */
void create_home_page(lv_obj_t *parent);

/**
 * @brief   后台线程调用:阈值判定 + 状态翻转检测 + 投递主页刷新请求
 * @param   temp      当前温度
 * @param   humidity  当前湿度
 * @note    线程安全,不调用任何 LVGL API;UI 刷新通过 request_home_refresh 投递
 */
void update_home_data(float temp, float humidity);

/**
 * @brief   主线程调用:更新主页温湿度数值显示与颜色(纯 LVGL 调用)
 * @param   temp        当前温度
 * @param   humi        当前湿度
 * @param   temp_alarm  温度是否超阈值(决定数值颜色)
 * @param   humi_alarm  湿度是否超阈值(决定数值颜色)
 * @note    仅在主线程 process_pending_events 中调用,保证 LVGL 线程安全
 */
extern void update_home_ui(float temp, float humi, bool temp_alarm, bool humi_alarm);

/**
 * @brief   定时刷新主页温湿度数值与曲线(后台线程)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    监听 cond_var 信号,每 5 秒被唤醒一次
 */
void* mock_data_update(void* arg);
#endif /* UI_PAGE_HOME_H */
