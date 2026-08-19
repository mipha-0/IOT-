/**
 * @file    main.c
 * @author  IOT 数据监控平台
 * @brief   程序入口模块
 *
 * @details 该文件是整个 IOT 数据监控平台的入口,负责以下核心工作:
 *          1. LVGL 图形库初始化、显示设备(framebuffer)与输入设备(evdev)注册;
 *          2. 创建主 UI(导航栏 + 4 个页面);
 *          3. 启动 5 个后台 POSIX 线程:
 *             - client        : TCP 客户端,接收远程命令并回传数据
 *             - get_temp_humi : HTTP 拉取天气数据,写入共享变量
 *             - write_data_to_file: 监听新数据并持久化至 ./data.txt
 *             - led_ctrl      : 根据温度阈值控制 4 路 LED 闪烁/常亮
 *             - beep_ctrl     : 超阈值时蜂鸣器报警
 *          4. 主线程进入 lv_timer_handler 循环,驱动 LVGL 事务;
 *          5. 创建一个 lv_timer 定时消费后台线程推入的 pending 事件,
 *             实现线程安全的 UI 更新(LVGL 非线程安全)。
 *
 * @note    运行需 root 权限以访问 /dev/Led、/dev/beep、/dev/rtc0。
 *          目标硬件: gec6818 开发板(800x480 LCD)。
 */

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


/**
 * @brief 显示缓冲区大小(像素数)
 * @note  缓冲越大绘制越流畅,但占用更多内存。480*1024 适配 800x480 屏幕。
 */
#define DISP_BUF_SIZE (480 * 1024)

uint32_t custom_tick_get(void);   /* 用真实时间驱动 LVGL tick,避免累积误差 */

/**
 * @brief  程序主入口
 * @return 程序退出码(实际永不返回,主循环为死循环)
 *
 * @details 工作流程:
 *          1. 初始化 LVGL 与随机数种子;
 *          2. 注册 framebuffer 显示设备(800x480)与 evdev 指针输入设备;
 *          3. 调用 create_ui() 构建主界面(导航栏 + 4 页面 + 锁初始化);
 *          4. 启动 5 个后台线程(线程句柄不保留,主线程不 join);
 *          5. 创建 200ms 周期的 lv_timer 用于消费 pending 事件队列;
 *          6. 主循环:用真实时间差驱动 lv_tick_inc,再调用 lv_timer_handler。
 */
int main(void)
{

    /*lvgl初始化*/
    lv_init();



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
    disp_drv.flush_cb   = fbdev_flush;     /* framebuffer 刷新回调 */
    disp_drv.hor_res    = 800;             /* 屏幕 horizontal 分辨率 */
    disp_drv.ver_res    = 480;             /* 屏幕 vertical 分辨率 */
    lv_disp_drv_register(&disp_drv);

    /*输入设备初始化及注册*/
    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    /*This function will be called periodically (by the library) to get the mouse position and state*/
    indev_drv_1.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv_1);
    /*创建ui(导航栏 + 4 页面 + 时间标签 + 所有锁/条件变量)*/
    create_ui();

    /*--------------------- 启动 5 个后台线程 ---------------------*/
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

    /* 主线程定时消费后台线程推入的待处理事件(线程安全更新 UI)
     * 周期 200ms: 平衡响应速度与 CPU 占用。
     * LVGL 非线程安全,后台线程不能直接调 LVGL API,只能通过此队列中转。 */
    lv_timer_create(process_pending_events, 200, NULL);

    /*事物处理及告知lvgl节拍数*/
    uint32_t last_tick = custom_tick_get();
    while(1)
    {
        uint32_t now = custom_tick_get();
        lv_tick_inc(now - last_tick);   /* 用真实时间差喂 tick,消除累积误差 */
        last_tick = now;

        lv_timer_handler();//事务处理
        usleep(5000);   /* 5ms 让出 CPU,避免空转 100% 占用 */
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
