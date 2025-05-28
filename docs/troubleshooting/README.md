# 🔍 故障排除文档

本目录包含ESP32控制板项目开发和使用过程中的故障排除指南，涵盖常见问题、调试方法、测试工具和解决方案。

## 📋 文档列表

### 🔌 硬件测试
- **usb-ttl-testing-guide.md** - USB转TTL测试说明
  - 硬件连接和配置
  - 通信测试方法
  - 常见问题和解决方案
  - 测试工具使用指南

## 🎯 故障分类

### 硬件相关问题
- **连接问题**: 线缆、接插件、焊接
- **电源问题**: 供电不足、电压异常
- **信号问题**: 信号完整性、干扰
- **器件问题**: 芯片损坏、外设故障

### 软件相关问题
- **编译问题**: 环境配置、依赖缺失
- **烧录问题**: 端口选择、驱动安装
- **运行问题**: 程序异常、死机重启
- **通信问题**: 协议错误、数据丢失

### 系统集成问题
- **时序问题**: 任务调度、中断冲突
- **资源问题**: 内存不足、栈溢出
- **性能问题**: CPU占用、响应延迟
- **兼容问题**: 版本不匹配、配置冲突

## 🔧 常见问题快速索引

### 编译和烧录问题

#### 问题1: 编译失败
**症状**: 编译过程中出现错误
**可能原因**:
- ESP-IDF环境未正确配置
- 依赖库缺失或版本不匹配
- 代码语法错误

**解决方案**:
```bash
# 检查ESP-IDF环境
idf.py --version

# 清理重新编译
idf.py clean
idf.py build

# 检查配置
idf.py menuconfig
```

#### 问题2: 烧录失败
**症状**: 无法连接到ESP32或烧录中断
**可能原因**:
- USB驱动未安装
- 端口选择错误
- ESP32未进入下载模式

**解决方案**:
```bash
# 检查端口
idf.py -p COM10 monitor

# 手动进入下载模式
# 按住BOOT键，按下RESET键，松开RESET键，松开BOOT键

# 使用正确端口烧录
idf.py -p COM10 flash
```

### 运行时问题

#### 问题3: 程序无输出
**症状**: 烧录成功但串口无输出
**可能原因**:
- 串口配置错误
- 程序卡死或异常
- 硬件连接问题

**解决方案**:
```bash
# 检查串口监控
idf.py -p COM10 monitor

# 检查波特率设置
# 确认为115200

# 重启ESP32
# 按下RESET键
```

#### 问题4: SBUS无数据
**症状**: SBUS接收不到遥控器数据
**可能原因**:
- 遥控器未开启或未绑定
- SBUS线缆连接错误
- UART配置参数错误

**解决方案**:
1. 检查遥控器状态和绑定
2. 确认SBUS线缆连接到GPIO22
3. 验证UART配置 (100000, 8E2, 反相)
4. 使用示波器检查SBUS信号

#### 问题5: CAN通信失败
**症状**: 无法发送CAN消息或电机无响应
**可能原因**:
- CAN收发器未连接
- 总线终端电阻缺失
- 波特率不匹配

**解决方案**:
1. 检查CAN收发器连接 (GPIO16/17)
2. 确认120Ω终端电阻
3. 验证波特率设置 (250kbps)
4. 使用CAN分析仪检查总线状态

## 🛠️ 调试工具和方法

### 硬件调试工具
- **万用表**: 电压、电流、电阻测量
- **示波器**: 信号波形分析
- **逻辑分析仪**: 数字信号时序分析
- **CAN分析仪**: CAN总线协议分析

### 软件调试工具
- **ESP-IDF Monitor**: 串口日志监控
- **GDB调试器**: 程序断点调试
- **Heap Trace**: 内存泄漏检测
- **Task Monitor**: 任务状态监控

### 调试技巧

#### 1. 日志调试
```c
// 使用分级日志
ESP_LOGE(TAG, "Critical error: %d", error_code);
ESP_LOGW(TAG, "Warning: %s", warning_msg);
ESP_LOGI(TAG, "Info: %d", info_value);
ESP_LOGD(TAG, "Debug: %x", debug_data);

// 设置日志级别
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("SBUS", ESP_LOG_DEBUG);
```

#### 2. 断言检查
```c
// 参数检查
assert(ptr != NULL);
assert(value >= 0 && value <= 100);

// ESP-IDF断言
ESP_ERROR_CHECK(esp_err);
```

#### 3. 状态监控
```c
// 任务状态监控
void monitor_task(void *pvParameters) {
    while (1) {
        // 输出系统状态
        ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min free heap: %d", esp_get_minimum_free_heap_size());
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## 📊 性能监控

### 系统资源监控
```c
// CPU使用率监控
void print_task_stats(void) {
    char *task_list_buffer = malloc(2048);
    vTaskList(task_list_buffer);
    ESP_LOGI(TAG, "Task List:\n%s", task_list_buffer);
    free(task_list_buffer);
}

// 内存使用监控
void print_memory_stats(void) {
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
```

### 通信质量监控
```c
// SBUS数据质量
static uint32_t sbus_frame_count = 0;
static uint32_t sbus_error_count = 0;

void sbus_quality_check(void) {
    float success_rate = (float)(sbus_frame_count - sbus_error_count) / sbus_frame_count * 100;
    ESP_LOGI(TAG, "SBUS success rate: %.2f%%", success_rate);
}
```

## 🚨 紧急故障处理

### 系统死机
1. **立即操作**: 断电重启
2. **检查日志**: 查看最后的错误信息
3. **分析原因**: 栈溢出、内存泄漏、死锁
4. **临时方案**: 增加看门狗保护

### 通信中断
1. **检查连接**: 验证硬件连接
2. **重置模块**: 重新初始化通信模块
3. **降级运行**: 切换到安全模式
4. **报警提示**: 通知用户故障状态

### 数据异常
1. **数据校验**: 检查数据完整性
2. **重新获取**: 重新读取数据源
3. **默认值**: 使用安全默认值
4. **错误记录**: 记录异常事件

## 📞 技术支持

### 获取帮助
- **文档查阅**: 优先查阅相关技术文档
- **社区支持**: ESP32开发者社区和论坛
- **官方支持**: Espressif官方技术支持
- **开源项目**: GitHub相关开源项目

### 问题报告
提交问题时请包含以下信息：
1. **硬件版本**: ESP32型号和开发板信息
2. **软件版本**: ESP-IDF版本和项目版本
3. **问题描述**: 详细的问题现象和复现步骤
4. **错误日志**: 完整的错误日志和调试信息
5. **环境信息**: 开发环境和工具链版本

---

💡 **提示**: 遇到问题时，请先查阅相关文档，然后尝试基本的排查步骤！
