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

#define TEST_MAGIC 'x'                    //定义幻数
#define LED1 _IO(TEST_MAGIC, 0)              
#define LED2 _IO(TEST_MAGIC, 1)
#define LED3 _IO(TEST_MAGIC, 2)
#define LED4 _IO(TEST_MAGIC, 3) 

#define LED_ON  	0	//灯亮
#define LED_OFF		1   //灯灭
#define BEEP_ON		0	//蜂鸣器开
#define BEEP_OFF	1   //蜂鸣器关
/*led控制*/
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
        pthread_rwlock_rdlock(&threshold_rwlock);
        tm_threshold = temp_threshold;
        pthread_rwlock_unlock(&threshold_rwlock);

        pthread_rwlock_rdlock(&th_rwlock);
        current_temp = mock_temp;
        pthread_rwlock_unlock(&th_rwlock);

        // 温度达到或超过阈值，四个LED常亮
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
            // 根据温度计算闪烁延时：温度越高，闪烁越快
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
/*蜂鸣器控制*/
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
        /*蜂鸣器控制（温度或湿度超过阈值时蜂鸣1s）*/
        if(temp > tm_threshold || humi > hm_threshold)
        {
            ioctl(beep_fd, BEEP_ON, 1);
            sleep(1);
            ioctl(beep_fd, BEEP_OFF, 1);
            sleep(1);   /* 间歇告警：响1s停1s */
        }
        else
        {
            sleep(1);   /* 未超阈值时避免空循环 */
        }
    }
    close(beep_fd);
    return NULL;
}
