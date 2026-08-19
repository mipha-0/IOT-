/**
 * @file    data_source.h
 * @brief   数据采集与持久化模块对外接口
 *
 * @details 声明两个后台线程入口:
 *          - get_temp_humi     : 周期拉取天气数据写入共享变量
 *          - write_data_to_file: 监听新数据并追加写入 ./data.txt
 */
#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include "lvgl/lvgl.h"

/**
 * @brief   后台线程:每 5 秒通过 HTTP 拉取天气数据写入共享变量
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    数据更新后通过 cond_var 通知其他等待线程
 */
void* get_temp_humi(void* arg);

/**
 * @brief   后台线程:监听新数据信号,将当前温湿度+时间追加写入 ./data.txt
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    事件驱动,只在收到 cond_var 信号时执行写入
 */
void* write_data_to_file(void* arg);

#endif /* DATA_SOURCE_H */
