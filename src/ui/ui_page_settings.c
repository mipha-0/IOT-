/**
 * @file    ui_page_settings.c
 * @brief   设置页模块:MQTT 配置 + 阈值调节 + LED/BEEP 开关
 *
 * @details 设置页布局:
 *          - 主卡片(SCR_W-16 x CONTENT_H-16),白色圆角阴影
 *          - 卡片内包含:
 *            1. MQTT 服务器地址、端口、设备 ID 显示(只读,通过 Reset 恢复默认);
 *            2. Reset / Save 按钮(恢复默认 / 保存配置到文件);
 *            3. 配置状态标签("已保存" / "未保存");
 *            4. Up / Down 按钮(调节当前选中阈值);
 *            5. BEEP / LED 开关(switch 控件);
 *            6. Temp / Humi 阈值切换按钮(选中后 Up/Down 调节该项)。
 *
 *          配置文件:
 *          - config.txt    : 保存 MQTT 服务器/端口/设备 ID
 *          - threshold.txt : 保存温湿度阈值
 *
 *          阈值调节:
 *          - threshold_mode=0 调温度,=1 调湿度
 *          - 步进 0.5,范围 0~100(湿度)或无上限(温度)
 *
 *          开关控制:
 *          - switch 状态变化时更新共享变量 + 唤醒硬件线程(led_cond/beep_cond)。
 */

#include "ui_page_settings.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>

/*===================== 设置页内部状态 =====================*/
static char     cfg_mqtt_server[64] = "iot-acc.cn-north-4.myhuaweicloud.com";   /* MQTT服务器地址(默认值) */
static uint16_t cfg_port = 8883;                /* 端口号(默认值) */
static char     cfg_device_id[64] = "hw_device_001";     /* 设备ID(默认值) */


static lv_obj_t *cfg_mqtt_server_lbl;        /* MQTT服务器数值标签 */
static lv_obj_t *cfg_port_lbl;               /* 端口号数值标签 */
static lv_obj_t *cfg_device_id_lbl;          /* 设备ID数值标签 */
static lv_obj_t *cfg_status_lbl;             /* 配置状态标签("已保存"/"未保存") */
static lv_obj_t *led_sw;                     /* LED开关滑块 */
static lv_obj_t *beep_sw;                    /* BEEP开关滑块 */
static lv_obj_t *btn_sel_temp;                 /* 温度阈值选择按钮 */
static lv_obj_t *btn_sel_humi;                 /* 湿度阈值选择按钮 */
static lv_obj_t *temp_thresh_lbl;                 /* 温度阈值标签 */
static lv_obj_t *humi_thresh_lbl;                 /* 湿度阈值标签 */
static lv_obj_t *card;                          /* 主卡片对象 */

static int threshold_mode = 0;                       /* 阈值选择模式:0=温度,1=湿度 */
float temp_threshold = 35.0f;                       /* 温度阈值(全局共享) */
float humi_threshold = 50.0f;                       /* 湿度阈值(全局共享) */
bool     cfg_saved = true;                          /* 配置保存状态(true=已保存) */

/*====================================================*
 *  设置页回调
 *====================================================*/
/* 内部函数(在 ui_page_settings.c 内实现,供 process_pending_events 调用) */
extern void display_threshold(void);
extern void write_threshold_config(void);


/**
 * @brief   重置配置按钮回调
 * @param   e  LVGL 事件对象
 *
 * @details 将 MQTT 配置恢复为代码内默认值,阈值恢复为 35/50,
 *          标记 cfg_saved=false,调用 display_threshold 刷新显示。
 *
 * @note    本函数不写入文件,需用户点击 Save 才会持久化。
 */
static void settings_reset_btn_cb(lv_event_t *e)
{
    cfg_saved = false;
    strncpy(cfg_mqtt_server, "iot-acc.cn-north-4.myhuaweicloud.com", sizeof(cfg_mqtt_server) - 1);
    cfg_mqtt_server[sizeof(cfg_mqtt_server) - 1] = '\0';
    cfg_port = 8883;
    strncpy(cfg_device_id, "hw_device_001", sizeof(cfg_device_id) - 1);
    cfg_device_id[sizeof(cfg_device_id) - 1] = '\0';

    pthread_rwlock_wrlock(&threshold_rwlock);
    temp_threshold = 35.0f;
    humi_threshold = 50.0f;
    pthread_rwlock_unlock(&threshold_rwlock);
    display_threshold();

}

/**
 * @brief   保存配置按钮回调
 * @param   e  LVGL 事件对象
 *
 * @details 标记 cfg_saved=true,调用 write_config 与 write_threshold_config
 *          将配置持久化到 config.txt 与 threshold.txt,更新状态标签。
 */
static void settings_save_btn_cb(lv_event_t *e)
{
    cfg_saved = true;
    write_config();
    write_threshold_config();
    lv_label_set_text(cfg_status_lbl, "已保存");
}

/**
 * @brief   切换选择温度阈值按钮回调
 * @param   e  LVGL 事件对象
 * @details 设置 threshold_mode=0,后续 Up/Down 调节温度阈值。
 */
static void settings_sel_temp_btn_cb(lv_event_t *e)
{
    threshold_mode = 0;
    display_threshold();
}

/**
 * @brief   切换选择湿度阈值按钮回调
 * @param   e  LVGL 事件对象
 * @details 设置 threshold_mode=1,后续 Up/Down 调节湿度阈值。
 */
static void settings_sel_humi_btn_cb(lv_event_t *e)
{
    threshold_mode = 1;
    display_threshold();
}

/**
 * @brief   上升阈值按钮回调
 * @param   e  LVGL 事件对象
 *
 * @details 根据 threshold_mode 调节对应阈值:
 *          - 模式 0: temp_threshold += 0.5(无上限)
 *          - 模式 1: humi_threshold += 0.5(上限 100)
 *          标记 cfg_saved=false,刷新显示。
 */
static void settings_inc_btn_cb(lv_event_t *e)
{
    if(threshold_mode == 0)
    {
        pthread_rwlock_wrlock(&threshold_rwlock);
        temp_threshold += 0.5f;
        pthread_rwlock_unlock(&threshold_rwlock);
    }
    else
    {
        pthread_rwlock_wrlock(&threshold_rwlock);
        if(humi_threshold < 100.0f)
            humi_threshold += 0.5f;
        pthread_rwlock_unlock(&threshold_rwlock);
    }
    cfg_saved = false;
    display_threshold();
}

/**
 * @brief   下降阈值按钮回调
 * @param   e  LVGL 事件对象
 *
 * @details 根据 threshold_mode 调节对应阈值(下限 0):
 *          - 模式 0: temp_threshold -= 0.5(下限 0)
 *          - 模式 1: humi_threshold -= 0.5(下限 0)
 *          标记 cfg_saved=false,刷新显示。
 */
static void settings_dec_btn_cb(lv_event_t *e)
{
    if(threshold_mode == 0)
    {
        pthread_rwlock_wrlock(&threshold_rwlock);
        if(temp_threshold > 0.0f)
            temp_threshold -= 0.5f;
        pthread_rwlock_unlock(&threshold_rwlock);
    }
    else
    {
        pthread_rwlock_wrlock(&threshold_rwlock);
        if(humi_threshold > 0.0f)
            humi_threshold -= 0.5f;
        pthread_rwlock_unlock(&threshold_rwlock);
    }
    cfg_saved = false;
    display_threshold();

}

/**
 * @brief   蜂鸣器开关回调
 * @param   e  LVGL 事件对象
 *
 * @details 切换 BEEP_status:
 *          1. 加锁翻转 BEEP_status;
 *          2. 若新状态为 ON,发送 beep_cond 信号唤醒 beep_ctrl 线程。
 *
 * @note    信号在锁外发送,避免持锁等待对方持锁导致死锁。
 */
static void beep_sw_cb(lv_event_t *e)
{
    pthread_mutex_lock(&status_mutex);
    BEEP_status = !BEEP_status;
    uint8_t cur_status = BEEP_status;
    pthread_mutex_unlock(&status_mutex);
    if(cur_status == ON)
    {
        pthread_mutex_lock(&beep_mutex);
        pthread_cond_signal(&beep_cond);
        pthread_mutex_unlock(&beep_mutex);
    }
    return;
}

/**
 * @brief   LED 开关回调
 * @param   e  LVGL 事件对象
 *
 * @details 切换 LED_status:
 *          1. 加锁翻转 LED_status;
 *          2. 若新状态为 ON,发送 led_cond 信号唤醒 led_ctrl 线程。
 *
 * @note    信号在锁外发送,避免持锁等待对方持锁导致死锁。
 */
static void led_sw_cb(lv_event_t *e)
{
    pthread_mutex_lock(&status_mutex);
    LED_status = !LED_status;
    uint8_t cur_status = LED_status;
    pthread_mutex_unlock(&status_mutex);
    if(cur_status == ON)
    {
        pthread_mutex_lock(&led_mutex);
        pthread_cond_signal(&led_cond);
        pthread_mutex_unlock(&led_mutex);
    }
    return;
}

/*====================================================*
 *  配置文件读写
 *====================================================*/
/**
 * @brief   保存 MQTT 配置到 ./config.txt
 *
 * @details 文件格式:
 *          port:<端口>
 *          id:<设备ID>
 *          ip:<服务器地址>
 *
 * @note    文件以 "w" 模式打开,会覆盖旧内容。
 *          打开失败时仅打印日志,不抛出异常。
 */
void write_config(void)
{
    FILE *fp = fopen("./config.txt", "w");
    if(fp == NULL)
    {
        printf("write config file failed\n");
        return;
    }
    fprintf(fp, "port:%hu\n", cfg_port);
    fprintf(fp, "id:%s\n", cfg_device_id);
    fprintf(fp, "ip:%s\n", cfg_mqtt_server);
    fclose(fp);
}

/**
 * @brief   从 ./config.txt 读取 MQTT 配置
 *
 * @details 按行读取,根据前缀(port/id/ip)解析到对应变量。
 *          文件不存在时使用代码内默认值。
 *
 * @note    sscanf("%hu", ...) 读取端口,%s 读取字符串,
 *          缓冲区大小 128,正常配置不会溢出。
 */
void read_config(void)
{
    FILE *fp = fopen("./config.txt", "r");
    if(fp == NULL)
    {
        printf("read config file failed\n");
        return;
    }
    char line[128];
    while(fgets(line, sizeof(line), fp))
    {
        if(strstr(line, "port"))
        {
            sscanf(line, "port:%hu", &cfg_port);
        }
        else if(strstr(line, "id"))
        {
            sscanf(line, "id:%s", cfg_device_id);
        }
        else if(strstr(line, "ip"))
        {
            sscanf(line, "ip:%s", cfg_mqtt_server);
        }
    }
    fclose(fp);
}

/**
 * @brief   读取阈值配置从 ./threshold.txt
 *
 * @details 文件格式:
 *          temp:<温度阈值>
 *          humi:<湿度阈值>
 *          按行读取解析,加锁写入共享阈值变量。
 *          仅当 tmp_temp>0 或 tmp_humi>0 时才更新(避免 0 覆盖默认值)。
 *
 * @note    文件不存在时使用代码内默认值(35.0/50.0)。
 */
void threshold_config(void)
{
    FILE *fp = fopen("./threshold.txt", "r");
    if(fp == NULL)
    {
        printf("read threshold config file failed\n");
        return;
    }
    char line[128];
    float tmp_temp = 0, tmp_humi = 0;
    while(fgets(line, sizeof(line), fp))
    {
        if(strstr(line, "temp"))
        {
            sscanf(line, "temp:%f", &tmp_temp);
        }
        else if(strstr(line, "humi"))
        {
            sscanf(line, "humi:%f", &tmp_humi);
        }
    }
    fclose(fp);

    /* 加锁写入共享阈值(仅更新有效值) */
    if(tmp_temp > 0 || tmp_humi > 0)
    {
        pthread_rwlock_wrlock(&threshold_rwlock);
        if(tmp_temp > 0) temp_threshold = tmp_temp;
        if(tmp_humi > 0) humi_threshold = tmp_humi;
        pthread_rwlock_unlock(&threshold_rwlock);
    }
}

/**
 * @brief   保存阈值配置到 ./threshold.txt
 *
 * @details 加锁读取当前阈值,以 "w+" 模式写入文件。
 *          文件格式:
 *          temp:<温度阈值>
 *          humi:<湿度阈值>
 *
 * @note    打开失败时仅打印日志,不抛出异常。
 */
void write_threshold_config(void)
{
    pthread_rwlock_rdlock(&threshold_rwlock);
    float cur_temp = temp_threshold;
    float cur_humi = humi_threshold;
    pthread_rwlock_unlock(&threshold_rwlock);

    FILE *fp = fopen("./threshold.txt", "w+");
    if(fp == NULL)
    {
        printf("write threshold config file failed\n");
        return;
    }
    fprintf(fp, "temp:%f\n", cur_temp);
    fprintf(fp, "humi:%f\n", cur_humi);
    fclose(fp);
}

/*====================================================*
 *  设置页
 *====================================================*/
/**
 * @brief   创建设置页
 * @param   parent  父页面容器(pages[PAGE_SETTINGS])
 *
 * @details 工作流程:
 *          1. 调用 read_config/threshold_config 加载持久化配置;
 *          2. 创建主卡片(白色圆角阴影);
 *          3. 添加标题 "System Configuration";
 *          4. 添加 MQTT 服务器/端口/设备 ID 显示行(只读);
 *          5. 创建 Reset / Save 按钮;
 *          6. 添加配置状态标签;
 *          7. 创建 Up / Down 阈值调节按钮;
 *          8. 创建 BEEP / LED 开关(switch 控件,初始状态由共享变量决定);
 *          9. 创建 Temp / Humi 阈值切换按钮(显示当前阈值)。
 *
 * @note    控件句柄保存到 static 变量,供 display_threshold 后续刷新。
 */
void create_settings_page(lv_obj_t *parent)
{
    /* 加载持久化配置(文件不存在时使用默认值) */
    read_config();
    threshold_config();

    /* 创建设置页主卡片容器 */
    card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, SCR_W - 16, CONTENT_H - 16);
    lv_obj_set_pos(card, 8, 8);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 卡片标题 */
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "System Configuration");
    lv_obj_set_style_text_font(title, FONT_CJK, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_DARK, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* MQTT服务器配置行(只读显示) */
    lv_obj_t *lbl_server = lv_label_create(card);
    lv_label_set_text(lbl_server, "MQTT Server:");
    lv_obj_set_style_text_font(lbl_server, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_server, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(lbl_server, 30, 60);

    cfg_mqtt_server_lbl = lv_label_create(card);
    lv_label_set_text(cfg_mqtt_server_lbl, cfg_mqtt_server);
    lv_obj_set_style_text_font(cfg_mqtt_server_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(cfg_mqtt_server_lbl, COLOR_TEXT_DARK, 0);
    lv_obj_set_pos(cfg_mqtt_server_lbl, 150, 60);

    /* 端口号配置行(只读显示) */
    lv_obj_t *lbl_port = lv_label_create(card);
    lv_label_set_text(lbl_port, "Port:");
    lv_obj_set_style_text_font(lbl_port, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_port, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(lbl_port, 30, 100);

    cfg_port_lbl = lv_label_create(card);
    lv_label_set_text_fmt(cfg_port_lbl, "%u", cfg_port);
    lv_obj_set_style_text_font(cfg_port_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(cfg_port_lbl, COLOR_TEXT_DARK, 0);
    lv_obj_set_pos(cfg_port_lbl, 150, 100);

    /* 设备ID配置行(只读显示) */
    lv_obj_t *lbl_device = lv_label_create(card);
    lv_label_set_text(lbl_device, "Device ID:");
    lv_obj_set_style_text_font(lbl_device, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_device, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(lbl_device, 30, 140);

    cfg_device_id_lbl = lv_label_create(card);
    lv_label_set_text(cfg_device_id_lbl, cfg_device_id);
    lv_obj_set_style_text_font(cfg_device_id_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(cfg_device_id_lbl, COLOR_TEXT_DARK, 0);
    lv_obj_set_pos(cfg_device_id_lbl, 150, 140);

    /*恢复默认按钮(灰色背景)*/
    lv_obj_t *btn_reset = lv_btn_create(card);
    lv_obj_set_size(btn_reset, 120, 40);
    lv_obj_set_pos(btn_reset, 330, 260);
    lv_obj_set_style_radius(btn_reset, 6, 0);
    lv_obj_set_style_shadow_width(btn_reset, 0, 0);
    lv_obj_set_style_border_width(btn_reset, 0, 0);
    lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x888888), 0);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset");
    lv_obj_set_style_text_font(lbl_reset, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_reset, lv_color_white(), 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, settings_reset_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 保存配置按钮(蓝色背景) */
    lv_obj_t *btn_save = lv_btn_create(card);
    lv_obj_set_size(btn_save, 120, 40);
    lv_obj_set_pos(btn_save, 200, 260);
    lv_obj_set_style_radius(btn_save, 6, 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0);
    lv_obj_set_style_bg_color(btn_save, COLOR_NAV_ACTIVE, 0);

    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_set_style_text_font(lbl_save, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_save, lv_color_white(), 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, settings_save_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 配置状态标签(显示保存状态) */
    cfg_status_lbl = lv_label_create(card);
    lv_label_set_text(cfg_status_lbl, cfg_saved ? "已保存" : "未保存");
    lv_obj_set_style_text_font(cfg_status_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(cfg_status_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(cfg_status_lbl, 200, 320);

    /* 增加阈值按钮(绿色 Up) */
    lv_obj_t *btn_inc_thresh = lv_btn_create(card);
    lv_obj_set_size(btn_inc_thresh, 50, 30);
    lv_obj_set_pos(btn_inc_thresh, 550, 300);
    lv_obj_set_style_radius(btn_inc_thresh, 6, 0);
    lv_obj_set_style_shadow_width(btn_inc_thresh, 0, 0);
    lv_obj_set_style_border_width(btn_inc_thresh, 0, 0);
    lv_obj_set_style_bg_color(btn_inc_thresh, lv_color_hex(0x00AA00), 0);

    lv_obj_t *lbl_inc_thresh = lv_label_create(btn_inc_thresh);
    lv_label_set_text(lbl_inc_thresh, "Up");
    lv_obj_set_style_text_font(lbl_inc_thresh, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_inc_thresh, lv_color_white(), 0);
    lv_obj_center(lbl_inc_thresh);
    lv_obj_add_event_cb(btn_inc_thresh, settings_inc_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 降低阈值按钮(绿色 Down) */
    lv_obj_t *btn_dec_thresh = lv_btn_create(card);
    lv_obj_set_size(btn_dec_thresh, 50, 30);
    lv_obj_set_pos(btn_dec_thresh, 650, 300);
    lv_obj_set_style_radius(btn_dec_thresh, 6, 0);
    lv_obj_set_style_shadow_width(btn_dec_thresh, 0, 0);
    lv_obj_set_style_border_width(btn_dec_thresh, 0, 0);
    lv_obj_set_style_bg_color(btn_dec_thresh, lv_color_hex(0x00AA00), 0);

    lv_obj_t *lbl_dec_thresh = lv_label_create(btn_dec_thresh);
    lv_label_set_text(lbl_dec_thresh, "Down");
    lv_obj_set_style_text_font(lbl_dec_thresh, FONT_CJK, 0);
    lv_obj_set_style_text_color(lbl_dec_thresh, lv_color_white(), 0);
    lv_obj_center(lbl_dec_thresh);
    lv_obj_add_event_cb(btn_dec_thresh, settings_dec_btn_cb, LV_EVENT_CLICKED, NULL);

    /*蜂鸣器开关(switch 控件)*/
    lv_obj_t *beep_lbl = lv_label_create(card);
    lv_label_set_text(beep_lbl, "BEEP:");
    lv_obj_set_style_text_font(beep_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(beep_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(beep_lbl, 30, 180);

    beep_sw = lv_switch_create(card);
    lv_obj_set_pos(beep_sw, 100, 180);
    lv_obj_set_size(beep_sw, 60, 30);
    lv_obj_set_style_bg_color(beep_sw, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(beep_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(beep_sw, 15, LV_PART_MAIN);
    lv_obj_set_style_bg_color(beep_sw, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(beep_sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(beep_sw, 12, LV_PART_KNOB);
    lv_obj_set_style_pad_all(beep_sw, 2, LV_PART_KNOB);
    lv_obj_add_state(beep_sw, BEEP_status ? LV_STATE_CHECKED : LV_STATE_DEFAULT);
    lv_obj_add_event_cb(beep_sw, beep_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /*LED开关(switch 控件)*/
    lv_obj_t *led_lbl = lv_label_create(card);
    lv_label_set_text(led_lbl, "LED:");
    lv_obj_set_style_text_font(led_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(led_lbl, COLOR_TEXT_GRAY, 0);
    lv_obj_set_pos(led_lbl, 30, 220);

    led_sw = lv_switch_create(card);
    lv_obj_set_pos(led_sw, 100, 220);
    lv_obj_set_size(led_sw, 60, 30);
    lv_obj_set_style_bg_color(led_sw, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(led_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(led_sw, 15, LV_PART_MAIN);
    lv_obj_set_style_bg_color(led_sw, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(led_sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(led_sw, 12, LV_PART_KNOB);
    lv_obj_set_style_pad_all(led_sw, 2, LV_PART_KNOB);
    lv_obj_add_state(led_sw, LED_status ? LV_STATE_CHECKED : LV_STATE_DEFAULT);
    lv_obj_add_event_cb(led_sw, led_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ========== 温度阈值选择按钮模块 ========== */
    /* 创建温度阈值切换按钮 */
    btn_sel_temp = lv_btn_create(card);
    lv_obj_set_size(btn_sel_temp, 100, 30);
    lv_obj_set_pos(btn_sel_temp, 570, 210);
    lv_obj_set_style_radius(btn_sel_temp, 6, 0);
    lv_obj_set_style_shadow_width(btn_sel_temp, 0, 0);
    lv_obj_set_style_border_width(btn_sel_temp, 0, 0);
    /* 默认颜色为导航激活色,选中时切换为绿色 */
    lv_obj_set_style_bg_color(btn_sel_temp, COLOR_NAV_ACTIVE, 0);
    if(threshold_mode==1) lv_obj_set_style_bg_color(btn_sel_temp, lv_color_hex(0x00AA00), 0);

    /* 创建温度阈值标签并显示当前温度值 */
    temp_thresh_lbl = lv_label_create(btn_sel_temp);
    pthread_rwlock_rdlock(&threshold_rwlock);
    float threshold = temp_threshold;
    pthread_rwlock_unlock(&threshold_rwlock);
    lv_label_set_text_fmt(temp_thresh_lbl, "Temp: %.1f", threshold);
    lv_obj_set_style_text_font(temp_thresh_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(temp_thresh_lbl, lv_color_white(), 0);
    lv_obj_center(temp_thresh_lbl);
    /* 绑定温度阈值切换事件回调 */
    lv_obj_add_event_cb(btn_sel_temp, settings_sel_temp_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 创建湿度阈值切换按钮 */
    btn_sel_humi = lv_btn_create(card);
    lv_obj_set_size(btn_sel_humi, 100, 30);
    lv_obj_set_pos(btn_sel_humi, 570, 250);
    lv_obj_set_style_radius(btn_sel_humi, 6, 0);
    lv_obj_set_style_shadow_width(btn_sel_humi, 0, 0);
    lv_obj_set_style_border_width(btn_sel_humi, 0, 0);
    /* 默认颜色为导航激活色,选中时切换为绿色 */
    lv_obj_set_style_bg_color(btn_sel_humi, COLOR_NAV_ACTIVE, 0);
    if(threshold_mode==0) lv_obj_set_style_bg_color(btn_sel_humi, lv_color_hex(0x00AA00), 0);

    /* 创建湿度阈值标签并显示当前湿度值 */
    humi_thresh_lbl = lv_label_create(btn_sel_humi);
    pthread_rwlock_rdlock(&threshold_rwlock);
    threshold = humi_threshold;
    pthread_rwlock_unlock(&threshold_rwlock);
    lv_label_set_text_fmt(humi_thresh_lbl, "Humi: %.1f", threshold);
    lv_obj_set_style_text_font(humi_thresh_lbl, FONT_CJK, 0);
    lv_obj_set_style_text_color(humi_thresh_lbl, lv_color_white(), 0);
    lv_obj_center(humi_thresh_lbl);
    /* 绑定湿度阈值切换事件回调 */
    lv_obj_add_event_cb(btn_sel_humi, settings_sel_humi_btn_cb, LV_EVENT_CLICKED, NULL);

}

/**
 * @brief   阈值显示刷新(主线程调用)
 *
 * @details 刷新以下控件:
 *          1. 读阈值(加锁),更新 temp_thresh_lbl / humi_thresh_lbl 文本;
 *          2. 读设备状态(加锁),更新 led_sw / beep_sw 选中状态;
 *          3. 根据 threshold_mode 更新 btn_sel_temp / btn_sel_humi 颜色:
 *             - 选中项:绿色(0x00AA00)
 *             - 未选中项:COLOR_NAV_ACTIVE(蓝色)
 *          4. 更新 cfg_status_lbl 保存状态文字。
 *
 * @note    本函数被以下场景调用:
 *          - 设置页按钮回调(用户调节阈值/切换模式后)
 *          - process_pending_events(收到远程命令更新阈值后)
 *          需在主线程调用,不可在持锁状态下调用 LVGL API。
 */
void display_threshold(void)
{
    pthread_rwlock_rdlock(&threshold_rwlock);
    float cur_temp_threshold = temp_threshold;
    float cur_humi_threshold = humi_threshold;
    pthread_rwlock_unlock(&threshold_rwlock);

    pthread_mutex_lock(&status_mutex);
    uint8_t cur_led_status = LED_status;
    uint8_t cur_beep_status = BEEP_status;
    pthread_mutex_unlock(&status_mutex);

    /* 更新温度按键选择状态(选中=绿,未选中=蓝) */
    lv_obj_set_style_bg_color(btn_sel_temp, threshold_mode == 1 ? lv_color_hex(0x00AA00) : COLOR_NAV_ACTIVE, 0);
    /* 更新湿度按键选择状态 */
    lv_obj_set_style_bg_color(btn_sel_humi, threshold_mode == 0 ? lv_color_hex(0x00AA00) : COLOR_NAV_ACTIVE, 0);
    /* 更新温度阈值显示 */
    lv_label_set_text_fmt(temp_thresh_lbl, "Temp: %.1f", cur_temp_threshold);
    /* 更新湿度阈值显示 */
    lv_label_set_text_fmt(humi_thresh_lbl, "Humi: %.1f", cur_humi_threshold);

    /* 更新配置保存状态 */
    lv_label_set_text(cfg_status_lbl, cfg_saved ? "已保存" : "未保存");
    /* 更新LED开关状态 */
    lv_obj_clear_state(led_sw, LV_STATE_CHECKED);
    lv_obj_add_state(led_sw, cur_led_status == ON ? LV_STATE_CHECKED : LV_STATE_DEFAULT);
    /* 更新BEEP开关状态 */
    lv_obj_clear_state(beep_sw, LV_STATE_CHECKED);
    lv_obj_add_state(beep_sw, cur_beep_status == ON ? LV_STATE_CHECKED : LV_STATE_DEFAULT);
}
