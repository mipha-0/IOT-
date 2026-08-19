#include "data_source.h"
#include "ui_common.h"
#include "ui_page_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../net/network.h"

/*====================================================*
 *  数据更新
 *====================================================*/

/*获取温湿度（后台线程）*/
void* get_temp_humi(void* arg)
{
    pthread_detach(pthread_self());
    temp_humi_t th;
    for(;;)
    {
        /* HTTP 请求在锁外执行，避免阻塞读者 */
        th = httprequest_weather();

        pthread_rwlock_wrlock(&th_rwlock);
        mock_temp = th.temp;
        mock_humi = th.humi;
        pthread_rwlock_unlock(&th_rwlock);

        /* 通知 write_data_to_file 线程有新数据 */
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_signal(&cond_var);
        pthread_mutex_unlock(&cond_mutex);


        sleep(5);
    }
    return NULL;
}

/*将数据写入本地文件（后台线程，事件驱动）*/
void* write_data_to_file(void* arg)
{
    pthread_detach(pthread_self());
    char time_str[32]={0x00};
    time_t t;
    struct tm *tm;
    FILE *fp = fopen("./data.txt", "a");
    if(fp == NULL)
    {
        printf("Error: file %s not found\n", "./data.txt");
        return NULL;
    }

    for(;;)
    {
        /* 等待 get_temp_humi 发出的新数据信号 */
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_wait(&cond_var, &cond_mutex);
        pthread_mutex_unlock(&cond_mutex);

        pthread_rwlock_rdlock(&th_rwlock);
        float temp = mock_temp;
        float humidity = mock_humi;
        pthread_rwlock_unlock(&th_rwlock);

        t = time(NULL);
        tm = localtime(&t);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
        fprintf(fp, "%s,%.1f,%.1f\n", time_str, temp, humidity);
        fflush(fp);
    }
    fclose(fp);
}