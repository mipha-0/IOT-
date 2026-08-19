#ifndef UI_PAGE_SETTINGS_H
#define UI_PAGE_SETTINGS_H

#include "lvgl/lvgl.h"

/* 创建设置页：MQTT 服务器/端口/设备ID 配置 + 切换/保存按钮 */
void create_settings_page(lv_obj_t *parent);

/* 保存配置到 ./config.txt */
void write_config(void);

/* 从 ./config.txt 读取配置 */
void read_config(void);

/* 保存阈值配置到 ./threshold.txt */
void write_threshold_config(void);

/* 显示阈值 */
void display_threshold(void);



/* 阈值配置 */
extern float temp_threshold;
extern float humi_threshold;
extern bool cfg_saved;

#endif /* UI_PAGE_SETTINGS_H */
