# ESP32控制板设备管理功能修复方案

## 问题诊断结果

### 1. 主要问题
- **编译链接错误**：全局变量作用域问题导致HTTP服务器无法访问状态数据
- **API端点不完整**：缺少Wi-Fi扫描、连接等关键API
- **错误处理不完善**：字符串操作和空指针检查不充分
- **系统监控功能缺失**：缺少系统健康检查和性能监控

### 2. 具体问题分析
- `g_last_sbus_update`等变量在main.c中定义为static，导致http_server.c无法访问
- HTTP响应头CORS支持不完整
- 设备信息获取函数缺少安全的字符串操作
- 缺少系统健康状态监控API

## 解决方案实施

### 1. 修复编译链接问题

#### main.h 修改：
```c
// 添加全局变量声明
extern uint32_t g_last_sbus_update;
extern uint32_t g_last_motor_update;
extern uint16_t g_last_sbus_channels[16];
extern int8_t g_last_motor_left;
extern int8_t g_last_motor_right;
```

#### main.c 修改：
```c
// 将static变量改为全局变量
uint16_t g_last_sbus_channels[16] = {0};
int8_t g_last_motor_left = 0;
int8_t g_last_motor_right = 0;
uint32_t g_last_sbus_update = 0;
uint32_t g_last_motor_update = 0;
```

### 2. 完善HTTP服务器功能

#### 新增API端点：
1. **Wi-Fi扫描** - `GET /api/wifi/scan`
2. **Wi-Fi连接** - `POST /api/wifi/connect`
3. **系统健康检查** - `GET /api/device/health`

#### 改进的功能：
- **CORS支持**：完整的跨域资源共享头部
- **错误处理**：更安全的字符串操作和状态码处理
- **JSON响应**：统一的响应格式和错误信息

### 3. 新增系统健康监控

#### 健康检查指标：
```c
typedef struct {
    uint32_t uptime_seconds;      // 系统运行时间
    uint32_t free_heap;           // 可用内存
    uint32_t min_free_heap;       // 最小可用内存
    uint8_t cpu_usage_percent;    // CPU使用率
    float cpu_temperature;        // CPU温度
    bool watchdog_triggered;      // 看门狗状态
    uint32_t task_count;          // 任务数量
    bool wifi_healthy;            // Wi-Fi健康状态
    bool sbus_healthy;            // SBUS健康状态
    bool motor_healthy;           // 电机健康状态
} system_health_t;
```

#### 健康评分算法：
- 基础分：100分
- Wi-Fi异常：-20分
- SBUS异常：-30分
- 电机异常：-30分
- 内存不足：-10分
- CPU过载：-10分

### 4. 设备状态数据改进

#### 增强的设备信息：
- 使用应用描述获取版本信息
- 更详细的芯片信息（核心数）
- 安全的字符串操作
- 准确的MAC地址格式

#### 改进的状态监控：
- 实时的Wi-Fi连接状态和信号强度
- SBUS通道数据实时更新
- 电机速度状态监控
- 时间戳精确计算

## API接口说明

### 设备信息 - GET /api/device/info
```json
{
  "status": "success",
  "data": {
    "device_name": "ESP32 Control Board",
    "firmware_version": "1.0.0",
    "hardware_version": "v1.0",
    "chip_model": "ESP32-2核心",
    "flash_size": 4194304,
    "free_heap": 200000,
    "uptime_seconds": 3600,
    "mac_address": "AA:BB:CC:DD:EE:FF"
  }
}
```

### 设备状态 - GET /api/device/status
```json
{
  "status": "success",
  "data": {
    "sbus_connected": true,
    "can_connected": true,
    "wifi_connected": true,
    "wifi_ip": "192.168.1.100",
    "wifi_rssi": -45,
    "sbus_channels": [1500, 1500, ...],
    "motor_left_speed": 50,
    "motor_right_speed": -30,
    "last_sbus_time": 123456,
    "last_cmd_time": 123450
  }
}
```

### 系统健康检查 - GET /api/device/health
```json
{
  "status": "success",
  "data": {
    "uptime_seconds": 3600,
    "free_heap": 200000,
    "min_free_heap": 150000,
    "cpu_usage_percent": 25,
    "cpu_temperature": 45.0,
    "task_count": 12,
    "wifi_healthy": true,
    "sbus_healthy": true,
    "motor_healthy": true,
    "health_score": 100,
    "health_status": "excellent"
  }
}
```

### Wi-Fi连接 - POST /api/wifi/connect
请求：
```json
{
  "ssid": "WiFi_Name",
  "password": "wifi_password"
}
```

响应：
```json
{
  "status": "success",
  "message": "Connected to Wi-Fi"
}
```

## 实施步骤

### 1. 编译验证
```bash
cd esp32controlboard
idf.py build
```

### 2. 烧录测试
```bash
idf.py flash monitor
```

### 3. 功能测试
1. 连接到ESP32的Wi-Fi热点或确保ESP32连接到网络
2. 访问 `http://[ESP32_IP]/api/device/info` 测试设备信息API
3. 访问 `http://[ESP32_IP]/api/device/status` 测试设备状态API
4. 访问 `http://[ESP32_IP]/api/device/health` 测试系统健康检查

### 4. Web界面测试
确保前端能够正常调用这些API并显示设备信息。

## 注意事项

1. **内存管理**：注意JSON对象的正确释放，避免内存泄漏
2. **并发访问**：多个HTTP请求同时访问时的线程安全问题
3. **错误恢复**：网络断开重连、SBUS信号中断恢复等场景
4. **性能优化**：减少不必要的内存分配和字符串操作

## 后续改进建议

1. **实时监控**：添加WebSocket支持，实现实时数据推送
2. **日志系统**：完善系统日志记录和远程日志上传
3. **配置管理**：支持通过Web界面修改系统配置参数
4. **固件更新**：完善OTA更新机制，支持增量更新
5. **安全认证**：添加API访问认证和HTTPS支持 