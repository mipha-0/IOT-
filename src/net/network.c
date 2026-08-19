/**
 * @file    network.c
 * @brief   网络通信模块(TCP 客户端 + HTTP 天气拉取)
 *
 * @details 提供 3 个对内线程入口与 1 个对外 HTTP 接口:
 *
 *  线程:
 *  - client            : TCP 客户端主线程,连接服务器后启动收发子线程,
 *                         断线自动重连(5 秒间隔);
 *  - get_news (子线程)  : 接收服务器下发的 JSON 命令,解析后更新共享状态
 *                         (LED/蜂鸣器/温湿度阈值),唤醒硬件线程,推 UI 事件;
 *  - send_news(子线程) : 每获取一次数据打包当前温湿度/阈值/设备状态为 JSON 上报。
 *
 *  对外接口:
 *  - httprequest_weather(): 向 seniverse API 发起 HTTP GET 拉取广州实时天气,
 *                           返回 temp_humi_t 结构体。
 *
 *  命令格式(JSON):
 *    {"command_name":"LED","paras":{"LED开关":1}}
 *    {"command_name":"蜂鸣器","paras":{"蜂鸣器开关":0}}
 *    {"command_name":"温度阈值","paras":{"temp_threshold":35}}
 *    {"command_name":"湿度阈值","paras":{"humi_threshold":50}}
 *
 *  依赖关系:
 *  - 共享状态: mock_temp/mock_humi + th_rwlock(读)
 *              temp_threshold/humi_threshold + threshold_rwlock(写)
 *              LED_status/BEEP_status/connect_event + status_mutex(写)
 *  - 硬件唤醒: led_cond / beep_cond
 *  - UI 通信: push_pending_event() / request_threshold_refresh()
 *  - 第三方: cJSON(解析与生成)
 *
 * @note    服务器地址硬编码 192.168.44.21:8888;天气 API key 硬编码。
 *          修改需重新编译。多客户端连接时 cli_socket_fd 单值覆盖(已知限制)。
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "network.h"
#include "ui_page_settings.h"
#include <stdlib.h>
#include <netdb.h>
#include "cJSON.h"


/*====================================================
   网络通信模块（客户端）
====================================================*/

/*===================== 接收线程 =====================*/
/**
 * @brief   命令接收子线程(由 client 创建)
 * @param   arg  指向已连接 socket fd 的指针(int*)
 * @return  对端关闭或 recv 出错时返回 NULL
 *
 * @details 循环接收 JSON 命令并分发:
 *          1. recv 读取数据,ret<=0 表示对端关闭,退出循环;
 *          2. cJSON_Parse 解析,失败打印日志并 continue;
 *          3. 取 command_name 与 paras 字段,缺一不可;
 *          4. 按 command_name 分支:
 *             - "LED"      : 写 LED_status + 需要时发 led_cond 信号
 *             - "蜂鸣器"    : 写 BEEP_status + 需要时发 beep_cond 信号
 *             - "温度阈值"  : 写 temp_threshold,cfg_saved=false
 *             - "湿度阈值"  : 写 humi_threshold,cfg_saved=false
 *          5. cJSON_Delete 释放;
 *          6. push_pending_event 推事件到主线程 UI 队列;
 *          7. request_threshold_refresh 请求刷新阈值显示;
 *          8. 若 LED/BEEP 从 OFF→ON,在锁外发 cond 信号唤醒硬件线程。
 *
 * @note    buf 同时被复用为接收缓冲与事件文本缓冲,通过 sprintf 改写。
 *          signal 在锁外发送以避免持锁等待对方持锁导致死锁。
 */
void* get_news(void* arg)
{
    int fd = *(int*)arg;

    char buf[1024]={0};
    char command_name[32]={0};
    int ret;

    while(1)
    {
        bzero(buf, sizeof(buf));
        bzero(command_name, sizeof(command_name));

        /* 接收数据 */
        ret = recv(fd, buf, sizeof(buf) - 1, 0);
        if(ret <= 0) break;       /* 对端关闭或出错 */
        buf[ret] = '\0';
        cJSON *root = cJSON_Parse(buf);
        if(root == NULL)
        {
            printf("net: error json\n");
            continue;
        }
        cJSON *cmd_item = cJSON_GetObjectItem(root, "command_name");
        cJSON *paras    = cJSON_GetObjectItem(root, "paras");
        if(cmd_item == NULL || cmd_item->valuestring == NULL || paras == NULL)
        {
            printf("net: missing command_name or paras\n");
            cJSON_Delete(root);
            continue;
        }
        strcpy(command_name, cmd_item->valuestring);

        bool need_led_signal = false;
        bool need_beep_signal = false;

        if(strcmp(command_name, "LED") == 0)
        {
            printf("net: update LED\n");
            cJSON *val = cJSON_GetObjectItem(paras, "LED开关");
            if(val)
            {
                uint8_t new_status = val->valueint;
                pthread_mutex_lock(&status_mutex);
                LED_status = new_status;
                if(LED_status == ON) need_led_signal = true;
                sprintf(buf, "command:update LED: %s", LED_status == ON ? "ON" : "OFF");
                pthread_mutex_unlock(&status_mutex);
            }
        }
        else if(strcmp(command_name, "蜂鸣器") == 0)
        {
            printf("net: update BEEP\n");
            cJSON *val = cJSON_GetObjectItem(paras, "蜂鸣器开关");
            if(val)
            {
                uint8_t new_status = val->valueint;
                pthread_mutex_lock(&status_mutex);
                BEEP_status = new_status;
                if(BEEP_status == ON) need_beep_signal = true;
                sprintf(buf, "command:update BEEP: %s", BEEP_status == ON ? "ON" : "OFF");
                pthread_mutex_unlock(&status_mutex);
            }
        }
        else if(strcmp(command_name, "温度阈值") == 0)
        {
            printf("net: update temp_threshold\n");
            cJSON *val = cJSON_GetObjectItem(paras, "temp_threshold");
            if(val)
            {
                float new_val = (float)val->valueint;
                pthread_rwlock_wrlock(&threshold_rwlock);
                temp_threshold = new_val;
                pthread_rwlock_unlock(&threshold_rwlock);
                cfg_saved = false;
                sprintf(buf, "command:update temp_threshold: %.1f", new_val);
            }
        }
        else if(strcmp(command_name, "湿度阈值") == 0)
        {
            printf("net: update humi_threshold\n");
            cJSON *val = cJSON_GetObjectItem(paras, "humi_threshold");
            if(val)
            {
                float new_val = (float)val->valueint;
                pthread_rwlock_wrlock(&threshold_rwlock);
                humi_threshold = new_val;
                pthread_rwlock_unlock(&threshold_rwlock);
                cfg_saved = false;
                sprintf(buf, "command:update humi_threshold: %.1f", new_val);
            }
        }

        cJSON_Delete(root);

        /* 线程安全:通过 pending 队列在主线程更新 UI */
        push_pending_event(buf);
        request_threshold_refresh();

        /* 唤醒 LED/BEEP 线程(在锁外发送信号避免死锁) */
        if(need_led_signal)
        {
            pthread_mutex_lock(&led_mutex);
            pthread_cond_signal(&led_cond);
            pthread_mutex_unlock(&led_mutex);
        }
        if(need_beep_signal)
        {
            pthread_mutex_lock(&beep_mutex);
            pthread_cond_signal(&beep_cond);
            pthread_mutex_unlock(&beep_mutex);
        }
    }

    return NULL;
}

/*===================== 发送线程 =====================*/
/**
 * @brief   状态上报子线程(由 client 创建)
 * @param   arg  指向已连接 socket fd 的指针(int*)
 * @return  send 失败时返回 NULL
 *
 * @details 每获取一次数据就将数据打包 JSON 上报到服务器,字段包括:
 *          time、temp、humi、LED、BEEP、temp_threshold、humi_threshold。
 *          send 失败(ret<=0)即退出线程,主线程 client 会关闭 fd 后重连。
 *
 * @note    退出前会更新 connect_event=OFF 并推送 "connect failed" 事件,
 *          触发事件页与系统状态卡片刷新。
 */
void* send_news(void* arg)
{
    int fd = *(int*)arg;
    char buf[128];
    int ret;

    while(1)
    {
        /* 等待新数据信号 */
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_wait(&cond_var, &cond_mutex);
        pthread_mutex_unlock(&cond_mutex);

        /* 读共享温湿度 */
        char time_date[32];
        pthread_rwlock_rdlock(&th_rwlock);
        float temp = mock_temp;
        float humi = mock_humi;
        pthread_rwlock_unlock(&th_rwlock);

        /* 读阈值和状态（加锁） */
        pthread_rwlock_rdlock(&threshold_rwlock);
        float tm_threshold = temp_threshold;
        float hm_threshold = humi_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);

        pthread_mutex_lock(&status_mutex);
        uint8_t led_st = LED_status;
        uint8_t beep_st = BEEP_status;
        pthread_mutex_unlock(&status_mutex);

        /* 时间*/
        char time_str[32] = {0x00};
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
        strcpy(time_date, time_str);

        bzero(buf, sizeof(buf));
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "time", time_date);
        cJSON_AddNumberToObject(root, "temp", temp);
        cJSON_AddNumberToObject(root, "humi", humi);
        cJSON_AddNumberToObject(root, "LED", led_st);
        cJSON_AddNumberToObject(root, "BEEP", beep_st);
        cJSON_AddNumberToObject(root, "temp_threshold", tm_threshold);
        cJSON_AddNumberToObject(root, "humi_threshold", hm_threshold);
        char *json_str = cJSON_Print(root);
        ret = send(fd, json_str, strlen(json_str), 0);
        cJSON_Delete(root);
        free(json_str);
        if(ret <= 0) break;
    }
    /* 通知主线程连接已断开 */
    pthread_mutex_lock(&status_mutex);
    connect_event = OFF;
    pthread_mutex_unlock(&status_mutex);
    strcpy(buf, "system:connect failed");
    push_pending_event(buf);

    return NULL;
}

/*===================== 客户端主线程 =====================*/
/**
 * @brief   TCP 客户端主线程(由 main 创建)
 * @param   arg  线程参数(未使用)
 * @return  socket 创建失败时返回 NULL,正常情况永不返回
 *
 * @details 工作流程:
 *          1. 创建 socket,失败 perror 后退出;
 *          2. 循环 connect 服务器(192.168.44.21:8888),
 *             失败 sleep(5) 重试,直至成功;
 *          3. 连接成功:更新 connect_event=ON,推 "connect success" 事件;
 *          4. 启动 get_news、send_news 两个子线程;
 *          5. pthread_join 等待 get_news 退出(对端断开);
 *          6. close(fd),pthread_join send_news;
 *          7. goto reconnect 重连。
 *
 * @note    重连间隔为 5 秒,避免短时间大量失败连接占满资源。
 *          socket_fd 通过指针传递给子线程,生命周期由本函数管理。
 */
void* client(void* arg)
{

    /* 创建 socket */
    int socket_fd;
    char buf[1024];

    /* 连接服务器 */
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8888);
    addr.sin_addr.s_addr = inet_addr("192.168.44.21");
reconnect:
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
        perror("socket");
        return NULL;
    }
    while(1)
    {
        if(connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            sleep(5);
            continue;
        }
        else
        {
            printf("connect success\n");
            break;
        }
    }
    bzero(buf, sizeof(buf));
    strcpy(buf, "system:connect success");
    pthread_mutex_lock(&status_mutex);
    connect_event = ON;
    pthread_mutex_unlock(&status_mutex);
    push_pending_event(buf);

    /* 启动收发子线程 */
    pthread_t get_tid, send_tid;
    if(pthread_create(&get_tid,  NULL, get_news,  &socket_fd) != 0)
    {
        perror("pthread_create get_news");
        close(socket_fd);
        sleep(5);
        goto reconnect;
    }
    if(pthread_create(&send_tid, NULL, send_news, &socket_fd) != 0)
    {
        perror("pthread_create send_news");
        close(socket_fd);
        sleep(5);
        goto reconnect;
    }

    /* 等待接收线程结束（对端断开时退出） */
    pthread_join(get_tid, NULL);
    /* 关闭 socket,send 线程的 send 会失败并退出 */
    close(socket_fd);
    pthread_join(send_tid, NULL);
    goto reconnect;

    return NULL;
}

/**
 * @brief   HTTP 请求天气数据(对外接口)
 * @return  temp_humi_t 结构体,包含 temp 与 humi 字段;
 *          任何错误情况下返回默认值 {35, 70}
 *
 * @details 工作流程:
 *          1. gethostbyname 解析 api.seniverse.com 域名;
 *          2. 创建 TCP socket 并 connect 到 80 端口;
 *          3. 发送 HTTP GET 请求(广州实时天气);
 *          4. 逐字节接收响应头,直至遇到 "\r\n\r\n" 分隔符;
 *          5. 解析 HTTP 状态码(200~299 视为成功);
 *          6. 解析 Content-Length 决定正文长度;
 *          7. 接收正文(JSON),cJSON_Parse 解析;
 *          8. 从 results[0].now.temperature / humidity 取值;
 *          9. 释放资源并返回。
 *
 * 错误处理:
 *   - DNS 失败、socket 失败、connect 失败、recv 失败、JSON 解析失败
 *     均返回 {35, 70} 默认值,不抛出异常。
 *
 * @note    请求 URL 中 key=SKRr_WDhQZaxCxyxd 为 seniverse API key(硬编码)。
 *          逐字节接收响应头效率低,但保证不会读入正文,简化逻辑。
 *          返回的 temp_humi_t 在所有错误路径均默认 {35, 70},
 *          调用方需注意此默认值可能掩盖真实错误。
 */
temp_humi_t httprequest_weather()
{
    // 利用DNS服务查询指定域名的IP
    struct hostent *he = gethostbyname("api.seniverse.com");
    if(he == NULL)
    {
        perror("gethostbyname");    
        return (temp_humi_t){35, 70}; 
    }

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, len);

    addr.sin_family = AF_INET;
    addr.sin_addr   = *(struct in_addr*)((he->h_addr_list)[0]);
    addr.sin_port   = htons(80);

    // 创建TCP套接字(因为HTTP是基于TCP的)，并发起连接
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        perror("socket");
        return (temp_humi_t){35, 70};
    }
    if(connect(fd, (struct sockaddr *)&addr, len) < 0)
    {
        perror("connect");
        close(fd);
        return (temp_humi_t){35, 70};
    }

    static char request[1024];
    snprintf(request, 1024, "GET /v3/weather/now.json?key=SKRr_WDhQZaxCxyxd&location=guangzhou&language=zh-Hans&unit=c HTTP/1.1\r\n"
                            "Host:api.seniverse.com\r\n\r\n");
    send(fd, request, strlen(request), 0);

    // 接收对方的响应头部(逐字节直至 \r\n\r\n)
    char res[1024]={0x00};
    int total = 0;
    while(1)
    {
        int n = recv(fd, res + total, 1, 0);

        if(n <= 0)
        {
            perror("recv");    
            close(fd);
            return (temp_humi_t){35, 70}; 
        }

        total += n;

        if(strstr(res, "\r\n\r\n"))
            break;
    }

    int ok, jsonlen = 0;
    char *retcode = res + strlen("HTTP/1.x ");

    /* 解析 HTTP 状态码 */
    switch(atoi(retcode))
    {
    case 200 ... 299:
        ok = 1;
        break;

    case 400 ... 499:
        ok = 0;
        printf("client error %d\n", atoi(retcode));
        break;

    case 500 ... 599:
        ok = 0;
        printf("server error\n");
        break;

    default:
        ok = 0;
        break;
    }

    char *p=NULL;
    if((p = strstr(res, "Content-Length: ")) != NULL)
    {
        jsonlen = atoi(p + strlen("Content-Length: "));
    }

    if(!ok || jsonlen <= 0)
    {
        close(fd);
        return (temp_humi_t){35, 70}; 
    }

    // 接收正文
    char *json = calloc(1, jsonlen);
    if(json == NULL)
    {
        printf("calloc failed\n");
        close(fd);
        return (temp_humi_t){35, 70};
    }
    total = 0;
    while(jsonlen > 0)
    {
        int n    = recv(fd, json + total, jsonlen, 0);
        if(n <= 0) break;
        total   += n;
        jsonlen -= n;
    }

    cJSON *root = cJSON_Parse(json);
    if(root == NULL)
    {
        printf("weather: JSON parse error\n");
        free(json);
        close(fd);
        return (temp_humi_t){35, 70};
    }
    cJSON *body = cJSON_GetObjectItem(root, "results");
    cJSON *a = cJSON_GetArrayItem(body, 0);
    cJSON *now      = cJSON_GetObjectItem(a, "now");

    float temp = atof(cJSON_GetObjectItem(now, "temperature")->valuestring);
    float humi = atof(cJSON_GetObjectItem(now, "humidity")->valuestring);
    temp_humi_t th = {temp, humi};

    cJSON_Delete(root);
    free(json);
    close(fd);

    return th;
}
