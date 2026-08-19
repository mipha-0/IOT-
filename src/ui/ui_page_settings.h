/**
 * @file    ui_page_settings.h
 * @brief   设置页模块对外接口
 *
 * @details 声明设置页创建、配置文件读写、阈值显示接口,
 *          以及全局共享的阈值变量与配置保存状态。
 */
#ifndef UI_PAGE_SETTINGS_H
#define UI_PAGE_SETTINGS_H

#include "lvgl/lvgl.h"

/**
 * @brief   创建设置页:MQTT 服务器/端口/设备ID 配置 + 切换/保存按钮
 * @param   parent  父页面容器
 * @note    创建时会自动调用 read_config/threshold_config 加载持久化配置
 */
void create_settings_page(lv_obj_t *parent);

/**
 * @brief   保存 MQTT 配置到 ./config.txt
 * @details 文件格式: port:端口 / id:设备ID / ip:服务器地址
 */
void write_config(void);

/**
 * @brief   从 ./config.txt 读取 MQTT 配置
 * @details 文件不存在时使用代码内默认值
 */
void read_config(void);

/**
 * @brief   保存阈值配置到 ./threshold.txt
 * @details 文件格式: temp:温度阈值 / humi:湿度阈值
 */
void write_threshold_config(void);

/**
 * @brief   显示阈值(刷新设置页阈值相关控件)
 * @note    主线程调用,由按钮回调或 process_pending_events 触发
 */
void display_threshold(void);



/* 阈值配置(全局共享,在 ui_page_settings.c 中定义) */
extern float temp_threshold;     /* 温度阈值,默认 35.0°C */
extern float humi_threshold;     /* 湿度阈值,默认 50.0% */
extern bool cfg_saved;           /* 配置保存状态: true=已保存,false=未保存 */

#endif /* UI_PAGE_SETTINGS_H */
