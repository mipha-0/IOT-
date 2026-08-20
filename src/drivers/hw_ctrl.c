/**
 * @file    hw_ctrl.c
 * @brief   硬件驱动控制模块(LED + 蜂鸣器)
 *
 * @details 提供 2 个后台线程,通过 Linux ioctl 接口控制开发板硬件:
 *          1. led_ctrl  : 根据 LED_status 和当前温度/阈值,控制 4 路 LED:
 *             - OFF        : 全灭并阻塞,等待用户开启信号
 *             - temp >= 阈值: 4 路常亮
 *             - temp <  阈值: 按温度比例计算闪烁延时,4 路同步闪烁
 *          2. beep_ctrl : 根据 BEEP_status 和当前温湿度/阈值,控制蜂鸣器:
 *             - OFF           : 关闭并阻塞,等待用户开启信号
 *             - 超阈值         : 响 1s 停 1s 间歇报警
 *             - 未超阈值       : sleep(1) 避免空循环
 *
 *          等待-唤醒模式:
 *          - led_cond / led_mutex  : LED 关闭时阻塞,被 led_sw_cb 或
 *                                     net/network 命令处理唤醒
 *          - beep_cond / beep_mutex : 同上,对应蜂鸣器
 *
 *          依赖关系:
 *          - 共享状态: LED_status/BEEP_status + status_mutex
 *          - 阈值数据: temp_threshold/humi_threshold + threshold_rwlock
 *          - 实时数据: mock_temp/mock_humi + th_rwlock
 *          - 设备节点: /dev/Led、/dev/beep
 *
 * @note    需要 root 权限访问 /dev/Led、/dev/beep。
 *          幻数 'x' 与内核驱动约定一致,修改需同步内核侧。
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "hw_ctrl.h"
#include "ui_common.h"
#include "ui_page_settings.h"

#define TEST_MAGIC 'x'                    /* 内核驱动约定的幻数,用于生成 ioctl 命令 */
#define LED1 _IO(TEST_MAGIC, 0)           /* LED1 控制命令 */
#define LED2 _IO(TEST_MAGIC, 1)           /* LED2 控制命令 */
#define LED3 _IO(TEST_MAGIC, 2)           /* LED3 控制命令 */
#define LED4 _IO(TEST_MAGIC, 3)           /* LED4 控制命令 */

#define LED_ON  	0	/*灯亮:ioctl 第三参数 */
#define LED_OFF		1   /*灯灭:ioctl 第三参数 */
#define BEEP_ON		0	/*蜂鸣器开:ioctl 第三参数 */
#define BEEP_OFF	1   /*蜂鸣器关:ioctl 第三参数 */

/**
 * @brief   LED 控制线程(后台)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 打开 /dev/Led 设备,失败则退出线程;
 *          2. 循环:
 *             a) 读 LED_status(OFF 时全灭并阻塞等 led_cond 信号);
 *             b) 读温度阈值 threshold_rwlock 与当前温度 th_rwlock;
 *             c) 温度 >= 阈值: 4 路 LED 常亮,usleep 500ms 后重新检查;
 *             d) 温度 <  阈值: 计算闪烁延时 delay_us(温度越高延时越短,
 *                闪烁越快),4 路同步亮 delay_us 后灭 delay_us。
 *
 * 闪烁延时算法:
 *   ratio    = current_temp / tm_threshold  (0~1)
 *   delay_us = 1000000 - 950000 * ratio     (50ms~1000ms)
 *   下限保护: delay_us 不小于 50ms,避免过快闪烁伤眼/无效。
 *
 * @note    led_cond 由 led_sw_cb(设置页 LED 开关)或 net/network::get_news
 *          (远程 LED 开命令)在 LED_status 从 OFF 切到 ON 时发送。
 */
void* led_ctrl(void* arg)
{
    pthread_detach(pthread_self());

    int led_fd = open("/dev/Led", O_RDWR);
    if(led_fd < 0)
    {
        perror("open led");
        return NULL;
    }

    while(1)
    {
        /* 加锁读取 LED 状态 */
        pthread_mutex_lock(&status_mutex);
        uint8_t cur_led_status = LED_status;
        pthread_mutex_unlock(&status_mutex);

        if(cur_led_status == OFF)
        {
            /* LED 关闭: 全灭 */
            ioctl(led_fd, LED1, LED_OFF);
            ioctl(led_fd, LED2, LED_OFF);
            ioctl(led_fd, LED3, LED_OFF);
            ioctl(led_fd, LED4, LED_OFF);
            /* 阻塞等下次用户开 LED 时 led_sw_cb 发的 signal */
            pthread_mutex_lock(&led_mutex);
            pthread_cond_wait(&led_cond, &led_mutex);
            pthread_mutex_unlock(&led_mutex);
            continue;
        }

        float current_temp;
        float tm_threshold;
        /* 读温度阈值 */
        pthread_rwlock_rdlock(&threshold_rwlock);
        tm_threshold = temp_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);

        /* 读当前实时温度 */
        pthread_rwlock_rdlock(&th_rwlock);
        current_temp = mock_temp;
        pthread_rwlock_unlock(&th_rwlock);

        // 温度达到或超过阈值,四个LED常亮
        if(current_temp >= tm_threshold)
        {
            ioctl(led_fd, LED1, LED_ON);
            ioctl(led_fd, LED2, LED_ON);
            ioctl(led_fd, LED3, LED_ON);
            ioctl(led_fd, LED4, LED_ON);
            usleep(500000);
        }
        else
        {
            // 根据温度计算闪烁延时:温度越高,闪烁越快
            float ratio = current_temp / tm_threshold;
            int delay_us = 1000000 - (int)(950000 * ratio);
            if(delay_us < 50000) delay_us = 50000;  /* 下限保护 */

            // 四个LED同时点亮
            ioctl(led_fd, LED1, LED_ON);
            ioctl(led_fd, LED2, LED_ON);
            ioctl(led_fd, LED3, LED_ON);
            ioctl(led_fd, LED4, LED_ON);
            usleep(delay_us);

            // 四个LED同时熄灭
            ioctl(led_fd, LED1, LED_OFF);
            ioctl(led_fd, LED2, LED_OFF);
            ioctl(led_fd, LED3, LED_OFF);
            ioctl(led_fd, LED4, LED_OFF);
            usleep(delay_us);
        }
    }

    close(led_fd);
    return NULL;
}

/**
 * @brief   蜂鸣器控制线程(后台)
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环),理论返回 NULL
 *
 * @details 工作流程:
 *          1. 打开 /dev/beep 设备,失败则退出线程;
 *          2. 循环:
 *             a) 读 BEEP_status(OFF 时关闭蜂鸣器并阻塞等 beep_cond);
 *             b) 读温湿度阈值 threshold_rwlock 与当前温湿度 th_rwlock;
 *             c) 温度 > 阈值 或 湿度 > 阈值: 蜂鸣 1s 停 1s 间歇告警;
 *             d) 否则: sleep(1) 避免空转。
 *
 * @note    beep_cond 由 beep_sw_cb(设置页 BEEP 开关)或 net/network::get_news
 *          (远程蜂鸣器命令)在 BEEP_status 从 OFF 切到 ON 时发送。
 *          间歇告警(响 1s 停 1s)避免连续噪音刺耳。
 */
void* beep_ctrl(void* arg)
{
    pthread_detach(pthread_self());
    int beep_fd = open("/dev/beep", O_RDWR);
    if(beep_fd < 0)
    {
        perror("open beep");
        return NULL;
    }
    while(1)
    {
        /* 加锁读取 BEEP 状态 */
        pthread_mutex_lock(&status_mutex);
        uint8_t cur_beep_status = BEEP_status;
        pthread_mutex_unlock(&status_mutex);

        /*等待蜂鸣器开关*/
        if(cur_beep_status == OFF)
        {
            /*蜂鸣器关闭*/
            ioctl(beep_fd, BEEP_OFF, 1);
            /* 阻塞等下次用户开蜂鸣器时 beep_sw_cb 发的 signal */
            pthread_mutex_lock(&beep_mutex);
            pthread_cond_wait(&beep_cond, &beep_mutex);
            pthread_mutex_unlock(&beep_mutex);
            continue;   /* 被唤醒后重新检查状态 */
        }
        /*获取阈值和当前值*/
        float tm_threshold, hm_threshold, temp, humi;
        pthread_rwlock_rdlock(&threshold_rwlock);
        tm_threshold = temp_threshold;
        hm_threshold = humi_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);
        pthread_rwlock_rdlock(&th_rwlock);
        temp = mock_temp;
        humi = mock_humi;
        pthread_rwlock_unlock(&th_rwlock);
        /*蜂鸣器控制(温度或湿度超过阈值时蜂鸣1s)*/
        if(temp >= tm_threshold || humi >= hm_threshold)
        {
            ioctl(beep_fd, BEEP_ON, 1);
            sleep(1);
            ioctl(beep_fd, BEEP_OFF, 1);
            sleep(1);   /* 间歇告警:响1s停1s */
        }
        else
        {
            sleep(1);   /* 未超阈值时避免空循环 */
        }
    }
    close(beep_fd);
    return NULL;
}
