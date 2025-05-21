# ESP32 控制板

![ESP32 控制板](https://www.espressif.com/sites/default/files/product_images/esp32_2.png)

## 项目概述

ESP32 控制板是一个基于 ESP32 微控制器平台的复杂电机控制系统，利用 ESP-IDF 框架和 FreeRTOS 实时操作系统构建。本项目复制了基于 STM32 的控制板功能，为遥控和自主电机控制应用提供了一个稳健的解决方案。

系统采用多任务架构，通过 FreeRTOS 队列实现任务间通信，确保实时响应性和系统稳定性。它支持通过 SBUS 协议进行遥控，也支持通过命令接口进行编程控制。

## 主要特性

- **双输入控制**：
  - SBUS 接收器输入，用于遥控器控制
  - 命令速度（cmd_vel）接口，用于编程控制

- **电机控制**：
  - 通过 CAN 总线（TWAI）与 LKBLS481502 双通道电机驱动器接口
  - 可配置的电机速度和方向控制
  - 带超时保护的自动刹车系统

- **实时性能**：
  - 基于 FreeRTOS 的任务架构
  - 基于优先级的调度
  - 基于队列的任务间通信

- **系统监控**：
  - LED 状态指示
  - 通过串行接口进行全面日志记录
  - 看门狗保护

## 系统架构

### 硬件组件

- **ESP32 微控制器**：带集成 Wi-Fi 和蓝牙的双核处理器
- **SBUS 接收器**：用于遥控输入
- **CAN 收发器**：与电机驱动器接口所需（ESP32 上不包含）
- **LKBLS481502**：带 CAN 接口的双通道电机驱动器

### 软件架构

#### 多任务设计

| 任务 | 优先级 | 堆栈大小 | 描述 |
|------|----------|------------|-------------|
| SBUS 处理 | 高 (12) | 4096 字节 | 接收并解析来自遥控接收器的 SBUS 信号 |
| CMD_VEL 接收 | 高 (12) | 2048 字节 | 接收并解析命令速度消息 |
| 电机控制 | 中 (10) | 4096 字节 | 处理命令并控制电机 |
| 状态监控 | 低 (5) | 2048 字节 | 监控系统状态并更新 LED 指示器 |

#### 任务间通信

- **SBUS 队列**：将通道数据从 SBUS 任务传输到电机控制任务
- **CMD 队列**：将电机命令从 CMD_VEL 任务传输到电机控制任务
- **优先级管理**：确保关键任务在需要时获得 CPU 时间

#### 控制流程

1. 通过 SBUS 或 CMD_VEL 接口接收输入信号
2. 解析信号并转换为电机命令
3. 对电机命令进行优先级排序（CMD_VEL 优先于 SBUS）
4. 通过 CAN 总线将命令发送到电机驱动器
5. 持续监控系统状态并指示

## 硬件连接

### GPIO 引脚分配

| 功能 | GPIO 引脚 | 描述 |
|----------|----------|-------------|
| **状态指示器** |
| LED_BLUE_PIN | GPIO_NUM_2 | 系统状态指示蓝色 LED |
| **电机控制** |
| LEFT_EN_PIN | GPIO_NUM_4 | 左电机使能 |
| LEFT_DIR_PIN | GPIO_NUM_5 | 左电机方向 |
| RIGHT_DIR_PIN | GPIO_NUM_18 | 右电机方向 |
| RIGHT_EN_PIN | GPIO_NUM_19 | 右电机使能 |
| LEFT_BK_PIN | GPIO_NUM_21 | 左电机刹车 |
| RIGHT_BK_PIN | GPIO_NUM_22 | 右电机刹车 |
| **通信** |
| UART_DEBUG | UART0 | 调试串口（默认引脚） |
| UART_SBUS | UART2 (RX: GPIO16) | SBUS 接收器输入 |
| UART_CMD | UART1 (RX: GPIO17) | 命令速度输入 |
| **CAN 总线 (TWAI)** |
| CAN_TX | GPIO_NUM_21 | CAN 总线发送（需要外部收发器） |
| CAN_RX | GPIO_NUM_22 | CAN 总线接收（需要外部收发器） |

### CAN 总线配置

- **协议**：TWAI（Two-Wire Automotive Interface，ESP32 的 CAN 实现）
- **波特率**：250 Kbps
- **模式**：正常模式
- **过滤器**：接受所有消息
- **所需外部硬件**：CAN 收发器（例如 SN65HVD230、TJA1050）

## 项目结构

```
esp32controlboard/
├── main/
│   ├── CMakeLists.txt        # 组件 makefile
│   ├── main.c                # 主应用程序入口点
│   ├── main.h                # 全局定义和配置
│   ├── sbus.c                # SBUS 协议实现
│   ├── sbus.h                # SBUS 接口定义
│   ├── channel_parse.c       # 通道值解析和运动控制
│   ├── channel_parse.h       # 通道解析接口
│   ├── drv_keyadouble.c      # 电机驱动实现
│   └── drv_keyadouble.h      # 电机驱动接口
├── CMakeLists.txt            # 项目 makefile
├── auto_build.bat            # 自动构建脚本
├── flash.bat                 # 自动烧录脚本
└── README.md                 # 项目文档
```

## 构建和烧录

### 前提条件

- 已安装 ESP-IDF v5.x
- 已安装 Python 3.x
- 已安装 Git
- 兼容的 ESP32 开发板
- CAN 收发器模块
- LKBLS481502 双通道电机驱动器

### 自动构建过程

项目包含两个批处理脚本，用于轻松构建和烧录：

1. **auto_build.bat**：自动构建项目
   - 设置 ESP-IDF 环境
   - 清理项目
   - 构建项目
   - 显示构建结果
   - 完成后自动关闭

2. **flash.bat**：将构建好的固件烧录到 ESP32
   - 自动检测可用的 COM 端口
   - 允许选择目标 COM 端口
   - 烧录固件
   - 显示烧录结果

### 手动构建过程

如果您更喜欢手动构建：

1. 设置 ESP-IDF 环境：
   ```
   . $IDF_PATH/export.sh  # Linux/macOS
   %IDF_PATH%\export.bat  # Windows
   ```

2. 导航到项目目录：
   ```
   cd path/to/esp32controlboard
   ```

3. 构建项目：
   ```
   idf.py build
   ```

4. 烧录到 ESP32：
   ```
   idf.py -p [PORT] flash
   ```

5. 监控串口输出：
   ```
   idf.py -p [PORT] monitor
   ```

## 使用说明

1. **硬件设置**：
   - 通过 USB 将 ESP32 连接到计算机
   - 将 SBUS 接收器连接到 GPIO16（UART2 RX）
   - 将 CAN 收发器连接到 GPIO21（TX）和 GPIO22（RX）
   - 将 CAN 收发器连接到 LKBLS481502 电机驱动器
   - 将电机连接到驱动器

2. **构建和烧录**：
   - 运行 `auto_build.bat` 构建固件
   - 运行 `flash.bat` 将固件烧录到 ESP32
   - 在提示时选择适当的 COM 端口

3. **操作**：
   - 系统将初始化并等待输入
   - 通过遥控发射器（SBUS）控制或通过 UART1 发送 cmd_vel 命令
   - 蓝色 LED 指示系统状态：
     - 以 1Hz 闪烁：系统正常运行
     - 快速闪烁：接收命令
     - 常亮/常灭：错误状态

4. **命令格式**：
   - CMD_VEL 格式：`[0xFF, 0x02, speed_left, speed_right, 0x00]`
   - 速度值范围从 -100 到 100

## 技术说明

### CAN 通信

ESP32 的 TWAI 模块需要外部 CAN 收发器才能与 CAN 总线接口。收发器将 ESP32 的 TTL 电平信号转换为差分 CAN 总线信号。

### SBUS 协议

SBUS 是许多遥控接收器使用的数字串行协议。它以 100,000 波特率运行，采用反相逻辑、8 个数据位、偶校验和 2 个停止位。

### 电机控制

系统使用以下 CAN 命令控制 LKBLS481502 双通道电机驱动器：
- 使能电机：`[0x23, 0x0D, 0x20, channel, 0x00, 0x00, 0x00, 0x00]`
- 禁用电机：`[0x23, 0x0C, 0x20, channel, 0x00, 0x00, 0x00, 0x00]`
- 设置速度：`[0x23, 0x00, 0x20, channel, speed_bytes]`

## 故障排除

- **ESP32 无响应**：检查电源和 USB 连接
- **CAN 通信失败**：验证 CAN 收发器连接和电源
- **电机无响应**：检查电机驱动器电源和 CAN 连接
- **SBUS 不工作**：验证 SBUS 接收器电源和信号连接
- **构建错误**：确保正确安装 ESP-IDF 并设置环境变量

## 许可证

本项目采用 MIT 许可证 - 详情请参阅 LICENSE 文件。

## 致谢

- 感谢 Espressif Systems 提供 ESP-IDF 框架
- 感谢 FreeRTOS 提供实时操作系统
- 感谢原始 STM32 控制板开发者提供参考实现
