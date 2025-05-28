# 🔧 模块说明文档

本目录包含ESP32控制板项目中各个功能模块的详细说明，涵盖模块设计、接口定义、配置方法和使用指南。

## 📋 文档列表

### 🔌 UART调试模块
- **uart-debug-mechanism.md** - 串口打印机制详解
  - UART配置和初始化
  - 调试信息输出机制
  - 日志级别和格式控制
  - 性能优化和注意事项

## 🎯 模块架构

### 核心模块
- **SBUS接收模块**: 遥控信号接收和解析
- **CAN通信模块**: 电机驱动器通信
- **UART调试模块**: 系统调试和日志输出
- **GPIO控制模块**: 通用输入输出控制
- **定时器模块**: 系统定时和计数功能

### 模块关系图
```
┌─────────────────────────────────────────────────────────────┐
│                      模块架构图                              │
├─────────────────────────────────────────────────────────────┤
│  应用模块  │  控制逻辑  │  数据处理  │  状态管理  │  用户接口  │
├─────────────────────────────────────────────────────────────┤
│  通信模块  │  SBUS模块  │  CAN模块   │  UART模块  │  GPIO模块  │
├─────────────────────────────────────────────────────────────┤
│  驱动模块  │  UART驱动  │  TWAI驱动  │  GPIO驱动  │  定时器驱动│
├─────────────────────────────────────────────────────────────┤
│  硬件层   │           ESP32芯片和外设                        │
└─────────────────────────────────────────────────────────────┘
```

## 📡 SBUS接收模块

### 功能特性
- **协议支持**: 标准SBUS协议 (100kbps, 8E2)
- **信号处理**: 硬件反相，无需外部电路
- **数据解析**: 16通道11位数据提取
- **实时性**: 14ms更新周期

### 接口定义
```c
// SBUS初始化
void sbus_init(void);

// SBUS数据获取
bool sbus_get_channels(uint16_t* channels);

// SBUS状态检查
bool sbus_is_connected(void);
```

### 配置参数
- **UART端口**: UART2
- **GPIO引脚**: GPIO22 (RX)
- **波特率**: 100000
- **数据格式**: 8E2 + 反相

## 📡 CAN通信模块

### 功能特性
- **协议支持**: CAN 2.0B扩展帧
- **波特率**: 250kbps
- **命令类型**: 使能命令、速度命令
- **多节点**: 支持多个电机驱动器

### 接口定义
```c
// CAN初始化
esp_err_t can_init(void);

// 电机控制命令
esp_err_t motor_enable(uint8_t channel);
esp_err_t motor_set_speed(uint8_t channel, int8_t speed);

// CAN状态检查
bool can_is_ready(void);
```

### 配置参数
- **TX引脚**: GPIO16
- **RX引脚**: GPIO17
- **波特率**: 250kbps
- **过滤器**: 接受所有帧

## 🔌 UART调试模块

### 功能特性
- **多级日志**: ERROR, WARN, INFO, DEBUG
- **格式化输出**: 时间戳、模块标签、颜色编码
- **性能优化**: 缓冲输出、非阻塞发送
- **配置灵活**: 运行时日志级别调整

### 接口定义
```c
// 日志输出宏
ESP_LOGE(TAG, "Error message");
ESP_LOGW(TAG, "Warning message");
ESP_LOGI(TAG, "Info message");
ESP_LOGD(TAG, "Debug message");

// 日志级别设置
esp_log_level_set(TAG, ESP_LOG_INFO);
```

### 配置参数
- **UART端口**: UART0
- **波特率**: 115200
- **数据格式**: 8N1
- **缓冲区**: 1024字节

## 🔧 GPIO控制模块

### 功能特性
- **输入检测**: 按键、开关状态检测
- **输出控制**: LED、继电器控制
- **中断支持**: 边沿触发中断
- **防抖处理**: 软件防抖算法

### 接口定义
```c
// GPIO初始化
esp_err_t gpio_init_pin(gpio_num_t pin, gpio_mode_t mode);

// GPIO读写
int gpio_read(gpio_num_t pin);
void gpio_write(gpio_num_t pin, int level);

// 中断配置
esp_err_t gpio_set_interrupt(gpio_num_t pin, gpio_int_type_t type);
```

### 引脚分配
- **SBUS输入**: GPIO22
- **CAN_TX**: GPIO16
- **CAN_RX**: GPIO17
- **状态LED**: GPIO2
- **用户按键**: GPIO0

## ⏱️ 定时器模块

### 功能特性
- **高精度定时**: 微秒级精度
- **多定时器**: 支持多个独立定时器
- **中断回调**: 定时器中断处理
- **计数功能**: 脉冲计数和测量

### 接口定义
```c
// 定时器初始化
esp_err_t timer_init(timer_group_t group, timer_idx_t timer);

// 定时器启动/停止
esp_err_t timer_start(timer_group_t group, timer_idx_t timer);
esp_err_t timer_stop(timer_group_t group, timer_idx_t timer);

// 定时器回调
esp_err_t timer_set_callback(timer_group_t group, timer_idx_t timer, 
                            timer_isr_t callback);
```

## 📊 模块性能指标

### SBUS模块
- **数据更新率**: 71 Hz (14ms周期)
- **解析延迟**: < 1ms
- **CPU占用**: < 5%
- **内存占用**: < 1KB

### CAN模块
- **发送速率**: > 1000 帧/秒
- **发送延迟**: < 1ms
- **CPU占用**: < 3%
- **内存占用**: < 2KB

### UART调试模块
- **输出速率**: 115200 bps
- **缓冲延迟**: < 10ms
- **CPU占用**: < 2%
- **内存占用**: < 1KB

## 🛠️ 开发指南

### 模块扩展
1. **新增模块**: 遵循统一的接口设计规范
2. **配置管理**: 使用menuconfig进行参数配置
3. **错误处理**: 统一的错误码和处理机制
4. **文档更新**: 及时更新模块说明文档

### 调试方法
1. **日志输出**: 使用分级日志进行调试
2. **状态监控**: 定期输出模块状态信息
3. **性能测量**: 监控CPU和内存使用情况
4. **压力测试**: 验证模块在高负载下的表现

### 优化建议
1. **内存优化**: 合理使用静态和动态内存
2. **CPU优化**: 避免阻塞操作，使用中断和DMA
3. **功耗优化**: 合理使用睡眠模式和时钟管理
4. **实时性优化**: 优化任务优先级和调度策略

---

💡 **提示**: 在开发新模块时，请参考现有模块的设计模式和编码规范！
