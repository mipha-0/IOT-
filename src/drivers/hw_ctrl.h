/**
 * @file    hw_ctrl.h
 * @brief   硬件驱动控制模块对外接口
 *
 * @details 声明 LED 与蜂鸣器控制线程入口,由 main.c 启动。
 *          详细行为见 hw_ctrl.c 文件头说明。
 */
#ifndef HW_CTRL_H
#define HW_CTRL_H

#include <sys/ioctl.h>
#include <unistd.h>

/**
 * @brief   LED 控制线程入口
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    根据温度阈值控制 4 路 LED 常亮或按比例闪烁,
 *          LED_status=OFF 时阻塞等待 led_cond 唤醒。
 */
extern void* led_ctrl(void* arg);

/**
 * @brief   蜂鸣器控制线程入口
 * @param   arg  线程参数(未使用)
 * @return  永不返回(死循环)
 * @note    温度或湿度超阈值时蜂鸣器间歇告警(响 1s 停 1s),
 *          BEEP_status=OFF 时阻塞等待 beep_cond 唤醒。
 */
extern void* beep_ctrl(void* arg);


#endif
