# ESP32端日志系统使用指南

## 🎯 概述

ESP32端Supabase集成项目现已配备完整的日志系统，提供详细的运行状态监控、调试信息和错误追踪功能。

## 📊 日志级别说明

### 日志级别分类

| 级别 | 符号 | 描述 | 使用场景 |
|------|------|------|----------|
| ERROR | ❌ | 错误信息 | 系统错误、功能失败 |
| WARN | ⚠️ | 警告信息 | 潜在问题、非致命错误 |
| INFO | ✅ | 信息日志 | 重要状态变化、成功操作 |
| DEBUG | 🔍 | 调试信息 | 详细执行流程、变量值 |

### 模块日志配置

#### 核心模块（INFO级别）
- `CLOUD_CLIENT` - 云客户端操作
- `DATA_INTEGRATION` - 数据集成模块
- `WIFI_MANAGER` - Wi-Fi管理
- `HTTP_SERVER` - HTTP服务器
- `MAIN` - 主程序流程

#### 业务模块（WARN级别）
- `SBUS` - SBUS通信
- `CAN` - CAN总线
- `MOTOR` - 电机控制
- `OTA` - OTA更新

#### 系统模块（WARN级别）
- `wifi` - ESP-IDF Wi-Fi驱动
- `tcpip_adapter` - TCP/IP适配器
- `httpd_*` - HTTP服务器内部

## 🔧 日志配置函数

### 基础配置

```c
// 配置默认日志级别
configure_logging();

// 启用详细调试日志（开发阶段）
enable_debug_logging();

// 启用生产环境日志（部署阶段）
enable_production_logging();
```

### 系统信息打印

```c
// 打印完整系统信息
print_system_info();

// 打印网络状态
print_network_status();

// 打印云服务状态
print_cloud_status();
```

## 📝 日志输出示例

### 系统启动日志

```
I (1234) LOG_CONFIG: 🔧 配置日志系统...
I (1235) LOG_CONFIG: ✅ 日志系统配置完成

╔══════════════════════════════════════════════════════════════╗
║                    ESP32 控制板系统信息                        ║
╠══════════════════════════════════════════════════════════════╣
║ 📱 应用名称: ESP32控制板                                      ║
║ 🔢 应用版本: 2.1.0                                           ║
║ 📅 编译时间: 12:34:56                                        ║
║ 📅 编译日期: Jan 15 2024                                     ║
║ 💾 芯片型号: ESP32 (Rev 3)                                   ║
║ 🔧 CPU核心数: 2                                              ║
║ 📡 Wi-Fi支持: 是                                             ║
║ 📶 蓝牙支持: 是                                              ║
║ 💾 可用堆内存: 298765                                        ║
║ 💾 最小堆内存: 285432                                        ║
║ 🔗 MAC地址: 24:6f:28:12:34:56                               ║
║ ⏰ 运行时间: 123秒                                            ║
╚══════════════════════════════════════════════════════════════╝
```

### 云客户端日志

```
I (2345) CLOUD_CLIENT: 🌐 初始化云客户端...
I (2346) CLOUD_CLIENT: 📍 服务器地址: http://www.nagaflow.top
I (2347) CLOUD_CLIENT: 📍 Supabase项目: https://hfmifzmuwcmtgyjfhxvx.supabase.co
I (2348) CLOUD_CLIENT: 🆔 生成设备ID: esp32-a1b2c3d4
I (2349) CLOUD_CLIENT: 📋 设备名称: ESP32控制板-a1b2c3d4
I (2350) CLOUD_CLIENT: 📋 MAC地址: 24:6f:28:12:34:56
I (2351) CLOUD_CLIENT: ✅ 云客户端初始化完成
```

### 状态上报日志

```
I (3456) CLOUD_CLIENT: 📊 状态上报任务已启动
I (3457) CLOUD_CLIENT: ⏰ 上报间隔: 30秒
D (3458) CLOUD_CLIENT: 🔄 开始第1次状态收集...
D (3459) DATA_INTEGRATION: 📊 开始收集设备状态数据...
D (3460) DATA_INTEGRATION: 💾 系统信息 - 堆内存: 298765/285432, 运行时间: 123s, 任务数: 8
D (3461) DATA_INTEGRATION: 🌐 Wi-Fi IP: 192.168.1.100
D (3462) DATA_INTEGRATION: 📶 Wi-Fi信号强度: -45 dBm
I (3463) DATA_INTEGRATION: ✅ 设备状态收集完成
D (3464) CLOUD_CLIENT: 📤 开始发送设备状态到Supabase...
D (3465) CLOUD_CLIENT: 📏 JSON数据大小: 1234字节
I (3466) CLOUD_CLIENT: ✅ 状态上报成功 [1/1] - 成功率: 100.0%
```

### 错误处理日志

```
W (4567) CLOUD_CLIENT: ⚠️ 状态上报失败 [1/2]: HTTP错误: 500
I (4568) CLOUD_CLIENT: 🔄 尝试重连 (第1次)...
I (4569) CLOUD_CLIENT: 📡 开始注册设备到云服务器...
E (4570) CLOUD_CLIENT: ❌ 设备注册失败，HTTP错误
W (4571) CLOUD_CLIENT: ⚠️ 设备注册失败，将在后台重试
```

## 🔍 调试技巧

### 1. 启用详细调试

在开发阶段，可以启用详细调试日志：

```c
// 在main.c中取消注释
enable_debug_logging();
```

### 2. 模块特定调试

针对特定模块启用调试：

```c
esp_log_level_set("CLOUD_CLIENT", ESP_LOG_DEBUG);
esp_log_level_set("DATA_INTEGRATION", ESP_LOG_DEBUG);
```

### 3. 运行时状态检查

```c
// 检查云服务状态
const char* error = cloud_client_get_last_error();
if (strlen(error) > 0) {
    ESP_LOGE(TAG, "云客户端错误: %s", error);
}

// 检查网络状态
network_status_t status = cloud_client_get_network_status();
ESP_LOGI(TAG, "网络状态: %d", status);
```

## 📈 性能监控

### 内存使用监控

```
D (5678) DATA_INTEGRATION: 💾 系统信息 - 堆内存: 298765/285432, 运行时间: 456s, 任务数: 8
```

### 网络性能监控

```
I (6789) CLOUD_CLIENT: ✅ 状态上报成功 [45/50] - 成功率: 90.0%
D (6790) CLOUD_CLIENT: 📏 JSON数据大小: 1234字节
D (6791) CLOUD_CLIENT: 📥 HTTP响应 - 状态码: 200, 内容长度: 56
```

### 数据收集监控

```
D (7890) MAIN: 🎮 SBUS状态回调 - 连接: 是, 数据年龄: 125ms
D (7891) MAIN: 🚗 电机状态回调 - 左: 50, 右: -30, 数据年龄: 89ms
D (7892) MAIN: 🚌 CAN状态回调 - 连接: 否, TX: 0, RX: 0
```

## 🛠️ 故障排除

### 常见问题诊断

1. **设备注册失败**
   ```
   E (1234) CLOUD_CLIENT: ❌ 设备注册失败，HTTP错误
   ```
   - 检查网络连接
   - 验证Supabase配置
   - 确认服务器可达性

2. **状态上报失败**
   ```
   W (2345) CLOUD_CLIENT: ⚠️ 状态上报失败 [1/5]: HTTP错误: 500
   ```
   - 检查JSON数据格式
   - 验证API密钥
   - 检查服务器状态

3. **数据收集异常**
   ```
   W (3456) DATA_INTEGRATION: ⚠️ 获取SBUS状态失败
   ```
   - 检查回调函数实现
   - 验证硬件连接
   - 确认数据源状态

### 日志分析工具

使用ESP-IDF监控工具：

```bash
# 实时监控日志
idf.py monitor

# 过滤特定模块日志
idf.py monitor | grep "CLOUD_CLIENT"

# 保存日志到文件
idf.py monitor > esp32_logs.txt
```

## 📋 最佳实践

### 1. 生产环境配置

```c
// 生产环境使用精简日志
enable_production_logging();

// 只保留关键错误和警告
esp_log_level_set("*", ESP_LOG_WARN);
esp_log_level_set("MAIN", ESP_LOG_INFO);
esp_log_level_set("CLOUD_CLIENT", ESP_LOG_INFO);
```

### 2. 开发环境配置

```c
// 开发环境使用详细日志
enable_debug_logging();

// 启用所有调试信息
esp_log_level_set("CLOUD_CLIENT", ESP_LOG_DEBUG);
esp_log_level_set("DATA_INTEGRATION", ESP_LOG_DEBUG);
```

### 3. 日志存储管理

- 定期清理日志文件
- 监控存储空间使用
- 设置日志轮转策略

---

**🎉 ESP32端日志系统配置完成！**

通过详细的日志记录，您可以轻松监控设备运行状态、诊断问题和优化性能。日志系统为ESP32设备的云端集成提供了强大的调试和监控能力。
