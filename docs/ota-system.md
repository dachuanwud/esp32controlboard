# 🔄 ESP32控制板Web OTA系统技术文档

## 📋 项目概述

ESP32控制板Web OTA系统是一个完整的固件无线更新解决方案，集成了ESP32端HTTP服务器、React前端界面和双分区OTA机制。该系统在保持现有SBUS通信、CAN总线控制等核心功能的基础上，新增了Web上位机功能。

## 🏗️ 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32控制板OTA系统                        │
├─────────────────────────────────────────────────────────────┤
│  Web前端     │  React + TypeScript + Bootstrap              │
│             │  设备监控 │ OTA更新 │ Wi-Fi配置 │ 实时状态    │
├─────────────────────────────────────────────────────────────┤
│  HTTP API   │  RESTful API接口                              │
│             │  /api/device/* │ /api/ota/* │ /api/wifi/*    │
├─────────────────────────────────────────────────────────────┤
│  ESP32端    │  HTTP服务器 │ OTA管理器 │ Wi-Fi管理器        │
│             │  现有功能: SBUS │ CAN总线 │ 电机控制         │
├─────────────────────────────────────────────────────────────┤
│  FreeRTOS   │  任务调度 │ 队列通信 │ 内存管理             │
├─────────────────────────────────────────────────────────────┤
│  硬件层     │  ESP32芯片 │ Wi-Fi模块 │ Flash存储          │
└─────────────────────────────────────────────────────────────┘
```

### 🔧 ESP32端架构

#### 新增模块

1. **Wi-Fi管理器 (wifi_manager.c/.h)**
   - Station模式连接
   - 自动重连机制
   - 状态监控和报告
   - 网络扫描功能

2. **HTTP服务器 (http_server.c/.h)**
   - 基于esp_http_server组件
   - RESTful API接口
   - CORS跨域支持
   - JSON数据交换

3. **OTA管理器 (ota_manager.c/.h)**
   - 双分区OTA机制
   - 固件验证和回滚
   - 进度监控
   - 安全性保护

#### FreeRTOS任务架构

| 任务名称 | 优先级 | 栈大小 | 功能描述 |
|----------|--------|--------|----------|
| sbus_task | 12 (高) | 4096 | SBUS信号接收和解析 |
| cmd_uart_task | 12 (高) | 2048 | CMD_VEL命令接收 |
| motor_task | 10 (中) | 4096 | 电机控制和CAN通信 |
| wifi_task | 8 (中) | 4096 | Wi-Fi连接管理 |
| http_task | 7 (中) | 4096 | HTTP服务器管理 |
| status_task | 5 (低) | 2048 | 系统状态监控 |

## 🌐 Web前端架构

### 技术栈

- **框架**: React 18 + TypeScript
- **UI库**: React Bootstrap 5
- **构建工具**: Vite
- **HTTP客户端**: Axios
- **路由**: React Router DOM

### 组件结构

```
src/
├── components/
│   ├── DeviceInfo.tsx      # 设备信息展示
│   ├── DeviceStatus.tsx    # 实时状态监控
│   ├── OTAUpdate.tsx       # OTA固件更新
│   └── WiFiSettings.tsx    # Wi-Fi网络配置
├── services/
│   └── api.ts              # API接口封装
├── App.tsx                 # 主应用组件
└── main.tsx               # 应用入口
```

## 📡 API接口规范

### 设备信息接口

#### GET /api/device/info
获取设备基本信息

**响应示例:**
```json
{
  "status": "success",
  "data": {
    "device_name": "ESP32 Control Board",
    "firmware_version": "1.0.0-OTA",
    "hardware_version": "v1.0",
    "chip_model": "ESP32-1",
    "flash_size": 2097152,
    "free_heap": 180000,
    "uptime_seconds": 3600,
    "mac_address": "AA:BB:CC:DD:EE:FF"
  }
}
```

#### GET /api/device/status
获取设备实时状态

**响应示例:**
```json
{
  "status": "success",
  "data": {
    "sbus_connected": true,
    "can_connected": true,
    "wifi_connected": true,
    "wifi_ip": "192.168.1.100",
    "wifi_rssi": -45,
    "sbus_channels": [1500, 1500, 1500, ...],
    "motor_left_speed": 0,
    "motor_right_speed": 0,
    "last_sbus_time": 1640995200000,
    "last_cmd_time": 1640995200000
  }
}
```

### OTA更新接口

#### POST /api/ota/upload
上传固件文件

**请求**: Binary firmware data (.bin file)
**响应**: 
```json
{
  "status": "success",
  "message": "OTA update completed successfully"
}
```

#### GET /api/ota/progress
获取OTA进度

**响应示例:**
```json
{
  "status": "success",
  "data": {
    "in_progress": true,
    "total_size": 1048576,
    "written_size": 524288,
    "progress_percent": 50,
    "status_message": "Writing firmware: 50%",
    "success": false
  }
}
```

### Wi-Fi配置接口

#### GET /api/wifi/status
获取Wi-Fi状态

#### POST /api/wifi/connect
连接Wi-Fi网络

**请求示例:**
```json
{
  "ssid": "MyWiFi",
  "password": "password123"
}
```

## 🔒 安全性设计

### OTA安全机制

1. **固件验证**
   - 文件大小检查 (最大1MB)
   - 格式验证 (.bin文件)
   - SHA256校验和验证

2. **双分区保护**
   - 使用ESP32的OTA分区机制
   - 自动回滚功能
   - 断电保护

3. **更新流程保护**
   - 原子性操作
   - 进度监控
   - 错误恢复

### 网络安全

1. **CORS配置**
   - 允许跨域访问
   - 安全头部设置

2. **输入验证**
   - 参数类型检查
   - 长度限制
   - 特殊字符过滤

## 🚀 部署和使用

### 构建流程

1. **ESP32固件构建**
   ```bash
   # 使用提供的脚本
   build_only.bat
   
   # 或手动构建
   idf.py build
   ```

2. **Web前端构建**
   ```bash
   # 使用提供的脚本
   build_web.bat
   
   # 或手动构建
   cd web_client
   npm install
   npm run build
   ```

3. **完整构建**
   ```bash
   # 一键构建ESP32固件和Web前端
   build_all_ota.bat
   ```

### 部署步骤

1. **初始固件烧录**
   ```bash
   flash_com10.bat
   ```

2. **Wi-Fi配置**
   - 修改main.c中的Wi-Fi凭据
   - 或通过Web界面配置

3. **Web界面访问**
   - 获取ESP32的IP地址
   - 浏览器访问: http://[ESP32_IP]

### 使用流程

1. **设备连接**
   - ESP32连接到Wi-Fi网络
   - 获取IP地址并记录

2. **Web界面操作**
   - 访问设备信息页面
   - 监控实时状态
   - 上传新固件进行OTA更新
   - 配置Wi-Fi网络

3. **OTA更新**
   - 选择.bin固件文件
   - 确认更新操作
   - 监控更新进度
   - 设备自动重启

## 🔧 开发和调试

### 开发环境

- ESP-IDF 5.4.1
- Node.js 18+
- TypeScript 5.2+
- React 18+

### 调试方法

1. **ESP32端调试**
   ```bash
   idf.py monitor
   ```

2. **Web前端调试**
   ```bash
   cd web_client
   npm run dev
   ```

3. **API测试**
   - 使用Postman或curl测试API接口
   - 检查ESP32串口输出

### 常见问题

1. **Wi-Fi连接失败**
   - 检查SSID和密码
   - 确认网络可用性
   - 查看ESP32日志

2. **OTA更新失败**
   - 检查固件文件格式
   - 确认网络稳定性
   - 验证分区表配置

3. **Web界面无法访问**
   - 确认ESP32 IP地址
   - 检查防火墙设置
   - 验证HTTP服务器状态

## 📈 性能优化

### ESP32端优化

1. **内存管理**
   - 合理分配任务栈大小
   - 及时释放动态内存
   - 监控堆内存使用

2. **网络优化**
   - HTTP连接复用
   - 数据压缩传输
   - 超时设置优化

### Web端优化

1. **构建优化**
   - 代码分割和懒加载
   - 资源压缩和缓存
   - Tree shaking优化

2. **用户体验**
   - 加载状态指示
   - 错误处理和重试
   - 响应式设计

## 🔮 未来扩展

### 功能扩展

1. **高级OTA功能**
   - 固件签名验证
   - 增量更新
   - 批量设备管理

2. **监控增强**
   - 历史数据记录
   - 图表可视化
   - 告警通知

3. **安全增强**
   - 用户认证
   - HTTPS支持
   - 访问控制

### 架构优化

1. **微服务化**
   - 模块解耦
   - 独立部署
   - 负载均衡

2. **云端集成**
   - 远程管理
   - 数据同步
   - 固件分发

---

*本文档持续更新，如有问题请参考项目源码或联系开发团队。*
