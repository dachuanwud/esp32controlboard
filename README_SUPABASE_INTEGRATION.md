# ESP32控制板 Supabase集成方案

## 🎉 项目概述

本项目成功实现了基于Supabase的ESP32设备云管理系统，提供了完整的设备注册、状态监控、指令下发和实时数据展示功能。

## 🏗️ 系统架构

### 核心组件

1. **Supabase数据库** - 云端数据存储和实时订阅
2. **云服务器** (Node.js + Express) - API网关和业务逻辑
3. **React前端** - 用户界面和设备管理
4. **ESP32设备** - 物联网终端设备

### 数据流程

```
ESP32设备 ↔ 云服务器 ↔ Supabase数据库 ↔ React前端
```

## 📊 数据库设计

### 主要数据表

1. **esp32_devices** - 设备基本信息
   - 设备ID、名称、IP地址、硬件信息
   - 注册时间、最后活跃时间、在线状态

2. **device_status** - 设备状态历史
   - SBUS/CAN连接状态、WiFi信息
   - 系统资源、传感器数据
   - 时间戳记录

3. **device_commands** - 设备指令队列
   - 指令内容、执行状态
   - 创建时间、完成时间

### 视图和存储过程

- `device_details` - 设备详细信息视图
- `online_devices` - 在线设备视图
- `register_device()` - 设备注册存储过程
- `update_device_status()` - 状态更新存储过程
- `send_device_command()` - 指令发送存储过程

## 🚀 部署信息

### 云服务器
- **地址**: http://www.nagaflow.top
- **备用**: http://43.167.176.52
- **端口**: 3000

### Supabase项目
- **项目ID**: hfmifzmuwcmtgyjfhxvx
- **URL**: https://hfmifzmuwcmtgyjfhxvx.supabase.co
- **区域**: ap-southeast-1

## 🌐 前端功能

### 页面路由

1. **本地设备管理**
   - `/` - 设备信息
   - `/status` - 实时状态
   - `/ota` - OTA更新
   - `/wifi` - Wi-Fi设置

2. **云设备管理**
   - `/devices` - 云设备列表
   - `/cloud-status` - 云设备状态

### 核心功能

- ✅ 设备注册和管理
- ✅ 实时状态监控
- ✅ 指令下发和执行
- ✅ 历史数据查询
- ✅ 自动刷新和实时更新

## 🔧 API接口

### 设备管理
- `POST /register-device` - 设备注册
- `GET /devices` - 获取设备列表
- `GET /devices/online` - 获取在线设备
- `DELETE /api/devices/:deviceId` - 删除设备

### 状态管理
- `POST /device-status` - 更新设备状态
- `GET /api/device-status?deviceId=xxx` - 获取设备状态
- `GET /api/device-status-history?deviceId=xxx` - 获取状态历史

### 指令管理
- `POST /send-command` - 发送指令
- `GET /api/device-commands?deviceId=xxx` - 获取待处理指令

## 📱 ESP32集成

### 示例代码
参考 `esp32_code/supabase_integration_example.ino`

### 核心功能
1. **设备注册** - 启动时自动注册到云服务器
2. **状态上报** - 定期发送设备状态
3. **指令处理** - 接收并执行云端指令
4. **错误处理** - 网络异常和重连机制

### 支持的指令类型
- `led_control` - LED控制
- `motor_speed` - 电机速度控制
- `restart` - 设备重启
- `test_command` - 测试指令

## 🧪 测试和验证

### 模拟测试
运行 `cloud_server/test-esp32-simulation.js` 进行设备模拟测试：

```bash
cd cloud_server
node test-esp32-simulation.js
```

### 功能验证
1. ✅ 设备注册 - 多设备同时注册
2. ✅ 状态更新 - 实时数据上报
3. ✅ 指令下发 - 远程控制功能
4. ✅ 前端显示 - 实时界面更新

## 🔐 安全特性

1. **行级安全性** (RLS) - Supabase数据库安全策略
2. **CORS配置** - 跨域请求安全控制
3. **输入验证** - API参数验证和过滤
4. **错误处理** - 完善的异常处理机制

## 📈 性能优化

1. **数据库索引** - 关键字段建立索引
2. **连接池** - 数据库连接优化
3. **缓存策略** - 前端状态缓存
4. **批量操作** - 减少API调用次数

## 🛠️ 运维管理

### 服务器启动
```bash
cd cloud_server
node simple-supabase-server.js
```

### 日志监控
- 设备注册日志
- 状态更新日志
- 指令执行日志
- 错误异常日志

### 数据备份
- Supabase自动备份
- 定期数据导出
- 灾难恢复计划

## 🔄 实时功能

### 自动刷新
- 设备列表每30秒刷新
- 设备状态每5秒更新
- 指令状态实时同步

### 实时订阅
- Supabase Realtime订阅
- 数据变更实时推送
- 前端状态自动更新

## 📋 使用指南

### 1. 访问系统
打开浏览器访问: http://www.nagaflow.top

### 2. 查看云设备
- 点击"☁️ 云设备管理"查看注册设备
- 点击"📈 云设备状态"查看实时状态

### 3. 设备操作
- 选择设备查看详细状态
- 发送指令进行远程控制
- 查看历史数据和趋势

### 4. ESP32开发
- 参考示例代码进行开发
- 配置WiFi和服务器地址
- 实现设备特定的指令处理

## 🎯 项目特色

1. **完整的云端解决方案** - 从设备到前端的完整链路
2. **实时数据处理** - 毫秒级的状态更新和指令下发
3. **可扩展架构** - 支持大量设备并发接入
4. **用户友好界面** - 直观的设备管理和监控界面
5. **开发者友好** - 完整的API文档和示例代码

## 🚀 未来扩展

1. **用户认证系统** - 多用户权限管理
2. **数据分析** - 设备数据统计和分析
3. **告警系统** - 设备异常自动告警
4. **移动端应用** - iOS/Android原生应用
5. **边缘计算** - 本地数据处理和缓存

---

## 📞 技术支持

如有问题或建议，请通过以下方式联系：
- 项目地址: https://github.com/dachuanwud/esp32controlboard
- 在线演示: http://www.nagaflow.top

**🎉 恭喜！ESP32 Supabase集成方案已成功部署并运行！**
