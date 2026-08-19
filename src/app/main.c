#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "ui_manager.h"
#include "../drivers/hw_ctrl.h"
#include "data_source.h"
#include "network.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>


#define DISP_BUF_SIZE (480 * 1024)

uint32_t custom_tick_get(void);   /* 用真实时间驱动 LVGL tick，避免累积误差 */

int main(void)
{

    /*lvgl初始化*/
    lv_init();
    srand(time(NULL));


    /*输出设备初始化及注册*/
    fbdev_init();
    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf[DISP_BUF_SIZE];
    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    /*Initialize and register a display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = fbdev_flush;
    disp_drv.hor_res    = 800;
    disp_drv.ver_res    = 480;
    lv_disp_drv_register(&disp_drv);

    /*输入设备初始化及注册*/
    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    /*This function will be called periodically (by the library) to get the mouse position and state*/
    indev_drv_1.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv_1);
    /*创建ui*/
    create_ui();
    
    pthread_t client_thread;
    if(pthread_create(&client_thread, NULL, client, NULL) != 0)
    {
        perror("pthread_create client");
    }
    pthread_t temp_thread;
    if(pthread_create(&temp_thread, NULL, get_temp_humi, NULL) != 0)
    {
        perror("pthread_create get_temp_humi");
    }
    pthread_t write_thread;
    if(pthread_create(&write_thread, NULL, write_data_to_file, NULL) != 0)
    {
        perror("pthread_create write_data_to_file");
    }
    pthread_t led_thread;
    if(pthread_create(&led_thread, NULL, led_ctrl, NULL) != 0)
    {
        perror("pthread_create led_ctrl");
    }
    pthread_t beep_thread;
    if(pthread_create(&beep_thread, NULL, beep_ctrl, NULL) != 0)
    {
        perror("pthread_create beep_ctrl");
    }

    /* 主线程定时消费后台线程推入的待处理事件（线程安全更新 UI） */
    lv_timer_create(process_pending_events, 200, NULL);

    /*事物处理及告知lvgl节拍数*/
    uint32_t last_tick = custom_tick_get();
    while(1)
    {
        uint32_t now = custom_tick_get();
        lv_tick_inc(now - last_tick);   /* 用真实时间差喂 tick，消除累积误差 */
        last_tick = now;

        lv_timer_handler();//事务处理
        usleep(5000);
    }

    return 0;
}


/*用户节拍获取*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
