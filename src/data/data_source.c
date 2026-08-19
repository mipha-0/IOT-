/**
 * @file    data_source.c
 * @brief   数据采集与持久化模块
 *
 * @details 提供 2 个后台线程:
 *          1. get_temp_humi     : 每 5 秒通过 HTTP 拉取天气数据,
 *                                 写入共享变量 mock_temp/mock_humi,
 *                                 并通过 cond_var 通知其他等待线程;
 *          2. write_data_to_file: 监听 cond_var 信号,收到新数据时
 *                                 将 "时间,温度,湿度" 追加写入 ./data.txt。
 *
 *          依赖关系:
 *          - 调用 net/network::httprequest_weather() 拉取天气
 *          - 共享状态: mock_temp/mock_humi + th_rwlock + cond_mutex/cond_var
 *          - 文件输出: ./data.txt(供历史页读取)
 *
 * @note    文件指针 fp 在线程函数内长期持有,直至线程退出。
 *          data.txt 路径相对于可执行文件当前工作目录。
 */

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

/**
 * @brief  获取温湿度数据(后台线程,5 秒周期)
 * @param  arg  线程参数(未使用)
 * @return 永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 调用 httprequest_weather() 通过 seniverse API 拉取广州实时天气;
 *          2. 获取写锁 th_rwlock,更新 mock_temp/mock_humi;
 *          3. 通过 cond_var 广播信号,通知 write_data_to_file、
 *             mock_data_update(主页刷新)、update_history_data(历史页刷新);
 *          4. sleep(5) 后重复。
 *
 * @note   HTTP 请求在锁外执行,避免长时间持有写锁阻塞主页读取。
 *         线程通过 pthread_detach 自行回收资源。
 *         网络异常时 httprequest_weather 返回默认值 (35, 70)。
 */
void* get_temp_humi(void* arg)
{
    pthread_detach(pthread_self());
    temp_humi_t th;
    for(;;)
    {
        /* HTTP 请求在锁外执行,避免阻塞读者 */
        th = httprequest_weather();

        pthread_rwlock_wrlock(&th_rwlock);
        mock_temp = th.temp;
        mock_humi = th.humi;
        pthread_rwlock_unlock(&th_rwlock);

        /* 通知 write_data_to_file 线程有新数据 */
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_broadcast(&cond_var);
        pthread_mutex_unlock(&cond_mutex);

        sleep(5);
    }
    return NULL;
}

/**
 * @brief  将数据写入本地文件(后台线程,事件驱动)
 * @param  arg  线程参数(未使用)
 * @return 永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 打开 ./data.txt 追加模式,失败则直接退出线程;
 *          2. 阻塞等待 cond_var 信号(get_temp_humi 发出);
 *          3. 收到信号后读共享变量 mock_temp/mock_humi;
 *          4. 格式化为 "YYYY-MM-DD HH:MM:SS,温度,湿度\n" 追加写入文件;
 *          5. fflush 立即刷盘,保证数据不丢失;
 *          6. 回到步骤 2。
 *
 * @note   文件指针在循环外打开,长期持有,直至线程被强制终止。
 *         写入路径相对当前工作目录,与可执行文件同目录。
 */
void* write_data_to_file(void* arg)
{
    pthread_detach(pthread_self());
    char time_str[32]={0x00};
    time_t t;
    struct tm *tm;
    

    for(;;)
    {
        FILE *fp = fopen("./data.txt", "a");
        if(fp == NULL)
        {
            printf("Error: file %s not found\n", "./data.txt");
            return NULL;
        }
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
        fclose(fp);
    }
    
}
