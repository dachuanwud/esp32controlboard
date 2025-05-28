# 🐛 ESP32控制板调试指南

本文档详细介绍ESP32控制板项目的调试方法和技巧，包括日志系统、GDB调试、性能分析和内存调试等。

## 🎯 调试系统概述

ESP32控制板项目使用ESP-IDF的日志系统进行调试，支持多级日志输出、实时监控和性能分析。

## 📊 日志系统

### 1. 日志级别配置

项目中使用的日志级别：

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
static const char *TAG = "MAIN";

// 日志输出示例
ESP_LOGI(TAG, "System initialized");
ESP_LOGE(TAG, "Failed to create queues");
ESP_LOGW(TAG, "CMD队列已满");
ESP_LOGD(TAG, "Debug information");
````
</augment_code_snippet>

### 2. 日志级别说明

| 级别 | 宏 | 用途 | 颜色 |
|------|-----|------|------|
| ERROR | ESP_LOGE | 错误信息 | 红色 |
| WARN | ESP_LOGW | 警告信息 | 黄色 |
| INFO | ESP_LOGI | 一般信息 | 绿色 |
| DEBUG | ESP_LOGD | 调试信息 | 白色 |
| VERBOSE | ESP_LOGV | 详细信息 | 灰色 |

### 3. 运行时日志级别设置

```c
// 设置全局日志级别
esp_log_level_set("*", ESP_LOG_INFO);

// 设置特定模块日志级别
esp_log_level_set("SBUS", ESP_LOG_DEBUG);
esp_log_level_set("CHAN_PARSE", ESP_LOG_DEBUG);
esp_log_level_set("DRV_KEYADOUBLE", ESP_LOG_INFO);
```

### 4. 模块化日志标签

项目中各模块的日志标签：

<augment_code_snippet path="main/sbus.c" mode="EXCERPT">
````c
static const char *TAG = "SBUS";
````
</augment_code_snippet>

<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
static const char *TAG = "CHAN_PARSE";
````
</augment_code_snippet>

## 🔍 实时调试监控

### 1. 串口监控

```bash
# 启动串口监控
idf.py -p COM10 monitor

# 带过滤的监控
idf.py -p COM10 monitor | findstr "SBUS"
```

### 2. 系统状态监控

项目中的状态监控任务：

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
/**
 * 状态监控任务
 * 监控系统状态（LED显示功能已注销）
 */
static void status_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态监控任务已启动 (LED显示已注销)");
    
    while (1) {
        // 每2秒输出一次系统状态
        ESP_LOGI(TAG, "💓 System heartbeat - Free heap: %d bytes", 
                 esp_get_free_heap_size());
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
````
</augment_code_snippet>

### 3. 关键数据监控

SBUS数据接收监控：

```c
// 在sbus.c中添加调试输出
ESP_LOGI(TAG, "📦 SBUS frame received - Header: 0x%02X, Footer: 0x%02X", 
         g_sbus_rx_buf[0], g_sbus_rx_buf[24]);
```

通道变化监控：

<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
/**
 * 检查关键通道是否有变化
 */
static bool check_channel_changed(uint16_t* ch_val)
{
    // 检查关键控制通道：0(左右), 2(前后), 3(备用左右), 6(模式), 7(速度减半)
    uint8_t key_channels[] = {0, 2, 3, 6, 7};
    bool changed = false;

    for (int i = 0; i < 5; i++) {
        uint8_t ch = key_channels[i];
        if (last_ch_val[ch] != 0 && abs((int16_t)ch_val[ch] - (int16_t)last_ch_val[ch]) > CHANNEL_THRESHOLD) {
            ESP_LOGI(TAG, "📈 Channel %d changed: %d → %d (diff: %d)",
                     ch, last_ch_val[ch], ch_val[ch],
                     abs((int16_t)ch_val[ch] - (int16_t)last_ch_val[ch]));
            changed = true;
        }
    }
    return changed;
}
````
</augment_code_snippet>

## 🧠 内存调试

### 1. 堆内存监控

```c
// 添加到状态监控任务中
void print_memory_info(void)
{
    ESP_LOGI(TAG, "📊 Memory Status:");
    ESP_LOGI(TAG, "   Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "   Min free heap: %d bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "   Largest free block: %d bytes", 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
```

### 2. 任务栈监控

```c
// 检查任务栈使用情况
void print_task_stack_info(void)
{
    UBaseType_t stack_high_water_mark;
    
    // 检查SBUS任务栈
    if (sbus_task_handle != NULL) {
        stack_high_water_mark = uxTaskGetStackHighWaterMark(sbus_task_handle);
        ESP_LOGI(TAG, "SBUS task stack free: %d words", stack_high_water_mark);
    }
    
    // 检查控制任务栈
    if (control_task_handle != NULL) {
        stack_high_water_mark = uxTaskGetStackHighWaterMark(control_task_handle);
        ESP_LOGI(TAG, "Control task stack free: %d words", stack_high_water_mark);
    }
}
```

### 3. 内存泄漏检测

```c
// 启用堆内存跟踪
#include "esp_heap_trace.h"

void start_heap_trace(void)
{
    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
}

void stop_heap_trace(void)
{
    ESP_ERROR_CHECK(heap_trace_stop());
    heap_trace_dump();
}
```

## ⚡ 性能分析

### 1. 任务执行时间测量

```c
// 测量函数执行时间
uint64_t start_time = esp_timer_get_time();

// 执行要测量的代码
parse_chan_val(ch_val);

uint64_t end_time = esp_timer_get_time();
ESP_LOGI(TAG, "⏱️ parse_chan_val execution time: %lld us", end_time - start_time);
```

### 2. 队列性能监控

```c
// 监控队列状态
void monitor_queue_status(void)
{
    UBaseType_t sbus_queue_waiting = uxQueueMessagesWaiting(sbus_queue);
    UBaseType_t sbus_queue_spaces = uxQueueSpacesAvailable(sbus_queue);
    
    ESP_LOGI(TAG, "📊 SBUS Queue - Waiting: %d, Free: %d", 
             sbus_queue_waiting, sbus_queue_spaces);
    
    UBaseType_t cmd_queue_waiting = uxQueueMessagesWaiting(cmd_queue);
    UBaseType_t cmd_queue_spaces = uxQueueSpacesAvailable(cmd_queue);
    
    ESP_LOGI(TAG, "📊 CMD Queue - Waiting: %d, Free: %d", 
             cmd_queue_waiting, cmd_queue_spaces);
}
```

### 3. 任务运行统计

```c
// 获取任务运行时间统计
void print_task_stats(void)
{
    char *task_list_buffer = malloc(2048);
    if (task_list_buffer != NULL) {
        vTaskList(task_list_buffer);
        ESP_LOGI(TAG, "📋 Task List:\n%s", task_list_buffer);
        free(task_list_buffer);
    }
    
    char *run_time_buffer = malloc(2048);
    if (run_time_buffer != NULL) {
        vTaskGetRunTimeStats(run_time_buffer);
        ESP_LOGI(TAG, "⏱️ Task Runtime Stats:\n%s", run_time_buffer);
        free(run_time_buffer);
    }
}
```

## 🔧 GDB调试

### 1. 启用GDB调试

```bash
# 编译调试版本
idf.py -D CMAKE_BUILD_TYPE=Debug build

# 烧录并启动GDB
idf.py -p COM10 flash gdb
```

### 2. GDB基本命令

```gdb
# 设置断点
(gdb) break main.c:100
(gdb) break parse_chan_val

# 运行程序
(gdb) continue

# 查看变量
(gdb) print ch_val[0]
(gdb) print sbus_data

# 查看调用栈
(gdb) backtrace

# 单步执行
(gdb) step
(gdb) next

# 查看内存
(gdb) x/16x 0x3ffb0000
```

### 3. 远程调试配置

```bash
# 启动OpenOCD
openocd -f board/esp32-wrover-kit-3.3v.cfg

# 在另一个终端启动GDB
xtensa-esp32-elf-gdb build/esp32controlboard.elf
(gdb) target remote :3333
```

## 🚨 错误诊断

### 1. 常见错误模式

#### 任务创建失败
```c
BaseType_t xReturned = xTaskCreate(
    sbus_process_task,
    "sbus_task",
    4096,  // 栈大小
    NULL,
    12,    // 优先级
    &sbus_task_handle);

if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "❌ Failed to create SBUS task - Error: %d", xReturned);
    ESP_LOGE(TAG, "   Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGE(TAG, "   Required stack: %d bytes", 4096 * sizeof(StackType_t));
}
```

#### 队列操作失败
```c
if (xQueueSend(sbus_queue, &sbus_data, 0) != pdPASS) {
    ESP_LOGW(TAG, "⚠️ SBUS queue full - implementing overwrite strategy");
    sbus_data_t dummy;
    xQueueReceive(sbus_queue, &dummy, 0);
    xQueueSend(sbus_queue, &sbus_data, 0);
}
```

### 2. 系统重启分析

```c
// 检查重启原因
void print_reset_reason(void)
{
    esp_reset_reason_t reset_reason = esp_reset_reason();
    
    switch (reset_reason) {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "🔌 Reset reason: Power-on reset");
            break;
        case ESP_RST_SW:
            ESP_LOGI(TAG, "🔄 Reset reason: Software reset");
            break;
        case ESP_RST_PANIC:
            ESP_LOGE(TAG, "💥 Reset reason: Exception/panic");
            break;
        case ESP_RST_WDT:
            ESP_LOGE(TAG, "🐕 Reset reason: Watchdog timeout");
            break;
        default:
            ESP_LOGI(TAG, "❓ Reset reason: Unknown (%d)", reset_reason);
            break;
    }
}
```

### 3. 看门狗调试

```c
// 在长时间运行的任务中添加看门狗喂狗
void long_running_task(void *pvParameters)
{
    while (1) {
        // 执行任务逻辑
        process_data();
        
        // 喂看门狗
        esp_task_wdt_reset();
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## 🔍 协议调试

### 1. SBUS协议调试

```c
// 在sbus.c中添加详细的帧分析
void debug_sbus_frame(uint8_t* frame)
{
    ESP_LOGD(TAG, "🔍 SBUS Frame Analysis:");
    ESP_LOGD(TAG, "   Header: 0x%02X (expected: 0x0F)", frame[0]);
    ESP_LOGD(TAG, "   Footer: 0x%02X (expected: 0x00)", frame[24]);
    ESP_LOGD(TAG, "   Flags: 0x%02X", frame[23]);
    
    // 显示原始数据
    char hex_str[128];
    for (int i = 0; i < 25; i++) {
        sprintf(hex_str + i*3, "%02X ", frame[i]);
    }
    ESP_LOGD(TAG, "   Raw data: %s", hex_str);
}
```

### 2. CAN协议调试

```c
// 在drv_keyadouble.c中添加CAN发送调试
static void debug_can_message(uint32_t id, uint8_t* data)
{
    ESP_LOGD(TAG, "📡 CAN Message:");
    ESP_LOGD(TAG, "   ID: 0x%08X", id);
    ESP_LOGD(TAG, "   Data: %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3],
             data[4], data[5], data[6], data[7]);
}
```

## 📊 调试工具集成

### 1. VS Code调试配置

创建`.vscode/launch.json`：

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "ESP32 Debug",
            "type": "espidf",
            "request": "launch",
            "program": "${workspaceFolder}/build/esp32controlboard.elf",
            "port": "COM10",
            "console": "integratedTerminal"
        }
    ]
}
```

### 2. 逻辑分析仪集成

```c
// 添加GPIO调试信号
#define DEBUG_PIN_SBUS_RX    GPIO_NUM_2
#define DEBUG_PIN_CAN_TX     GPIO_NUM_4

// 在关键时刻切换调试引脚
void debug_signal_toggle(gpio_num_t pin)
{
    static int level = 0;
    level = !level;
    gpio_set_level(pin, level);
}
```

## 🎯 调试最佳实践

### 1. 分层调试策略

1. **硬件层**: 使用示波器检查信号
2. **驱动层**: 使用日志监控UART/CAN状态
3. **协议层**: 分析数据帧格式和内容
4. **应用层**: 监控业务逻辑和状态变化

### 2. 调试信息分级

- **ERROR**: 系统错误，影响正常功能
- **WARN**: 异常情况，但系统可继续运行
- **INFO**: 重要状态变化和里程碑事件
- **DEBUG**: 详细的执行流程和数据内容

### 3. 性能调试原则

- 避免在中断中使用printf
- 使用非阻塞的日志输出
- 定期监控内存和CPU使用率
- 使用条件编译控制调试代码

---

💡 **提示**: 调试时建议先使用日志系统定位问题范围，再使用GDB进行详细分析！
