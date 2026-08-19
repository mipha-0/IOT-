# 项目长期记忆

## 项目概述

IOT 数据监控平台 — 嵌入式 Linux IOT 监控终端
- 技术栈：ARM Linux (arm-linux-gcc) + LVGL v9.0.0-dev + framebuffer(800×480, 32bit) + evdev + pthread + cJSON
- 6 个后台线程：温湿度采集、数据写文件、LED控制、BEEP控制、TCP客户端、LVGL定时器(2s刷新)
- 4 个 UI 页面：主页(温湿度+曲线)、事件页、历史页、设置页
- 数据来源：心知天气 API (广州温湿度) + TCP 远程传感器数据
- 告警：阈值触发 LED + 蜂鸣器

## 重构状态（2026-08-18 更新）

MODULE_STRUCTURE_REPORT.md 定义了重构方案，但用户已回退所有重构改动。
- **当前状态**：回退到重构前代码，新模块文件已删除（仅残留 .o）
- 代码结构：network.c(TCP+HTTP+直接UI操作)、data_source.c(数据采集+文件写入+UI定时器)、ui_common.h(上帝文件12 extern)、配置读写嵌入 ui_page_settings.c

## Bug 修复（2026-08-18 完成）

对回退后代码进行了全面 Bug 修复（修改 9 个文件，无新增文件）：
- **LVGL 线程安全**：新增 pending event 机制（ui_common.c），后台线程通过 push_pending_event 推入事件，主线程 lv_timer(200ms) 消费；历史页后台线程改为 lv_timer(5s)
- **全局变量加锁**：新增 status_mutex 保护 LED_status/BEEP_status/connect_event/device_status；所有读写均加锁
- **cJSON NULL 检查**：get_news/httprequest_weather 所有 cJSON_GetObjectItem 返回值检查
- **httprequest_weather**：connect/calloc/cJSON_Parse 返回值检查，错误路径 close(fd)
- **display_threshold()**：修复局部变量遮蔽全局的自赋值 bug
- **data_source**：HTTP 请求移出写锁外，write_data_to_file 改为事件驱动（cond_var 等待新数据，跳过重复数据）
- **beep_ctrl**：未超阈值时加 sleep(1) 避免空循环；被唤醒后 continue 重新检查状态
- **其他**：pthread_create 返回值检查、threshold int→float、死代码清理
- **架构问题仍在**：ui_common.h 上帝文件、network.c 职责混乱、配置嵌入UI（本次只修 Bug 不重构架构）
