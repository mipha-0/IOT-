# IOT 数据监控平台

基于 LVGL 的嵌入式 GUI 应用，运行于 gec6818（ARM Linux）开发板，提供温湿度实时监控、历史数据查询、设备远程控制等功能。通过 TCP 客户端与上位机/华为云对接，接收远程命令并执行设备控制。

## 核心功能

| 功能模块 | 说明 |
|---|---|
| 实时监控主页 | 显示当前温度、湿度数值 + 30 点滚动曲线图 |
| 事件日志页 | 实时显示系统事件（连接状态、命令下发、报警等），环形缓冲 16 条 |
| 历史数据页 | 从 `./data.txt` 读取最近 20 条历史记录，表格展示 |
| 系统设置页 | MQTT 服务器/端口/设备 ID 配置、温湿度阈值调节、LED/BEEP 开关 |
| 远程命令接收 | TCP 客户端接收 `command_name` 为 LED / 蜂鸣器 / 温度阈值 / 湿度阈值 的 JSON 命令 |
| HTTP 天气拉取 | 通过 seniverse API 周期获取真实温湿度作为数据源 |
| LED 控制 | 根据温度阈值控制 4 路 LED：达到阈值常亮，未达阈值按温度比例闪烁 |
| 蜂鸣器报警 | 温度或湿度超阈值时蜂鸣 3s |
| 配置持久化 | `config.txt` 存 MQTT 配置，`threshold.txt` 存阈值，`data.txt` 存历史数据 |

## 技术栈

- **语言**：C99（`-std=gnu99`）
- **GUI 框架**：LVGL（含 lv_drivers，framebuffer + evdev）
- **JSON 解析**：cJSON（第三方库内置于 `src/third_party/`）
- **线程同步**：POSIX pthread（rwlock / mutex / cond）
- **网络**：POSIX socket（TCP 客户端 + HTTP GET）
- **硬件访问**：Linux ioctl（`/dev/Led`、`/dev/beep`、`/dev/rtc0`）
- **交叉编译**：`arm-linux-gcc`

## 环境要求

- **目标硬件**：gec6818 开发板（ARM Cortex-A53，800×480 LCD 屏）
- **操作系统**：嵌入式 Linux（自带 framebuffer 与 evdev 输入设备）
- **交叉工具链**：`arm-linux-gcc`（需在 PATH 中）
- **运行权限**：root（写 RTC、访问 `/dev/Led`、`/dev/beep`）
- **第三方依赖**：LVGL 与 lv_drivers 源码已作为子模块置于项目根目录（`lvgl/`、`lv_drivers/`）

## 目录结构

```
IOT数据监控平台/
├── Makefile                  # 交叉编译构建脚本
├── lv_conf.h                 # LVGL 配置（分辨率、字体、特性开关）
├── lv_drv_conf.h             # lv_drivers 配置（framebuffer/evdev 等）
├── mouse_cursor_icon.c       # 鼠标光标位图
├── README.md                 # 本文件
├── lvgl/                     # LVGL 核心库（子模块）
├── lv_drivers/               # LVGL 显示/输入驱动（子模块）
└── src/                      # 应用源码
    ├── app/                  # 应用入口层
    │   ├── main.c            # 程序入口：初始化 LVGL/显示/输入，启动所有后台线程
    │   ├── ui_manager.c       # UI 框架：导航栏 + 4 页面容器 + 时间标签
    │   └── ui_manager.h
    ├── ui/                   # UI 层（页面与公共组件）
    │   ├── ui_common.c/.h    # 共享状态、跨线程事件队列、颜色/字体/布局常量
    │   ├── ui_navbar.c/.h    # 顶部导航栏（4 个页面切换按钮）
    │   ├── ui_page_home.c/.h # 主页：实时温湿度 + 曲线图
    │   ├── ui_page_event.c/.h# 事件页：系统事件列表
    │   ├── ui_page_history.c/.h # 历史页：data.txt 表格展示
    │   └── ui_page_settings.c/.h # 设置页：MQTT 配置 + 阈值 + LED/BEEP 开关
    ├── data/                 # 数据层
    │   ├── data_source.c     # 后台线程：HTTP 拉取天气 → 共享变量 → 写 data.txt
    │   └── data_source.h
    ├── net/                  # 网络层
    │   ├── network.c         # TCP 客户端 + HTTP 天气请求 + 命令分发
    │   ├── network.h         # 对外接口：client() 线程入口、httprequest_weather()
    │   └── get_time.c        # HTTP 获取北京时间并同步系统/RTC（独立可执行）
    ├── drivers/              # 硬件驱动层
    │   ├── hw_ctrl.c         # LED 闪烁/常亮控制 + 蜂鸣器报警
    │   └── hw_ctrl.h         # 对外接口：led_ctrl()、beep_ctrl() 线程入口
    └── third_party/         # 第三方库
        ├── cJSON.c/.h        # JSON 解析库
```

### 各模块关系

```
┌─────────────────────────────────────────────────────────────┐
│                      app/main.c                              │
│  (LVGL 初始化 + 启动 5 个后台线程 + 主循环 lv_timer_handler) │
└─────────────────────────────────────────────────────────────┘
                  │                              │
                  ▼                              ▼
        ┌──────────────────┐            ┌──────────────────┐
        │   app/ui_manager │◄───────────►│  ui/ui_common    │
        │   (UI 框架)      │            │ (共享状态 + 事件队列)│
        └──────────────────┘            └──────────────────┘
                  │                              ▲
                  ▼                              │ push_pending_event
        ┌──────────────────────────────────┐    │ request_threshold_refresh
        │  ui/ui_page_*  (4 个页面实现)    │    │ request_history_refresh
        └──────────────────────────────────┘    │
                                                  │
        ┌──────────────────┐            ┌──────────────────┐
        │  data/data_source│ ──────────► │  ui/ui_common    │
        │  (HTTP 天气 + 写文件)│            │  (后台线程 → 主线程)│
        └──────────────────┘            └──────────────────┘
                                                ▲
        ┌──────────────────┐                    │
        │   net/network    │ ──────────────────┤
        │ (TCP 客户端 + 命令分发)│               │
        └──────────────────┘            ┌──────────────────┐
                │                       │ drivers/hw_ctrl  │
                ▼                       │ (LED + 蜂鸣器)    │
        命令 signal cond_var ──────────►└──────────────────┘
```

- **app/main.c** 是入口：初始化 LVGL 后，启动 `client`、`get_temp_humi`、`write_data_to_file`、`led_ctrl`、`beep_ctrl` 五个后台线程，主线程进入 `lv_timer_handler` 循环
- **ui/ui_common** 是共享中心：定义全部全局状态（温湿度、阈值、设备状态、锁），提供 `push_pending_event` / `request_*_refresh` 接口让后台线程安全更新 UI
- **data_source** 负责数据采集：HTTP 拉天气 → 写共享变量 → 通知 `write_data_to_file` 线程持久化 + 唤醒 `update_history_data` 线程刷新历史页
- **net/network** 负责命令接收：TCP 客户端连接服务器，解析 JSON 命令 → 更新共享状态 → 推事件给主线程 + signal 唤醒 LED/BEEP 线程
- **drivers/hw_ctrl** 负责硬件控制：`led_ctrl` 线程根据温度/阈值决策 LED 闪烁频率，`beep_ctrl` 线程超阈值时蜂鸣报警
- **跨线程通信**：后台线程绝不直接调 LVGL API（LVGL 非线程安全），统一通过 `ui_common` 的 pending 队列 + `lv_timer` 消费模式

## 重要文件作用

| 文件 | 作用 |
|---|---|
| [src/app/main.c](src/app/main.c) | 程序入口，启动所有线程，主循环驱动 LVGL tick 与 timer |
| [src/app/ui_manager.c](src/app/ui_manager.c) | 创建导航栏 + 4 页面容器，初始化所有锁与条件变量 |
| [src/ui/ui_common.h](src/ui/ui_common.h) | 项目"宪法"：全部共享变量、锁、UI 布局常量、跨线程接口声明 |
| [src/ui/ui_common.c](src/ui/ui_common.c) | pending event 队列实现 + 主线程 timer 消费逻辑 |
| [src/data/data_source.c](src/data/data_source.c) | `get_temp_humi` 线程：HTTP 拉天气；`write_data_to_file` 线程：写 `data.txt` |
| [src/net/network.c](src/net/network.c) | `client` 线程：TCP 连接 + `get_news`/`send_news` 子线程 + 命令分发 |
| [src/drivers/hw_ctrl.c](src/drivers/hw_ctrl.c) | LED/BEEP 控制：基于 cond_var 的等待-唤醒模式 |
| [src/net/get_time.c](src/net/get_time.c) | 独立工具：HTTP 获取北京时间 + 同步系统时间与 RTC |
| [Makefile](Makefile) | 交叉编译脚本，`arm-linux-gcc` + LVGL 模块 |
| [lv_conf.h](lv_conf.h) | LVGL 配置：色彩深度、字体、特性开关 |
| [lv_drv_conf.h](lv_drv_conf.h) | 显示/输入设备配置：framebuffer、evdev |

## 安装步骤

### 1. 准备工具链

确保 `arm-linux-gcc` 已安装并在 PATH 中：

```bash
arm-linux-gcc --version
```

### 2. 克隆项目（含子模块）

```bash
git clone --recurse-submodules <repo-url> IOT数据监控平台
cd IOT数据监控平台
```

如果已克隆但未拉子模块：

```bash
git submodule update --init --recursive
```

### 3. 编译

```bash
make            # 输出可执行文件 demo
```

如需重新编译：

```bash
make clean && make
```

### 4. 部署到开发板

将 `demo` 与运行时所需配置文件传到板子：

```bash
scp demo root@<board-ip>:/root/
scp config.txt threshold.txt root@<board-ip>:/root/   # 若已有配置
```

板子端授权并运行：

```bash
chmod +x demo
./demo          # 需要 root 权限访问 /dev/Led /dev/beep
```

## 使用指南

程序启动后会自动显示主页，通过顶部导航栏切换页面：

- **Home（主页）**：实时温湿度数值 + 30 点滚动曲线，每 5 秒刷新
- **Event（事件）**：系统事件日志，新事件出现在顶部
- **History（历史）**：从 `data.txt` 读取最近 20 条历史记录
- **Settings（设置）**：MQTT 配置 + 阈值调节 + LED/BEEP 开关

### 远程命令格式

TCP 服务器向板子下发的 JSON 命令格式：

```json
{
    "command_name": "LED",
    "paras": { "LED开关": 1 }
}
```

支持的 `command_name` 与 `paras` 字段：

| command_name | paras 字段 | 说明 |
|---|---|---|
| `LED` | `LED开关`（0/1） | 关闭/开启 LED 控制 |
| `蜂鸣器` | `蜂鸣器开关`（0/1） | 关闭/开启蜂鸣器报警 |
| `温度阈值` | `temp_threshold`（整数℃） | 设置温度阈值 |
| `湿度阈值` | `humi_threshold`（整数%） | 设置湿度阈值 |

## 配置说明

### `lv_conf.h` / `lv_drv_conf.h`

LVGL 与 lv_drivers 的全局配置，主要项：

- 屏幕分辨率：800×480
- 色彩深度：16/32 bit（按板子 LCD 实际配置）
- 字体：`lv_font_simsun_16_cjk`（中英文）、`lv_font_montserrat_16`（数字）
- 显示驱动：framebuffer (`fbdev`)
- 输入驱动：evdev (`evdev_read`)

### 运行时配置文件

程序运行时读取/写入以下文件（路径相对于可执行文件当前目录）：

| 文件 | 用途 | 格式 |
|---|---|---|
| `config.txt` | MQTT 服务器配置 | `port:8883`<br>`id:hw_device_001`<br>`ip:iot-acc.cn-north-4.myhuaweicloud.com` |
| `threshold.txt` | 温湿度阈值持久化 | `temp:35.000000`<br>`humi:50.000000` |
| `data.txt` | 历史温湿度数据 | `2026-08-19 10:30:00,25.6,55.3` |

首次运行无需这些文件，程序会自动创建 `data.txt` 与 `threshold.txt`；`config.txt` 缺失时使用代码内默认值。

### 网络配置

代码中硬编码的服务器地址（位于 [src/net/network.c](src/net/network.c)）：

- TCP 服务器：`192.168.44.21:8888`（命令下发通道）
- HTTP 天气 API：`api.seniverse.com`（seniverse 天气服务，密钥在源码内）

如需修改，编辑 [src/net/network.c](src/net/network.c) 中对应字符串字面量后重新编译。

## 调试技巧

- **看运行日志**：板子端 stdout/stderr 有 `printf` 与 `perror` 输出，建议 `./demo 2>&1 | tee run.log`
- **常见段错误排查**：
  - `data.txt` 不存在 + 历史页已显示：见 [ui_page_history.c#L129](src/ui/ui_page_history.c#L129) 的 NULL 判断
  - TCP 对端断开：`client` 线程会自动重连
  - 权限不足：以 root 运行，否则无法访问 `/dev/Led`、`/dev/beep`
- **修改后务必 `make clean && make`**：旧 `.o` 残留易导致全局变量重复定义链接错误

## 已知限制

1. 网络模块硬编码服务器 IP 与天气 API key，未做配置化
2. 无 NTP 自动时间同步，依赖 `get_time` 工具手动同步或板子内置 RTC
3. 多客户端同时连接时只服务最新连接（`cli_socket_fd` 单值覆盖）