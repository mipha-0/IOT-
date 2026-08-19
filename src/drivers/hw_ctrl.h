#ifndef HW_CTRL_H
#define HW_CTRL_H

#include <sys/ioctl.h>
#include <unistd.h>
/*led控制*/
extern void* led_ctrl(void* arg); 
/*蜂鸣器控制*/
extern void* beep_ctrl(void* arg);


#endif
