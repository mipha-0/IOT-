#ifndef SERVER_H
#define SERVER_H

/* TCP 服务器线程入口：pthread_detach 或 pthread_join 皆可
 * 启动后阻塞至 accept 成功（设备连接 + recv_news 线程已启动）后返回
 */
extern void* tcp_server_thread(void* arg);

/* 设备数据更新接口（由 AgentLiteDemo.c 实现，服务器的 recv_news 解析 JSON 后调用）
 * 约定：解析成功即调用一次；调用方不在持锁期间内执行
 */
void update_device_data(int led, int beep, float t, float h, float t_thresh, float h_thresh);

/* 下发命令到开发板（由服务器.c 实现，AgentLiteDemo.c 的 handleCommandRequest 调用）
 * 参数：
 *   json_str - 云端下发的原始 JSON 字符串（以 \0 结尾）
 *   len      - 字符串长度
 * 返回值：
 *   >0 实际发送字节数
 *   -1 socket 未连接或发送失败
 * 线程安全：内部加锁，可被多个回调线程同时调用
 */
int send_command_to_device(const char *json_str, size_t len);

/* 接收数据线程入口 */
extern void* recv_news(void* socket_fd);

#endif
