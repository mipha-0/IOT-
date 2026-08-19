#ifndef NETWORK_H
#define NETWORK_H
#include "ui_common.h"
/* TCP 客户端主线程：连接服务器，启动收发子线程实现网络通信 */
void* client(void* arg);

extern temp_humi_t httprequest_weather();

#endif /* NETWORK_H */
