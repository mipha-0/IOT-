/**
 * @file    ui_page_home.h
 * @brief   主页模块对外接口
 *
 * @details 声明主页创建、数值更新与后台刷新线程接口。
 */
#ifndef UI_PAGE_HOME_H
#define UI_PAGE_HOME_H

#include "lvgl/lvgl.h"

/**
 * @brief   创建主页:左侧温湿度卡片 + 右侧曲线图
 * @param   parent  父页面容器
 * @note    创建完成后会启动 mock_data_update 后台线程
 */
void create_home_page(lv_obj_t *parent);

/**
 * @brief   更新主页温湿度数值显示(含报警判定与颜色切换)
 * @param   temp      当前温度
 * @param   humidity  当前湿度
 * @note    会调用 update_system_status/update_event_page,勿在持锁状态调用
 */
void update_home_data(float temp, float humidity);

/**
 * @brief   定时刷新主页温湿度数值与曲线(后台线程)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    监听 cond_var 信号,每 5 秒被唤醒一次
 */
void* mock_data_update(void* arg);
#endif /* UI_PAGE_HOME_H */
