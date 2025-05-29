# 💻 开发指南文档

本目录包含ESP32控制板项目的开发指南，涵盖开发环境搭建、编译烧录、调试方法、编码规范等开发相关的详细说明。

## 📋 待添加文档列表

### 🛠️ 环境搭建
- **setup-guide.md** - 开发环境搭建指南
  - ESP-IDF安装和配置
  - 工具链设置
  - IDE配置 (VS Code)
  - 依赖库安装

### 🔨 编译烧录
- **build-flash.md** - 编译烧录指南
  - 项目编译流程
  - 固件烧录方法
  - 批处理脚本使用
  - 常见问题解决

### 🐛 调试指南
- **debugging.md** - 调试方法和技巧
  - GDB调试器使用
  - 日志系统配置
  - 性能分析工具
  - 内存调试方法

### 📝 编码规范
- **coding-standards.md** - 编码标准和规范
  - C语言编码规范
  - 文件组织结构
  - 注释和文档规范
  - 版本控制规范

## 🎯 开发流程

### 1. 环境准备
```bash
# 安装ESP-IDF
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh

# 设置环境变量
. ./export.sh

# 验证安装
idf.py --version
```

### 2. 项目构建
```bash
# 配置项目
idf.py menuconfig

# 编译项目
idf.py build

# 烧录固件
idf.py -p COM10 flash

# 监控输出
idf.py -p COM10 monitor
```

### 3. 开发调试
```bash
# 清理重建
idf.py clean
idf.py build

# 仅编译
build_only.bat

# 烧录到指定端口
flash_com10.bat
```

## 🛠️ 开发工具

### 必需工具
- **ESP-IDF**: Espressif官方开发框架
- **Python**: 3.7或更高版本
- **Git**: 版本控制工具
- **CMake**: 构建系统

### 推荐工具
- **VS Code**: 代码编辑器
- **ESP-IDF Extension**: VS Code扩展
- **Serial Monitor**: 串口监控工具
- **Logic Analyzer**: 逻辑分析仪软件

### 调试工具
- **ESP-IDF Monitor**: 内置串口监控
- **GDB**: GNU调试器
- **Valgrind**: 内存检查工具
- **PlatformIO**: 集成开发环境

## 📁 项目结构

```
esp32controlboard/
├── main/                    # 主程序源码
│   ├── main.c              # 主程序入口
│   ├── sbus.c/.h           # SBUS模块
│   ├── channel_parse.c/.h  # 通道解析
│   └── drv_keyadouble.c/.h # 电机驱动
├── docs/                   # 项目文档
├── build/                  # 编译输出
├── CMakeLists.txt         # CMake配置
├── sdkconfig              # 项目配置
├── build_only.bat         # 编译脚本
└── flash_com10.bat        # 烧录脚本
```

## 🔧 配置管理

### menuconfig配置
```bash
# 打开配置界面
idf.py menuconfig

# 主要配置项
# - Serial flasher config (烧录配置)
# - Component config (组件配置)
# - FreeRTOS (实时系统配置)
# - ESP32-specific (芯片特定配置)
```

### 重要配置项
- **Flash size**: 16MB
- **Partition table**: Custom OTA partitions (16MB optimized)
- **CPU frequency**: 240MHz
- **FreeRTOS tick rate**: 1000Hz
- **Main task stack size**: 4096

## 📝 编码规范

### 文件命名
- 源文件: `module_name.c`
- 头文件: `module_name.h`
- 驱动文件: `drv_device.c/.h`
- 工具文件: `util_function.c/.h`

### 函数命名
```c
// 模块初始化
void module_init(void);

// 公共接口
esp_err_t module_function(param_t param);

// 私有函数
static void module_internal_function(void);

// 中断处理
void IRAM_ATTR module_isr_handler(void);
```

### 变量命名
```c
// 全局变量
static uint32_t g_module_counter;

// 局部变量
int local_value;
uint8_t buffer[256];

// 常量
#define MODULE_MAX_SIZE 1024
const char* MODULE_TAG = "MODULE";
```

### 注释规范
```c
/**
 * @brief 函数简要描述
 *
 * @param param1 参数1描述
 * @param param2 参数2描述
 * @return esp_err_t 返回值描述
 */
esp_err_t function_name(int param1, char* param2);

// 单行注释用于简单说明
int value = 0;  // 变量说明
```

## 🚀 性能优化

### 编译优化
```c
// 编译器优化选项
CONFIG_COMPILER_OPTIMIZATION_SIZE=y  // 优化代码大小
CONFIG_COMPILER_OPTIMIZATION_PERF=y  // 优化性能

// 关键函数优化
void IRAM_ATTR critical_function(void) {
    // 放在IRAM中执行，提高速度
}
```

### 内存优化
```c
// 使用静态分配
static uint8_t buffer[1024];

// 避免频繁malloc/free
// 使用内存池或预分配

// 监控内存使用
ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
```

### 实时性优化
```c
// 任务优先级设置
#define SBUS_TASK_PRIORITY    12  // 高优先级
#define MOTOR_TASK_PRIORITY   10  // 中优先级
#define MONITOR_TASK_PRIORITY 5   // 低优先级

// 中断优先级
#define UART_INTR_PRIORITY    3   // 中断优先级
```

## 🔍 调试技巧

### 日志调试
```c
// 设置日志级别
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("SBUS", ESP_LOG_DEBUG);

// 使用日志宏
ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
ESP_LOGW(TAG, "Warning: value=%d", value);
ESP_LOGI(TAG, "Info: %s initialized", module_name);
ESP_LOGD(TAG, "Debug: data=0x%02X", data);
```

### 断点调试
```bash
# 启动GDB调试
idf.py gdb

# 设置断点
(gdb) break main.c:100
(gdb) break function_name

# 运行程序
(gdb) continue

# 查看变量
(gdb) print variable_name
(gdb) info locals
```

### 性能分析
```c
// 时间测量
uint32_t start_time = esp_timer_get_time();
// 执行代码
uint32_t end_time = esp_timer_get_time();
ESP_LOGI(TAG, "Execution time: %d us", end_time - start_time);

// 任务监控
void monitor_task_stats(void) {
    char *buffer = malloc(2048);
    vTaskList(buffer);
    ESP_LOGI(TAG, "Task stats:\n%s", buffer);
    free(buffer);
}
```

## 📚 学习资源

### 官方文档
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)

### 社区资源
- [ESP32 Forum](https://esp32.com/)
- [GitHub ESP-IDF](https://github.com/espressif/esp-idf)
- [ESP32 Examples](https://github.com/espressif/esp-idf/tree/master/examples)

### 开发工具
- [ESP-IDF Tools](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- [VS Code ESP-IDF Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)

---

💡 **提示**: 开发过程中遇到问题，请优先查阅官方文档和示例代码！
