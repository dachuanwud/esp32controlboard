# 🛠️ ESP32控制板开发环境搭建指南

本文档详细介绍ESP32控制板项目的开发环境搭建过程，包括ESP-IDF安装、工具链配置、IDE设置和项目验证。

## 🎯 环境要求

### 系统要求
- **操作系统**: Windows 10/11 (64位)
- **内存**: 最少4GB，推荐8GB以上
- **存储空间**: 至少5GB可用空间
- **网络**: 稳定的互联网连接（用于下载组件）

### 硬件要求
- **ESP32开发板**: ESP32-WROOM-32或兼容板
- **USB数据线**: Type-C或Micro-USB（根据开发板而定）
- **串口驱动**: CH340、CP2102或FTDI驱动

## 📦 ESP-IDF安装

### 1. 下载ESP-IDF安装器

访问Espressif官网下载ESP-IDF安装器：
- **官方地址**: https://dl.espressif.com/dl/esp-idf/
- **推荐版本**: ESP-IDF v5.4.1 (项目当前使用版本)
- **安装器**: esp-idf-tools-setup-online-5.4.1.exe

### 2. 运行安装器

1. **以管理员身份运行**安装器
2. **选择安装路径**：
   - 推荐路径：`C:\Espressif\`
   - 避免包含中文或空格的路径
3. **选择ESP-IDF版本**：v5.4.1
4. **选择Python版本**：3.11.x (推荐)
5. **选择Git版本**：最新版本

### 3. 验证安装

安装完成后，打开"ESP-IDF 5.4 CMD"命令行工具：

```bash
# 检查ESP-IDF版本
idf.py --version

# 预期输出
ESP-IDF v5.4.1
```

## 🔧 项目配置

### 1. 克隆项目

```bash
# 克隆项目到本地
git clone https://github.com/dachuanwud/esp32controlboard.git
cd esp32controlboard
```

### 2. 配置环境变量

根据项目中的批处理脚本，确认以下路径配置：

<augment_code_snippet path="build_only.bat" mode="EXCERPT">
````batch
REM Configuration variables
set PROJECT_NAME=esp32controlboard
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.4.1
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
````
</augment_code_snippet>

### 3. 项目结构验证

确认项目目录结构正确：

<augment_code_snippet path="main/CMakeLists.txt" mode="EXCERPT">
````cmake
idf_component_register(SRCS "main.c"
                       "channel_parse.c"
                       "drv_keyadouble.c"
                       "sbus.c"
                    INCLUDE_DIRS ".")
````
</augment_code_snippet>

## 🔌 硬件连接

### ESP32引脚分配

根据项目源码，ESP32的引脚分配如下：

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// 定义GPIO引脚
// LED指示灯引脚 - 共阳极RGB LED
// LED1组
#define LED1_RED_PIN            GPIO_NUM_12  // LED1红色引脚
#define LED1_GREEN_PIN          GPIO_NUM_13  // LED1绿色引脚
#define LED1_BLUE_PIN           GPIO_NUM_14  // LED1蓝色引脚

// LED2组
#define LED2_RED_PIN            GPIO_NUM_25  // LED2红色引脚
#define LED2_GREEN_PIN          GPIO_NUM_26  // LED2绿色引脚
#define LED2_BLUE_PIN           GPIO_NUM_27  // LED2蓝色引脚

// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
#define KEY2_PIN                GPIO_NUM_35  // 按键2

// 电机控制引脚 (通过CAN总线控制，不需要直接GPIO控制)
// CAN总线引脚定义:
// - TX: GPIO_NUM_16 (连接到SN65HVD232D的D引脚)
// - RX: GPIO_NUM_17 (连接到SN65HVD232D的R引脚)
````
</augment_code_snippet>

### UART接口配置

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// UART定义
#define UART_DEBUG              UART_NUM_0   // 调试串口 (通过CH340)
#define UART_CMD                UART_NUM_1   // CMD_VEL接收 (RX: GPIO_NUM_21)
#define UART_SBUS               UART_NUM_2   // SBUS接收 (RX: GPIO_NUM_22)
````
</augment_code_snippet>

### 连接说明

1. **SBUS接收**：
   - GPIO22 连接 SBUS信号线
   - 支持硬件反相，无需外部反相器

2. **CAN总线**：
   - GPIO16 (TX) 连接 SN65HVD232D的D引脚
   - GPIO17 (RX) 连接 SN65HVD232D的R引脚

3. **调试接口**：
   - USB Type-C连接到电脑
   - 自动识别为COM端口

## 🚀 首次编译

### 1. 使用批处理脚本编译

项目提供了便捷的批处理脚本：

```bash
# 仅编译项目
build_only.bat
```

### 2. 手动编译

如果需要手动编译：

```bash
# 设置ESP-IDF环境
call "C:\Espressif\frameworks\esp-idf-v5.4.1\export.bat"

# 配置项目
idf.py menuconfig

# 编译项目
idf.py build
```

### 3. 编译成功验证

编译成功后，应该看到以下输出文件：
- `build/esp32controlboard.bin` - 主应用程序
- `build/bootloader/bootloader.bin` - 引导程序
- `build/partition_table/partition-table.bin` - 分区表

## 📱 VS Code配置

### 1. 安装VS Code

下载并安装Visual Studio Code：
- **官方地址**: https://code.visualstudio.com/

### 2. 安装ESP-IDF扩展

1. 打开VS Code
2. 进入扩展市场 (Ctrl+Shift+X)
3. 搜索"ESP-IDF"
4. 安装"ESP-IDF"扩展

### 3. 配置ESP-IDF扩展

1. 按 `Ctrl+Shift+P` 打开命令面板
2. 输入"ESP-IDF: Configure ESP-IDF Extension"
3. 选择"Use Existing Setup"
4. 设置ESP-IDF路径：`C:\Espressif\frameworks\esp-idf-v5.4.1`
5. 设置Python路径：`C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe`

### 4. 打开项目

```bash
# 在VS Code中打开项目
code esp32controlboard
```

## 🔍 串口驱动安装

### 1. 识别USB芯片

常见的ESP32开发板使用以下USB转串口芯片：
- **CH340/CH341**: 最常见，需要安装驱动
- **CP2102/CP2104**: Silicon Labs芯片
- **FTDI**: FT232等系列

### 2. 安装CH340驱动

如果使用CH340芯片：
1. 下载CH340驱动：http://www.wch.cn/downloads/CH341SER_EXE.html
2. 以管理员身份运行安装程序
3. 重启计算机

### 3. 验证串口连接

```bash
# 检查可用串口
idf.py -p COM10 monitor

# 如果连接成功，应该看到ESP32的启动信息
```

## ✅ 环境验证

### 1. 编译测试

```bash
# 清理并重新编译
idf.py clean
idf.py build
```

### 2. 烧录测试

```bash
# 烧录到ESP32 (假设连接到COM10)
flash_com10.bat
```

### 3. 监控测试

```bash
# 监控串口输出
idf.py -p COM10 monitor
```

预期看到类似输出：
```
I (xxx) MAIN: System initialized
I (xxx) SBUS: ✅ UART2 initialized successfully:
I (xxx) SBUS:    📍 RX Pin: GPIO22
I (xxx) SBUS:    📡 Config: 100000bps, 8E2
I (xxx) SBUS:    🔄 Signal inversion: ENABLED
```

## 🛠️ 常见问题解决

### 1. 编译错误

**问题**: `idf.py: command not found`
**解决**: 确保正确设置了ESP-IDF环境变量

**问题**: Python版本不兼容
**解决**: 使用Python 3.11.x版本

### 2. 烧录错误

**问题**: 无法连接到ESP32
**解决**: 
1. 检查USB线缆连接
2. 确认串口驱动已安装
3. 尝试按住BOOT键进入下载模式

**问题**: 权限不足
**解决**: 以管理员身份运行命令行

### 3. 串口问题

**问题**: 找不到COM端口
**解决**:
1. 检查设备管理器中的端口
2. 重新安装USB驱动
3. 更换USB线缆

## 📚 开发工具推荐

### 必备工具
- **ESP-IDF**: 官方开发框架
- **VS Code**: 代码编辑器
- **Git**: 版本控制
- **串口调试助手**: 用于调试串口通信

### 可选工具
- **Logic Analyzer**: 逻辑分析仪软件
- **CAN分析仪**: CAN总线调试工具
- **示波器软件**: 信号分析工具

---

💡 **提示**: 环境搭建完成后，建议先运行一个简单的示例项目验证环境是否正常工作！
