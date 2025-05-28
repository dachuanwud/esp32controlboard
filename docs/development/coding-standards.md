# 📝 ESP32控制板编码规范

本文档定义了ESP32控制板项目的编码标准和规范，确保代码的一致性、可读性和可维护性。

## 🎯 编码原则

### 核心原则
- **一致性**: 遵循统一的命名和格式规范
- **可读性**: 代码应该自解释，注释清晰
- **简洁性**: 避免过度复杂的设计
- **安全性**: 注重错误处理和边界检查

## 📁 文件组织规范

### 1. 文件命名

#### 源文件命名
- **模块文件**: `module_name.c/.h`
- **驱动文件**: `drv_device.c/.h`
- **工具文件**: `util_function.c/.h`

项目实例：
<augment_code_snippet path="main" mode="EXCERPT">
````
main/
├── main.c                # 主程序入口
├── main.h                # 全局定义和配置
├── sbus.c/.h            # SBUS协议模块
├── channel_parse.c/.h    # 通道解析模块
└── drv_keyadouble.c/.h  # 电机驱动模块
````
</augment_code_snippet>

### 2. 头文件结构

标准头文件模板：

```c
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// 宏定义
#define MODULE_MAX_SIZE 1024

// 类型定义
typedef struct {
    uint16_t value;
    bool valid;
} module_data_t;

// 函数声明
esp_err_t module_init(void);
esp_err_t module_process(module_data_t* data);

#endif /* MODULE_NAME_H */
```

### 3. 源文件结构

<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "channel_parse.h"
#include "drv_keyadouble.h"
#include "esp_log.h"

static const char *TAG = "CHAN_PARSE";

// 静态变量
static uint16_t last_ch_val[16] = {0};
static bool first_run = true;

// 静态函数声明
static int8_t chg_val(uint16_t val);
static bool check_channel_changed(uint16_t* ch_val);

// 函数实现
````
</augment_code_snippet>

## 🏷️ 命名规范

### 1. 变量命名

#### 全局变量
```c
// 使用g_前缀
static uint8_t g_sbus_rx_buf[LEN_SBUS] = {0};
static uint8_t g_sbus_pt = 0;
```

#### 局部变量
```c
// 使用描述性名称
int8_t speed_left = 0;
int8_t speed_right = 0;
uint16_t channel_value = 0;
bool channels_changed = false;
```

#### 静态变量
<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
// 保存上一次的通道值，用于变化检测
static uint16_t last_ch_val[16] = {0};
static bool first_run = true;
````
</augment_code_snippet>

### 2. 函数命名

#### 公共接口函数
<augment_code_snippet path="main/sbus.h" mode="EXCERPT">
````c
/**
 * 初始化SBUS接收
 * @return ESP_OK=成功
 */
esp_err_t sbus_init(void);

/**
 * 解析SBUS数据，按照标准SBUS协议解析16个通道
 * @param sbus_data SBUS原始数据（25字节）
 * @param channel 输出的通道值数组（16个通道，每个通道0-2047）
 * @return 0=成功
 */
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel);
````
</augment_code_snippet>

#### 静态函数
<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
/**
 * 将通道值转换为速度值
 * @param val 通道值(1050~1950)
 * @return 速度值(-100~100)
 */
static int8_t chg_val(uint16_t val)

/**
 * 计算差速转弯的速度偏移
 * @param v1 主速度分量（前后）
 * @param v2 转向速度分量（左右）
 * @return 偏移后的速度值
 */
static int8_t cal_offset(int8_t v1, int8_t v2)
````
</augment_code_snippet>

### 3. 宏定义命名

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// GPIO引脚定义 - 使用描述性名称
#define LED1_RED_PIN            GPIO_NUM_12
#define LED1_GREEN_PIN          GPIO_NUM_13
#define LED1_BLUE_PIN           GPIO_NUM_14

// UART定义
#define UART_DEBUG              UART_NUM_0
#define UART_CMD                UART_NUM_1
#define UART_SBUS               UART_NUM_2

// 通道阈值定义
#define CHANNEL_THRESHOLD       10
````
</augment_code_snippet>

### 4. 类型定义

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 队列数据结构
typedef struct {
    uint16_t channel[LEN_CHANEL];
} sbus_data_t;

typedef struct {
    int8_t speed_left;
    int8_t speed_right;
} motor_cmd_t;
````
</augment_code_snippet>

## 📝 注释规范

### 1. 文件头注释

```c
/**
 * @file channel_parse.c
 * @brief 通道数据解析和控制逻辑实现
 * @author ESP32控制板项目组
 * @date 2024-05-28
 * @version 1.0
 * 
 * 本文件实现SBUS通道数据的解析和履带车差速控制逻辑
 */
```

### 2. 函数注释

<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
/**
 * 解析通道值并控制履带车运动
 * 标准SBUS协议：1050~1950映射到-100~100，1500对应0
 * 履带车差速控制：通过左右履带速度差实现转弯
 *
 * 通道分配：
 * - 通道0 (ch_val[0]): 左右方向控制，右>0
 * - 通道2 (ch_val[2]): 前后方向控制，前>0
 * - 通道3 (ch_val[3]): 备用左右方向控制（单手模式）
 * - 通道6 (ch_val[6]): 单手模式开关，1950时启用
 * - 通道7 (ch_val[7]): 低速模式开关，1950时启用
 */
uint8_t parse_chan_val(uint16_t* ch_val)
````
</augment_code_snippet>

### 3. 行内注释

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 创建FreeRTOS队列
sbus_queue = xQueueCreate(5, sizeof(sbus_data_t));
cmd_queue = xQueueCreate(5, sizeof(motor_cmd_t));

// 检查队列创建是否成功
if (sbus_queue == NULL || cmd_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create queues");
    return;
}
````
</augment_code_snippet>

### 4. 算法注释

<augment_code_snippet path="main/channel_parse.c" mode="EXCERPT">
````c
/**
 * 将通道值转换为速度值
 * 标准SBUS协议：输入范围1050~1950，中位值1500，映射到-100~100
 * 优化算法：直接映射，无需范围限制
 * @param val 通道值(1050~1950)
 * @return 速度值(-100~100)
 */
static int8_t chg_val(uint16_t val)
{
    // 优化的映射算法：(val-1500)/9*2，范围900/9*2=200，即-100到+100
    int8_t sp = (((int16_t)val - 1500) / 9 * 2) & 0xff;
    return sp;
}
````
</augment_code_snippet>

## 🔧 代码格式规范

### 1. 缩进和空格

```c
// 使用4个空格缩进，不使用Tab
if (condition) {
    function_call();
    another_function();
}

// 运算符前后加空格
int result = a + b * c;
bool flag = (value > 0) && (value < 100);

// 逗号后加空格
function(param1, param2, param3);
```

### 2. 大括号风格

```c
// 函数大括号换行
void function_name(void)
{
    // 函数体
}

// 控制结构大括号不换行
if (condition) {
    // 代码块
} else {
    // 代码块
}

// switch语句
switch (value) {
    case 1:
        // 处理
        break;
    case 2:
        // 处理
        break;
    default:
        // 默认处理
        break;
}
```

### 3. 行长度限制

```c
// 单行不超过100字符，超长行需要换行
esp_err_t very_long_function_name(int parameter1, 
                                  int parameter2,
                                  int parameter3)
{
    // 函数实现
}

// 长字符串换行
ESP_LOGI(TAG, "This is a very long log message that needs to be "
              "split across multiple lines for better readability");
```

## 🛡️ 错误处理规范

### 1. 返回值检查

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 检查任务创建结果
BaseType_t xReturned = xTaskCreate(
    sbus_process_task,
    "sbus_task",
    4096,
    NULL,
    12,
    &sbus_task_handle);

if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Failed to create SBUS task");
}
````
</augment_code_snippet>

### 2. 参数验证

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // 参数范围检查
    if ((abs(speed_left) > 100) || (abs(speed_right) > 100)) {
        printf("Wrong parameter in intf_move!!![lf=%d],[ri=%d]", speed_left, speed_right);
        return 1;
    }
    
    // 正常处理逻辑
    return 0;
}
````
</augment_code_snippet>

### 3. 资源管理

```c
// 动态内存分配检查
char *buffer = malloc(size);
if (buffer == NULL) {
    ESP_LOGE(TAG, "Memory allocation failed");
    return ESP_ERR_NO_MEM;
}

// 使用完毕后释放
free(buffer);
buffer = NULL;
```

## 📊 性能编码规范

### 1. 内存使用

```c
// 优先使用静态分配
static uint8_t buffer[1024];

// 避免频繁的malloc/free
// 使用内存池或预分配策略

// 大数组使用堆分配
uint8_t *large_buffer = heap_caps_malloc(LARGE_SIZE, MALLOC_CAP_8BIT);
```

### 2. 中断安全

```c
// 中断服务程序标记
void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // 中断处理代码
    // 避免使用printf等阻塞函数
}

// 关键函数放在IRAM中
void IRAM_ATTR critical_function(void)
{
    // 时间敏感的代码
}
```

### 3. 任务优先级

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// SBUS处理任务 - 高优先级
xReturned = xTaskCreate(
    sbus_process_task,
    "sbus_task",
    4096,
    NULL,
    12,  // 高优先级
    &sbus_task_handle);

// 电机控制任务 - 中优先级  
xReturned = xTaskCreate(
    motor_control_task,
    "motor_task",
    4096,
    NULL,
    10,  // 中优先级
    &control_task_handle);
````
</augment_code_snippet>

## 🔍 代码审查清单

### 1. 功能性检查
- [ ] 函数功能是否正确实现
- [ ] 边界条件是否正确处理
- [ ] 错误情况是否有适当处理
- [ ] 内存泄漏检查

### 2. 可读性检查
- [ ] 变量和函数命名是否清晰
- [ ] 注释是否充分和准确
- [ ] 代码结构是否清晰
- [ ] 复杂逻辑是否有解释

### 3. 性能检查
- [ ] 是否有不必要的计算
- [ ] 内存使用是否合理
- [ ] 是否有潜在的阻塞操作
- [ ] 中断处理是否高效

### 4. 安全性检查
- [ ] 缓冲区溢出检查
- [ ] 空指针检查
- [ ] 整数溢出检查
- [ ] 并发安全检查

## 📚 工具和自动化

### 1. 代码格式化

```bash
# 使用clang-format格式化代码
clang-format -i *.c *.h

# VS Code自动格式化
# 设置保存时自动格式化
```

### 2. 静态分析

```bash
# 使用ESP-IDF内置的静态分析
idf.py check

# 使用cppcheck
cppcheck --enable=all --std=c99 main/
```

### 3. 代码度量

```bash
# 代码行数统计
find . -name "*.c" -o -name "*.h" | xargs wc -l

# 复杂度分析
lizard main/
```

---

💡 **提示**: 编码规范的目标是提高代码质量和团队协作效率，应该在项目开始时就严格执行！
