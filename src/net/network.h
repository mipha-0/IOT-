/**
 * @file    network.h
 * @brief   网络通信模块对外接口
 *
 * @details 声明 TCP 客户端线程入口与 HTTP 天气拉取接口。
 *          详细行为见 network.c 文件头说明。
 */
#ifndef NETWORK_H
#define NETWORK_H
#include "ui_common.h"

/**
 * @brief   TCP 客户端主线程:连接服务器,启动收发子线程实现网络通信
 * @param   arg  线程参数(未使用)
 * @return  socket 创建失败时返回 NULL,正常情况永不返回
 * @note    断线自动重连(5 秒间隔),由 main.c 启动
 */
void* client(void* arg);

/**
 * @brief   HTTP 请求天气数据
 * @return  temp_humi_t 结构体,包含 temp 与 humi 字段;
 *         任何错误情况下返回默认值 {35, 70}
 * @note    通过 seniverse API 拉取广州实时天气,
 *          错误返回值 {35, 70} 可能掩盖真实错误,调用方需注意
 */
extern temp_humi_t httprequest_weather();

#endif /* NETWORK_H */
