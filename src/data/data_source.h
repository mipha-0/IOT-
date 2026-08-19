#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include "lvgl/lvgl.h"

/* 后台线程：每秒生成温湿度写入共享变量 */
void* get_temp_humi(void* arg);

/* 后台线程：每 5 秒把当前温湿度+时间写入 ./data.txt */
void* write_data_to_file(void* arg);

#endif /* DATA_SOURCE_H */
