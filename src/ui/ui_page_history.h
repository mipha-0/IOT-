/**
 * @file    ui_page_history.h
 * @brief   历史数据页模块对外接口
 *
 * @details 声明历史页创建、数据读取与显示接口。
 */
#ifndef UI_PAGE_HISTORY_H
#define UI_PAGE_HISTORY_H

#include "lvgl/lvgl.h"

/**
 * @brief   创建历史数据页(卡片 + 读取 data.txt 填表)
 * @param   parent  父页面容器
 * @note    创建完成后会启动 update_history_data 后台线程
 */
void create_history_page(lv_obj_t *parent);

/**
 * @brief   将历史数据写入指定卡片(读取 ./data.txt 最近 20 条,最新在顶部)
 * @param   card  历史页主卡片对象
 * @note    文件不存在时显示 "no history data" 提示
 */
void write_history_data(lv_obj_t *card);

/**
 * @brief   更新历史数据卡片(显示指定行数据)
 * @param   card  历史页主卡片对象(参数保留,实际使用全局 card)
 * @param   i     行号(0~19)
 * @param   lines 该行数据字符串
 * @note    主线程调用,由 process_pending_events 触发
 */
void update_history_card(lv_obj_t *card, int i, char *lines);


#endif /* UI_PAGE_HISTORY_H */
