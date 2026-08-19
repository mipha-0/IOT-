#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 IOT 数据监控平台项目说明书 Word 文档"""

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

def add_code_para(text, size=10):
    """添加等宽格式段落（用于代码/目录树）"""
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.15
    p.paragraph_format.left_indent = Pt(12)
    run = p.add_run(text)
    run.font.name = 'Consolas'
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
title_run = title_p.add_run('IOT 数据监控平台\n项目说明书')
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

# ===================== 一、项目名称与简介 =====================
add_heading('一、项目名称与简介', level=1)

add_heading('1.1 项目名称', level=2)
add_para('IOT 数据监控平台')

add_heading('1.2 项目简介', level=2)
add_para('本项目是一个基于 ARM Linux 平台的嵌入式 IOT 数据监控终端，运行于 gec6818 开发板（ARM Cortex-A53，800×480 LCD 屏）。系统通过心知天气 API 周期获取广州地区实时温湿度数据，在 LVGL 图形界面上进行多页面展示，并具备阈值告警（LED 闪烁 + 蜂鸣器报警）、历史数据本地持久化与查询、远程 TCP 双向通信等功能。')
add_para('通过 TCP 客户端与上位机/华为云 IoT 平台对接，接收远程控制指令（LED 开关、蜂鸣器开关、阈值调节），同时定时上报温湿度及设备状态。系统采用多线程并发架构，通过读写锁、互斥锁、条件变量和 pending event 机制保证线程安全与 UI 安全。')

# ===================== 二、技术栈说明 =====================
add_heading('二、技术栈说明', level=1)

add_table(
    ['类别', '技术/工具', '说明'],
    [
        ['编程语言', 'C99 (-std=gnu99)', '全部应用层源码使用 C 语言编写'],
        ['GUI 框架', 'LVGL v9.0.0-dev', '轻量级嵌入式图形库，支持丰富控件'],
        ['显示驱动', 'Linux framebuffer', '直驱 /dev/fb0，32 位色深，800×480'],
        ['输入驱动', 'evdev', 'Linux 标准输入框架，支持触摸屏/鼠标'],
        ['JSON 解析', 'cJSON', '轻量级 C 语言 JSON 库（内置源码）'],
        ['线程同步', 'POSIX pthread', '读写锁 / 互斥锁 / 条件变量'],
        ['网络通信', 'POSIX socket', '裸 socket 编程，TCP 客户端 + HTTP GET'],
        ['硬件访问', 'Linux ioctl', '/dev/Led、/dev/beep、/dev/rtc0'],
        ['交叉编译', 'arm-linux-gcc', 'ARM 交叉编译工具链'],
        ['构建工具', 'GNU Make', 'Makefile 管理编译链接流程'],
        ['版本控制', 'Git', '含 4 个子模块（lvgl/lv_drivers 等）'],
        ['IDE 支持', 'Eclipse CDT + VS Code', '.cproject/.project + .vscode/ 配置'],
        ['文档生成', 'Python + python-docx', 'gen_report.py 生成 Word 验收报告'],
        ['代码格式化', 'Clang-Format', '.clang-format 配置文件'],
    ]
)

# ===================== 三、目录结构说明 =====================
add_heading('三、目录结构说明', level=1)

add_para('项目根目录下包含构建配置文件、LVGL 子模块库、以及 src/ 应用源码目录。完整目录结构如下：', indent=True)

tree_lines = [
    'IOT数据监控平台/',
    '├── Makefile                  # 交叉编译构建脚本（arm-linux-gcc）',
    '├── lv_conf.h                 # LVGL 核心配置（色深 32bit、字体、内存等）',
    '├── lv_drv_conf.h             # lv_drivers 配置（framebuffer/evdev 启用）',
    '├── mouse_cursor_icon.c       # 鼠标光标位图数据',
    '├── gen_report.py             # Python 脚本：生成 Word 项目验收报告',
    '├── README.md                 # 项目根目录说明文件',
    '├── LICENSE                   # MIT 开源许可证',
    '├── .clang-format             # 代码格式化规则',
    '├── .gitignore                # Git 忽略规则',
    '├── .gitmodules               # Git 子模块配置（lvgl/lv_drivers 等）',
    '├── .project / .cproject      # Eclipse CDT 项目文件',
    '│',
    '├── lvgl/                     # [子模块] LVGL v9 核心图形库源码',
    '├── lv_drivers/               # [子模块] LVGL 显示/输入驱动库',
    '│',
    '├── .vscode/                  # VS Code 编辑器配置',
    '├── .github/                  # GitHub Actions 自动化配置',
    '├── .settings/                # Eclipse CDT 偏好设置',
    '│',
    '└── src/                      # 应用源码（本项目自主编写部分）',
    '    ├── readme.md             # src 目录详细说明文档',
    '    ├── app/                  # 应用入口层',
    '    │   ├── main.c            #   程序入口：初始化 + 启动线程 + 主循环',
    '    │   └── ui_manager.c/.h   #   UI 框架：导航栏 + 页面容器 + 锁初始化',
    '    ├── data/                 # 数据层',
    '    │   ├── data_source.c     #   HTTP 天气采集 + data.txt 持久化',
    '    │   └── data_source.h',
    '    ├── drivers/              # 硬件驱动层',
    '    │   ├── hw_ctrl.c         #   LED 闪烁/常亮 + 蜂鸣器报警',
    '    │   └── hw_ctrl.h',
    '    ├── net/                  # 网络层',
    '    │   ├── network.c         #   TCP 客户端 + HTTP 天气 + 命令分发',
    '    │   └── network.h',
    '    ├── third_party/          # 第三方库',
    '    │   └── cJSON.c/.h        #   JSON 解析库',
    '    └── ui/                   # UI 界面层',
    '        ├── ui_common.c/.h    #   共享状态 + 跨线程事件队列 + 样式常量',
    '        ├── ui_navbar.c/.h    #   顶部导航栏（4 按钮页面切换）',
    '        ├── ui_page_home.c/.h #   主页：温湿度卡片 + 曲线图',
    '        ├── ui_page_event.c/.h #   事件页：事件表格 + 统计卡片',
    '        ├── ui_page_history.c/.h #   历史页：data.txt 表格展示',
    '        └── ui_page_settings.c/.h #   设置页：MQTT 配置 + 阈值 + 开关',
]
for line in tree_lines:
    add_code_para(line)

add_para('注：以上目录结构中 app/data/drivers/net/ui/third_party 为 src 下的子目录（层级缩进表示包含关系）。', indent=True)

# ===================== 四、主要功能模块描述 =====================
add_heading('四、主要功能模块描述', level=1)

add_heading('4.1 实时监控主页（ui_page_home）', level=2)
add_bullet('显示当前温度、湿度数值，超阈值红色告警，正常绿色')
add_bullet('30 点滚动曲线图（lv_chart），温度/湿度双 Y 轴')
add_bullet('主线程 lv_timer 每 2 秒刷新数据和曲线')

add_heading('4.2 事件日志页（ui_page_event）', level=2)
add_bullet('实时显示系统事件：告警事件、命令事件、系统事件')
add_bullet('左侧事件表格（30 行），右侧 3 个统计卡片计数')
add_bullet('新事件通过 pending event 队列由主线程 200ms 定时器消费更新')

add_heading('4.3 历史数据页（ui_page_history）', level=2)
add_bullet('从 ./data.txt 读取最近 20 条历史记录，表格展示')
add_bullet('创建时一次性加载，后续新数据通过标志位机制由主线程实时更新')
add_bullet('最新数据始终显示在第一行')

add_heading('4.4 系统设置页（ui_page_settings）', level=2)
add_bullet('MQTT 服务器/端口/设备 ID 配置显示（默认华为云 IoT 平台）')
add_bullet('温度/湿度阈值调节（步进 0.5，支持温度/湿度模式切换）')
add_bullet('LED 和蜂鸣器开关控制')
add_bullet('配置保存与重置功能，持久化到 config.txt 和 threshold.txt')

add_heading('4.5 数据采集模块（data_source — get_temp_humi 线程）', level=2)
add_bullet('每 5 秒通过裸 socket HTTP GET 请求心知天气 API')
add_bullet('手动构造 HTTP 请求，解析 JSON 响应提取温湿度')
add_bullet('写入共享变量（读写锁保护），通知持久化线程和历史页')

add_heading('4.6 数据持久化模块（data_source — write_data_to_file 线程）', level=2)
add_bullet('事件驱动模式：pthread_cond_wait 阻塞等待新数据信号')
add_bullet('格式化"时间,温度,湿度"写入 ./data.txt')
add_bullet('自动跳过重复数据，避免无效写入')

add_heading('4.7 TCP 远程通信模块（network — client 线程）', level=2)
add_bullet('TCP 客户端连接服务器（192.168.44.21:8888），失败自动重连')
add_bullet('接收子线程：解析 JSON 命令（LED/蜂鸣器/温度阈值/湿度阈值），更新状态变量，唤醒硬件控制线程，推事件给主线程更新 UI')
add_bullet('发送子线程：每 5 秒组装 JSON 上报温湿度及设备状态')

add_heading('4.8 LED 控制模块（hw_ctrl — led_ctrl 线程）', level=2)
add_bullet('通过 ioctl 访问 /dev/Led 控制 4 路 LED')
add_bullet('条件变量阻塞模式：OFF 时零 CPU 占用')
add_bullet('达阈值常亮，未达阈值按温度比例闪烁（温度越高越快）')

add_heading('4.9 蜂鸣器报警模块（hw_ctrl — beep_ctrl 线程）', level=2)
add_bullet('通过 ioctl 访问 /dev/beep 控制蜂鸣器')
add_bullet('条件变量阻塞模式：OFF 时零 CPU 占用')
add_bullet('ON 时温度或湿度超阈值间歇告警（响 1s 停 1s）')

add_heading('4.10 跨线程安全 UI 更新机制（ui_common）', level=2)
add_bullet('pending event 队列（16 条环形缓冲，mutex 保护）')
add_bullet('后台线程通过 push_pending_event() 推入事件')
add_bullet('主线程 200ms lv_timer 调用 process_pending_events() 消费')
add_bullet('阈值刷新和历史数据刷新通过标志位机制由主线程处理')
add_bullet('确保 LVGL 控件仅在主线程操作，避免线程安全问题')

add_heading('4.11 HTTP 天气请求模块（network — httprequest_weather）', level=2)
add_bullet('DNS 解析 → TCP 连接 → HTTP GET → 接收响应 → 解析 JSON')
add_bullet('完整错误检查（connect/calloc/cJSON_Parse 返回值检查）')
add_bullet('错误路径正确释放资源（close fd, free, cJSON_Delete）')

# ===================== 五、项目运行方式 =====================
add_heading('五、项目运行方式', level=1)

add_heading('5.1 环境要求', level=2)
add_bullet('目标硬件：gec6818 开发板（ARM Cortex-A53，800×480 LCD 屏）')
add_bullet('操作系统：嵌入式 Linux（自带 framebuffer 与 evdev 输入设备）')
add_bullet('交叉工具链：arm-linux-gcc（需在 PATH 中可用）')
add_bullet('运行权限：root（需访问 /dev/Led、/dev/beep、/dev/rtc0）')
add_bullet('子模块：LVGL 与 lv_drivers 源码需通过 git submodule 拉取')

add_heading('5.2 克隆项目（含子模块）', level=2)
add_code_para('git clone --recurse-submodules <仓库地址> IOT数据监控平台')
add_code_para('cd IOT数据监控平台')
add_code_para('# 若已克隆但未拉子模块：')
add_code_para('git submodule update --init --recursive')

add_heading('5.3 编译', level=2)
add_code_para('make            # 交叉编译，输出可执行文件 demo')
add_code_para('# 如需重新编译：')
add_code_para('make clean && make')

add_heading('5.4 部署到开发板', level=2)
add_code_para('scp demo root@<开发板IP>:/root/')
add_code_para('scp config.txt threshold.txt root@<开发板IP>:/root/  # 若已有配置文件')

add_heading('5.5 运行', level=2)
add_code_para('# 在开发板上（需 root 权限）：')
add_code_para('chmod +x demo')
add_code_para('./demo          # 需要 root 访问 /dev/Led /dev/beep')

add_heading('5.6 运行时配置文件', level=2)
add_para('路径相对于可执行文件当前目录，首次运行无需这些文件，程序自动创建 data.txt 与 threshold.txt；config.txt 缺失时使用代码内默认值。', indent=True)

add_table(
    ['文件', '用途', '格式'],
    [
        ['config.txt', 'MQTT 服务器配置', 'port:8883 / id:hw_device_001 / ip:iot-acc...'],
        ['threshold.txt', '温湿度阈值持久化', 'temp:35.000000 / humi:50.000000'],
        ['data.txt', '历史温湿度数据', '2026-08-19 10:30:00,25.6,55.3'],
    ]
)

add_heading('5.7 界面操作', level=2)
add_para('程序启动后自动显示主页，通过顶部导航栏切换 4 个页面：', indent=True)
add_bullet('Home（主页）：实时温湿度数值 + 30 点滚动曲线，每 2 秒刷新')
add_bullet('Event（事件）：系统事件日志，新事件出现在顶部')
add_bullet('History（历史）：从 data.txt 读取最近 20 条历史记录')
add_bullet('Settings（设置）：MQTT 配置 + 阈值调节 + LED/BEEP 开关')

add_heading('5.8 远程命令格式（TCP 服务器下发 JSON）', level=2)
add_code_para('{')
add_code_para('    "command_name": "LED",')
add_code_para('    "paras": { "LED开关": 1 }')
add_code_para('}')
add_para('支持的 command_name：', indent=True)

add_table(
    ['command_name', 'paras 字段', '说明'],
    [
        ['LED', 'LED开关（0/1）', '关闭/开启 LED 控制'],
        ['蜂鸣器', '蜂鸣器开关（0/1）', '关闭/开启蜂鸣器报警'],
        ['温度阈值', 'temp_threshold（℃）', '设置温度阈值'],
        ['湿度阈值', 'humi_threshold（%）', '设置湿度阈值'],
    ]
)

add_heading('5.9 调试建议', level=2)
add_bullet('看运行日志：./demo 2>&1 | tee run.log')
add_bullet('修改后务必 make clean && make（旧 .o 残留易导致链接错误）')
add_bullet('常见段错误：检查权限（需 root）、检查 data.txt 是否存在')

# ---- 保存 ----
output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '项目说明书.docx')
doc.save(output_path)
print(f'文档已生成: {output_path}')
