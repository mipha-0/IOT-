#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 IOT 数据监控平台项目验收表 Word 文档"""

from docx import Document
from docx.shared import Pt, Inches, RGBColor, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
import os

doc = Document()

# ---- 全局样式 ----
style = doc.styles['Normal']
style.font.name = '宋体'
style.font.size = Pt(12)
style.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')

def set_cell_font(cell, text, bold=False, size=11, font_name='宋体'):
    cell.text = ''
    p = cell.paragraphs[0]
    run = p.add_run(text)
    run.font.name = font_name
    run.font.size = Pt(size)
    run.bold = bold
    run.element.rPr.rFonts.set(qn('w:eastAsia'), font_name)

def add_heading(text, level=1):
    h = doc.add_heading(text, level=level)
    for run in h.runs:
        run.font.name = '黑体'
        run.element.rPr.rFonts.set(qn('w:eastAsia'), '黑体')
        if level == 0:
            run.font.size = Pt(22)
        elif level == 1:
            run.font.size = Pt(16)
        elif level == 2:
            run.font.size = Pt(14)
    return h

def add_para(text, bold=False, size=12, indent=True):
    p = doc.add_paragraph()
    if indent:
        p.paragraph_format.first_line_indent = Pt(24)
    p.paragraph_format.space_after = Pt(6)
    p.paragraph_format.line_spacing = 1.5
    run = p.add_run(text)
    run.font.name = '宋体'
    run.font.size = Pt(size)
    run.bold = bold
    run.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')
    return p

def add_bullet(text, size=12):
    p = doc.add_paragraph(style='List Bullet')
    p.paragraph_format.line_spacing = 1.5
    run = p.add_run(text)
    run.font.name = '宋体'
    run.font.size = Pt(size)
    run.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')
    return p

def add_table(headers, rows):
    table = doc.add_table(rows=1 + len(rows), cols=len(headers))
    table.style = 'Table Grid'
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        set_cell_font(table.rows[0].cells[i], h, bold=True, size=11)
    for r, row in enumerate(rows):
        for c, val in enumerate(row):
            set_cell_font(table.rows[r+1].cells[c], str(val), bold=False, size=10)
    return table

# ===================== 封面 =====================
title_p = doc.add_paragraph()
title_p.alignment = WD_ALIGN_PARAGRAPH.CENTER
title_p.paragraph_format.space_before = Pt(120)
title_run = title_p.add_run('IOT 数据监控平台\n项目验收报告')
title_run.font.name = '黑体'
title_run.font.size = Pt(28)
title_run.bold = True
title_run.element.rPr.rFonts.set(qn('w:eastAsia'), '黑体')

info_p = doc.add_paragraph()
info_p.alignment = WD_ALIGN_PARAGRAPH.CENTER
info_p.paragraph_format.space_before = Pt(60)
info_lines = [
    '项目名称：IOT 数据监控平台',
    '技术栈：ARM Linux + LVGL v9 + Pthread + cJSON',
    '运行平台：ARM 嵌入式开发板（800×480 framebuffer）',
    '日　　期：2026 年 8 月',
]
for line in info_lines:
    run = info_p.add_run(line + '\n')
    run.font.name = '宋体'
    run.font.size = Pt(14)
    run.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')

doc.add_page_break()

# ===================== 目录页 =====================
add_heading('目　录', level=1)
toc_items = [
    '一、设计目标',
    '二、功能描述',
    '三、设计方案',
    '四、系统框架',
    '五、实现过程',
    '六、心得体会',
]
for item in toc_items:
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(8)
    run = p.add_run(item)
    run.font.name = '宋体'
    run.font.size = Pt(14)
    run.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')

doc.add_page_break()

# ===================== 一、设计目标 =====================
add_heading('一、设计目标', level=1)

add_heading('1.1 项目背景', level=2)
add_para('随着物联网技术的快速发展，嵌入式终端在工业监控、环境监测、智能家居等领域的应用日益广泛。本项目旨在设计并实现一个基于 ARM Linux 平台的 IOT 数据监控终端，通过图形化界面实时展示环境温湿度数据，并具备阈值告警、历史数据追溯、远程通信等核心能力，为物联网数据监控场景提供一套完整可行的嵌入式解决方案。')

add_heading('1.2 设计目标', level=2)
add_bullet('实时数据采集：通过心知天气 API 获取广州地区实时温湿度数据，每 5 秒自动刷新，数据更新后立即同步至 UI 界面和本地存储。')
add_bullet('图形化触控界面：基于 LVGL v9 图形库在 800×480 framebuffer 显示屏上渲染多页面触控 UI，包含主页、事件页、历史页、设置页四个功能页面，支持底部导航栏切换。')
add_bullet('阈值告警系统：当温度或湿度超过用户设定的阈值时，自动触发 LED 闪烁和蜂鸣器告警，LED 闪烁频率随温度动态变化，实现可视化预警。')
add_bullet('本地数据持久化：将采集到的温湿度数据以"时间,温度,湿度"格式写入本地文件 data.txt，历史页面可实时查看最近 20 条记录，最新数据在顶部显示。')
add_bullet('远程 TCP 通信：通过 TCP 客户端连接远程服务器，实现双向数据交互——上行定时上报温湿度及设备状态，下行接收远程控制指令（LED 开关、蜂鸣器开关、阈值调整）。')
add_bullet('多线程并发安全：采用 pthread 多线程架构，通过读写锁、互斥锁、条件变量等同步机制保证共享数据的线程安全，通过 pending event 机制确保 LVGL 控件仅在主线程操作。')

# ===================== 二、功能描述 =====================
add_heading('二、功能描述', level=1)

add_heading('2.1 温湿度数据采集', level=2)
add_para('系统通过后台线程每 5 秒调用心知天气 API 获取广州地区的实时温度和湿度数据。采用裸 socket 手动构造 HTTP GET 请求，通过 DNS 解析 api.seniverse.com 域名，建立 TCP 连接后发送请求并接收 JSON 格式的响应数据，使用 cJSON 库解析提取温度和湿度字段，写入共享变量供其他模块使用。')

add_heading('2.2 实时数据展示', level=2)
add_para('主页采用左右分栏布局：左侧显示温度和湿度两个数据卡片，数值用大号字体展示，温度超过阈值时标签变红色告警，正常时为绿色；右侧为实时曲线图，采用 lv_chart 控件绘制 30 个数据点的折线图，支持温度（0-50）和湿度（0-100）双 Y 轴显示。主线程每 2 秒通过 lv_timer 定时刷新主页数据和曲线。')

add_heading('2.3 数据持久化与历史查询', level=2)
add_para('采集到的温湿度数据以"时间,温度,湿度"格式追加写入本地 data.txt 文件。持久化线程采用事件驱动模式：通过条件变量阻塞等待，仅在 get_temp_humi 线程获取到新数据并发送信号后才执行写入操作，并自动跳过重复数据，避免无效写入。历史页面在创建时从 data.txt 读取最近 20 条记录填充表格，后续每获取一次新数据即通过 pending event 机制触发主线程更新表格，最新数据始终显示在第一行。')

add_heading('2.4 阈值告警控制', level=2)
add_para('LED 控制线程和蜂鸣器控制线程各自独立运行，采用条件变量实现按需唤醒机制。当 LED/BEEP 被关闭时，线程阻塞睡眠不消耗 CPU 资源；被开启时通过条件变量信号唤醒。LED 控制逻辑分为三种状态：关闭时全部熄灭；温度达到或超过阈值时四灯常亮；未超阈值时四灯同步闪烁，闪烁频率随温度升高而加快（温度从 0 到阈值时，闪烁间隔从 1 秒缩短至 50ms）。蜂鸣器在开启状态下，当温度或湿度超过阈值时间歇告警（响 1 秒停 1 秒）。')

add_heading('2.5 远程 TCP 通信', level=2)
add_para('TCP 客户端线程连接远程服务器（192.168.44.21:8888），连接失败时每 5 秒自动重试。连接成功后启动收发两个子线程：接收线程循环接收服务器下发的 JSON 命令，解析命令名称和参数，支持 LED 开关、蜂鸣器开关、温度阈值、湿度阈值四类远程控制指令，通过 pending event 机制通知主线程更新事件页和设置页显示；发送线程每 5 秒读取当前温湿度、阈值及设备状态，组装 JSON 数据包上报至服务器。')

add_heading('2.6 系统设置与配置管理', level=2)
add_para('设置页面提供 MQTT 配置信息显示（服务器地址、端口、设备 ID，默认为华为云 IoT 平台配置）、温度/湿度阈值调节（支持温度/湿度模式切换，每次步进 0.5）、LED 和蜂鸣器开关控制、配置保存与重置功能。配置数据存储在 config.txt（端口、设备 ID、IP 地址）和 threshold.txt（温度阈值、湿度阈值）两个纯文本文件中。')

add_heading('2.7 事件记录与统计', level=2)
add_para('事件页面以表格形式记录系统运行过程中的各类事件，包括三类：告警事件（温度/湿度超阈值触发）、命令事件（远程 TCP 指令到达）、系统事件（TCP 连接成功/失败）。右侧设有三个统计卡片，分别对三类事件进行计数统计，实时反映系统运行状态。')

# ===================== 三、设计方案 =====================
add_heading('三、设计方案', level=1)

add_heading('3.1 技术选型', level=2)
add_table(
    ['维度', '技术选型', '选型理由'],
    [
        ['处理器平台', 'ARM Linux', '资源丰富、支持多线程和网络栈，适合 IOT 场景'],
        ['图形库', 'LVGL v9.0.0-dev', '专为嵌入式设计的轻量级 GUI 库，支持 framebuffer 直驱'],
        ['显示输出', 'Linux framebuffer (/dev/fb0)', '32 位色深，800×480 分辨率，直接操作显示内存'],
        ['输入设备', 'evdev (/dev/event)', 'Linux 标准输入框架，支持触摸屏和鼠标'],
        ['交叉编译', 'arm-linux-gcc', '标准的 ARM 交叉编译工具链'],
        ['多线程', 'POSIX pthread', 'Linux 原生线程支持，配合互斥锁和条件变量实现同步'],
        ['JSON 解析', 'cJSON', '轻量级 C 语言 JSON 库，适合嵌入式环境'],
        ['网络通信', 'TCP Socket', '原生 socket 编程，手动构造 HTTP 和 JSON 协议'],
    ]
)

add_heading('3.2 线程架构设计', level=2)
add_para('系统采用多线程并发架构，共创建 5 个后台 pthread 和 3 个 LVGL 定时器，主线程运行 LVGL 事件循环：')
add_table(
    ['线程/定时器', '类型', '功能', '同步机制'],
    [
        ['get_temp_humi', 'pthread', '天气 API 数据采集（5s 轮询）', 'th_rwlock 写锁 + cond_var 信号'],
        ['write_data_to_file', 'pthread', '数据持久化（事件驱动）', 'cond_var 阻塞等待 + th_rwlock 读锁'],
        ['led_ctrl', 'pthread', 'LED 控制（按需唤醒）', 'led_cond 条件变量 + th_rwlock/threshold_rwlock 读锁'],
        ['beep_ctrl', 'pthread', '蜂鸣器控制（按需唤醒）', 'beep_cond 条件变量 + th_rwlock/threshold_rwlock 读锁'],
        ['client', 'pthread', 'TCP 客户端（含收发子线程）', 'status_mutex 保护状态变量'],
        ['mock_data_update', 'lv_timer(2s)', '主页数据与曲线刷新', '主线程执行，直接操作 LVGL'],
        ['process_pending_events', 'lv_timer(200ms)', '消费 pending event 队列', '主线程执行，pending_mutex 保护队列'],
        ['时间标签刷新', 'lv_timer(1s)', '导航栏时间显示', '主线程执行'],
    ]
)

add_heading('3.3 线程安全设计', level=2)
add_para('LVGL 图形库非线程安全，后台线程不能直接操作 UI 控件。本项目设计了 pending event 机制解决此问题：后台线程通过 push_pending_event() 将事件文本推入线程安全队列（mutex 保护），主线程 200ms 定时器检测队列并调用 update_event_page() 在主线程安全更新 UI。同理，阈值刷新和历史数据刷新也通过标志位机制由主线程定时检测处理。')
add_para('共享状态变量的同步策略按数据特性区分：温湿度和阈值数据使用读写锁（允许多读者并发，写者独占），状态变量（LED/BEEP/连接状态）使用互斥锁保护，数据更新通知和 LED/BEEP 唤醒使用条件变量实现事件驱动。')

add_heading('3.4 文件结构设计', level=2)
add_table(
    ['目录/文件', '职责'],
    [
        ['src/app/main.c', '程序入口，初始化 LVGL/驱动/线程'],
        ['src/app/ui_manager.c', 'UI 框架管理，页面切换逻辑'],
        ['src/data/data_source.c', '数据采集与持久化'],
        ['src/net/network.c', 'TCP 通信与 HTTP 天气请求'],
        ['src/drivers/hw_ctrl.c', 'LED 与蜂鸣器硬件控制'],
        ['src/ui/ui_common.c/h', '共享状态、同步原语、样式定义'],
        ['src/ui/ui_navbar.c', '底部导航栏'],
        ['src/ui/ui_page_home.c', '主页（温湿度卡片+曲线图）'],
        ['src/ui/ui_page_event.c', '事件页（事件表格+统计卡片）'],
        ['src/ui/ui_page_history.c', '历史页（数据文件读取+实时更新）'],
        ['src/ui/ui_page_settings.c', '设置页（阈值调节+开关+配置）'],
        ['lvgl/', 'LVGL v9 图形库源码'],
        ['lv_drivers/', 'LVGL 显示/输入驱动（fbdev/evdev）'],
    ]
)

# ===================== 四、系统框架 =====================
add_heading('四、系统框架', level=1)

add_heading('4.1 系统整体架构', level=2)
add_para('系统采用分层架构设计，自底向上分为硬件层、驱动层、业务逻辑层、UI 界面层和应用层：')

add_table(
    ['层次', '模块', '说明'],
    [
        ['应用层', 'main.c + ui_manager', '程序入口、线程启动、UI 框架创建、页面切换'],
        ['UI 界面层', 'ui_page_home/event/history/settings + ui_navbar', '4 个功能页面 + 导航栏，LVGL 控件渲染与交互'],
        ['业务逻辑层', 'data_source + network', '数据采集、持久化、TCP 通信、HTTP 天气请求'],
        ['驱动层', 'hw_ctrl + fbdev + evdev', 'LED/BEEP 硬件控制、显示驱动、输入驱动'],
        ['硬件层', '/dev/fb0, /dev/Led, /dev/beep, /dev/event', 'Linux 设备文件接口'],
    ]
)

add_heading('4.2 数据流设计', level=2)
add_para('系统的核心数据流如下：')
add_bullet('天气数据流：心知天气 API → get_temp_humi 线程（HTTP 请求+JSON 解析）→ 共享变量(mock_temp/mock_humi, 读写锁保护) → mock_data_update 定时器(主页显示+曲线图) / write_data_to_file 线程(写入 data.txt) / request_history_refresh(历史页更新) / led_ctrl+beep_ctrl(阈值判断告警)')
add_bullet('TCP 下行数据流：远程服务器 → client 线程 recv → cJSON 解析命令 → status_mutex 加锁写入状态变量 → pending event 通知主线程 → update_event_page(事件页) / display_threshold(设置页)')
add_bullet('TCP 上行数据流：共享变量(温湿度/阈值/状态, 加锁读取) → send_news 线程(5s 定时) → cJSON 组包 → TCP send → 远程服务器')
add_bullet('本地存储数据流：get_temp_humi 新数据 → cond_var 信号 → write_data_to_file 线程唤醒 → fprintf 写入 data.txt → 历史页从文件读取初始数据')

add_heading('4.3 共享状态与同步机制', level=2)
add_table(
    ['共享数据', '数据类型', '同步方式', '写入方', '读取方'],
    [
        ['mock_temp / mock_humi', 'float', '读写锁 th_rwlock', 'get_temp_humi', 'mock_data_update/write_data/led_ctrl/beep_ctrl/send_news'],
        ['temp_threshold / humi_threshold', 'float', '读写锁 threshold_rwlock', 'settings/network', 'led_ctrl/beep_ctrl/mock_data_update/send_news'],
        ['LED_status / BEEP_status', 'int(ON/OFF)', '互斥锁 status_mutex', 'settings/network', 'led_ctrl/beep_ctrl/send_news/display_threshold'],
        ['connect_event / device_status', 'int(ON/OFF)', '互斥锁 status_mutex', 'network', 'update_event_page/send_news'],
        ['数据更新通知', '—', '条件变量 cond_var+cond_mutex', 'get_temp_humi(signal)', 'write_data_to_file(wait)'],
        ['LED 唤醒信号', '—', '条件变量 led_cond+led_mutex', 'settings/network(signal)', 'led_ctrl(wait)'],
        ['BEEP 唤醒信号', '—', '条件变量 beep_cond+beep_mutex', 'settings/network(signal)', 'beep_ctrl(wait)'],
    ]
)

# ===================== 五、实现过程 =====================
add_heading('五、实现过程', level=1)

add_heading('5.1 开发环境搭建', level=2)
add_para('开发环境基于 Windows 主机 + ARM 交叉编译工具链。使用 arm-linux-gcc 进行交叉编译，Makefile 中配置了 LVGL 源码编译、lv_drivers 驱动编译、freetype 字体库链接、源文件编译和最终链接等完整构建流程。通过 lv_conf.h 配置 LVGL 的显示分辨率（800×480）、色深（32 位）、字体（含 CJK 中文字体）、DPI 等参数。lv_drv_conf.h 配置 framebuffer 显示驱动和 evdev 输入驱动的设备路径。')

add_heading('5.2 LVGL 显示与输入驱动集成', level=2)
add_para('在 main.c 中初始化 LVGL 核心：调用 lv_init() 初始化 LVGL 内部状态，通过 lv_display_create() 创建显示对象并注册 framebuffer 刷新回调函数 fbdev_flush()，设置显示分辨率为 800×480。通过 lv_evdev_create() 创建触摸/鼠标输入设备，绑定 /dev/event 设备文件。初始化完成后，主线程进入 lv_timer_handler() 循环，以约 5ms 间隔持续调用以驱动 LVGL 事件处理。')

add_heading('5.3 UI 界面实现', level=2)
add_para('UI 框架由 ui_manager.c 统一管理：创建顶部导航栏（50px 高，深蓝底色，左侧标题"embedded system"，右侧 4 个导航按钮）和 800×430 内容区域，4 个页面容器默认全部隐藏，通过 switch_page() 函数切换 LV_OBJ_FLAG_HIDDEN 标志实现页面切换，同时高亮当前导航按钮。')
add_para('主页创建温度和湿度数据卡片（左侧上下排列）和 lv_chart 折线图（右侧），曲线图设置 30 个数据点、LINE 类型、双 Y 轴。主页 lv_timer 每 2 秒读取共享温湿度数据，更新数值标签文本和颜色（超阈值红色/正常绿色），并通过 lv_chart_set_next_value() 推入新数据点。')
add_para('事件页创建 30 行单列表格和 3 个统计卡片，update_event_page() 函数根据事件类型（alarm/command/system）追加表格行并更新对应统计卡片数值。')
add_para('历史页创建时从 data.txt 读取最近 20 条记录，使用环形缓冲算法处理文件行数超过 20 条的情况。实时更新采用 pending event 机制：get_temp_humi 获取新数据后调用 request_history_refresh() 设置标志位，主线程 200ms 定时器检测到后调用 history_page_on_new_data() 在主线程安全更新表格，新数据插入第 0 行（最上方）。')
add_para('设置页提供 MQTT 配置信息显示、阈值调节按钮（温度/湿度模式切换，步进 0.5）、LED 和蜂鸣器开关控件、配置保存与重置按钮。配置读写通过 fopen/fprintf/sscanf 操作 config.txt 和 threshold.txt 纯文本文件。')

add_heading('5.4 数据采集与持久化实现', level=2)
add_para('get_temp_humi() 线程每 5 秒执行一次数据采集：调用 gethostbyname() 解析天气 API 域名，创建 TCP socket 连接 80 端口，手动构造 HTTP GET 请求字符串（包含 API Key、城市参数、语言和单位），通过 send() 发送后循环 recv() 接收响应。先逐字节接收直到找到 "\\r\\n\\r\\n" 分隔符跳过 HTTP 头部，解析 Content-Length 获取 JSON 正文长度，再按长度接收完整 JSON 正文。使用 cJSON_Parse() 解析 JSON，提取 results[0].now.temperature 和 humidity 字段，通过 atof() 转换为 float 后写入共享变量。HTTP 请求在读写锁外部执行，避免网络 IO 阻塞其他读取者。')
add_para('write_data_to_file() 线程采用事件驱动模式：通过 pthread_cond_wait() 阻塞等待 cond_var 条件变量，仅在 get_temp_humi 线程获取到新数据并发送信号后被唤醒。唤醒后读取当前温湿度，获取系统时间，格式化为"时间,温度,湿度"写入 data.txt，并自动跳过与上次相同的数据避免重复记录。')

add_heading('5.5 硬件控制实现', level=2)
add_para('LED 控制线程打开 /dev/Led 设备文件，通过 ioctl() 系统调用控制 4 个 LED（LED1-LED4），命令码使用 _IO(TEST_MAGIC, 0~3) 宏定义。线程在 LED_status 为 OFF 时调用 pthread_cond_wait() 阻塞睡眠，被唤醒后根据当前温度和阈值的关系执行不同控制逻辑：温度达到或超过阈值时四灯常亮，未超阈值时四灯同步闪烁，闪烁间隔根据温度/阈值比值动态计算（delay = 1000000 - 950000 * ratio，温度越高闪烁越快）。')
add_para('蜂鸣器控制线程打开 /dev/beep 设备文件，控制逻辑类似：OFF 状态时条件变量阻塞，ON 状态时检查温度和湿度是否超过阈值，超限时蜂鸣器响 1 秒停 1 秒间歇告警，未超限时休眠 1 秒避免空循环。')

add_heading('5.6 TCP 通信实现', level=2)
add_para('client() 线程创建 TCP socket 连接服务器，连接失败时每 5 秒重试。连接成功后启动接收和发送两个子线程。接收线程循环 recv() 接收数据，使用 cJSON 解析 JSON 命令，提取 command_name 和 paras 字段，根据命令名称分发处理：LED 命令更新 LED_status 并发送 led_cond 信号，蜂鸣器命令更新 BEEP_status，温度阈值和湿度阈值命令更新对应阈值变量。所有状态变量写入均通过 status_mutex 加锁保护。接收到的命令通过 push_pending_event() 推入事件队列，由主线程定时消费更新 UI。发送线程每 5 秒读取当前温湿度、阈值和设备状态，使用 cJSON 组装 JSON 上报包发送至服务器。')

add_heading('5.7 Bug 修复与优化', level=2)
add_para('在开发过程中发现并修复了多个关键问题：')
add_bullet('LVGL 线程安全：后台线程（TCP 接收、历史数据更新）原本直接调用 LVGL 控件操作函数，可能导致界面崩溃。通过引入 pending event 队列 + 主线程定时消费机制修复。')
add_bullet('共享变量数据竞争：LED_status、BEEP_status、connect_event 等状态变量在多线程间无锁读写。新增 status_mutex 互斥锁统一保护。')
add_bullet('cJSON 空指针崩溃：get_news() 中 cJSON_GetObjectItem 返回值未检查 NULL，收到非预期 JSON 时可能空指针崩溃。增加全链路 NULL 检查和 cJSON_Delete 释放。')
add_bullet('HTTP 请求错误处理：httprequest_weather() 中 connect/calloc/cJSON_Parse 返回值未检查，错误路径 fd 泄漏。增加完整错误检查和资源释放。')
add_bullet('持久化优化：write_data_to_file() 从定时 5 秒写入改为事件驱动（条件变量等待），避免数据无变化时的重复写入。')
add_bullet('阈值精度修复：update_home_data() 中将 threshold 读取为 int 导致小数部分丢失，修正为 float。')
add_bullet('蜂鸣器空循环修复：beep_ctrl() 在温湿度未超阈值时无 sleep 导致 CPU 空转，增加 sleep(1)。')

# ===================== 六、心得体会 =====================
add_heading('六、心得体会', level=1)

add_heading('6.1 嵌入式多线程编程的挑战', level=2)
add_para('本项目最大的技术挑战在于多线程环境下的数据安全与 UI 线程安全。在开发初期，后台线程直接操作 LVGL 控件导致界面随机崩溃，这类问题在嵌入式设备上极难调试——因为崩溃时机不确定，且缺乏完善的调试工具。通过引入 pending event 机制，将所有 UI 操作收敛到主线程的定时器回调中执行，从根本上解决了线程安全问题。这让我深刻理解了"非线程安全库"在多线程环境下的使用范式：不是加锁保护，而是将操作代理到库所属的线程执行。')

add_heading('6.2 同步机制的设计权衡', level=2)
add_para('在同步机制的选择上，我学会了根据数据访问模式选择合适的原语。温湿度和阈值数据"读多写少"，使用读写锁（rwlock）允许多个读者并发，提高吞吐量；状态变量读写频率相当且临界区短，使用互斥锁（mutex）更简单高效；线程唤醒场景使用条件变量（cond）实现事件驱动，避免轮询浪费 CPU。LED 和蜂鸣器控制线程的条件变量阻塞设计让我体会到了"按需唤醒"的价值——设备关闭时线程完全不占用 CPU，这对资源受限的嵌入式平台尤为重要。')

add_heading('6.3 网络编程的实践经验', level=2)
add_para('本项目在没有 HTTP 库的情况下用裸 socket 手动构造 HTTP 请求和解析响应，这让我对 HTTP 协议有了更深入的理解：请求行、头部字段、空行分隔符、Content-Length 的作用、状态码的含义等。同时也认识到裸 socket 编程的脆弱性——DNS 解析失败、连接超时、接收不完整、JSON 格式异常等各种边界情况都需要逐一处理。httprequest_weather() 从最初的"正常路径能跑就行"到最终加入完整的错误检查和资源释放，体现了网络编程中防御性编程的重要性。')

add_heading('6.4 嵌入式 UI 开发的思考', level=2)
add_para('LVGL 作为嵌入式 GUI 库，在资源受限的平台上提供了丰富的控件和较好的渲染效果。但在实际开发中，需要注意控件的生命周期管理——比如 display_threshold() 最初每次调用都创建新控件，导致控件无限堆积和内存泄漏。这提醒我在事件回调中要区分"创建"和"更新"两个阶段：控件在页面创建时初始化一次，后续回调只更新属性，不重复创建。')

add_heading('6.5 项目工程化的收获', level=2)
add_para('通过完整实现这个项目，我获得了以下工程化经验：（1）模块化设计思维——虽然当前代码仍有 ui_common.h"上帝文件"和 network.c 职责混合等架构问题，但已经明确了进一步重构的方向（拆分状态管理、独立天气 API 模块、剥离配置管理）。（2）Bug 定位与修复能力——通过代码审查系统性发现 8 类严重 Bug 并逐一修复，培养了"先分析根因再修改代码"的习惯。（3）文档意识——通过编写本验收报告，系统梳理了项目的设计思路和实现细节，加深了对整体架构的理解。')

add_heading('6.6 总结', level=2)
add_para('本项目实现了一个功能完整的嵌入式 IOT 数据监控终端，涵盖数据采集（天气 API + TCP 传感器数据）、本地持久化、阈值告警（LED + 蜂鸣器）、多页面触屏 UI、远程通信（TCP 双向通信）等核心能力。系统采用多线程并发架构，通过读写锁、互斥锁、条件变量和 pending event 机制保证了线程安全和 UI 安全。经过多轮 Bug 修复和优化，系统在功能完整性、运行稳定性和代码可维护性方面均达到了预期设计目标。')

# ---- 保存 ----
output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'IOT数据监控平台_项目验收报告.docx')
doc.save(output_path)
print(f'文档已生成: {output_path}')
