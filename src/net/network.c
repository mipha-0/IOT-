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

        /* 线程安全：通过 pending 队列在主线程更新 UI */
        push_pending_event(buf);
        request_threshold_refresh();

        /* 唤醒 LED/BEEP 线程（在锁外发送信号避免死锁） */
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
void* send_news(void* arg)
{
    int fd = *(int*)arg;
    char buf[128];
    int ret;

    while(1)
    {
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
        sleep(5);
    }
    pthread_mutex_lock(&status_mutex);
    connect_event = OFF;
    pthread_mutex_unlock(&status_mutex);
    strcpy(buf, "system:connect failed");
    push_pending_event(buf);

    return NULL;
}

/*===================== 客户端主线程 =====================*/
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
    /* 关闭 socket，send 线程的 send 会失败并退出 */
    close(socket_fd);
    pthread_join(send_tid, NULL);
    goto reconnect;

    return NULL;
}

/*HTTP请求天气*/
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

    // 接收对方的响应头部
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
