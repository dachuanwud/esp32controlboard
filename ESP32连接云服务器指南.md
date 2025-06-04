# 🌐 ESP32连接云服务器指南

## 📋 问题说明

ESP32设备在局域网中，云服务器在公网上，无法直接连接。我们提供了设备注册机制来解决这个问题。

## 🔧 解决方案

### 架构图
```
ESP32设备 (局域网) → 设备注册 → 云服务器 (公网) ← 用户访问
192.168.x.x                    www.nagaflow.top
```

## 🚀 使用方法

### 方法1：使用注册脚本（推荐）

1. **在能访问ESP32的网络中运行脚本**：
   ```bash
   ./register-esp32.sh
   ```

2. **按提示输入信息**：
   - 设备名称：例如 `ESP32-客厅控制板`
   - ESP32 IP：例如 `192.168.1.100`
   - 设备ID：例如 `esp32-living-room`

3. **脚本会自动**：
   - 测试ESP32连接
   - 注册设备到云服务器
   - 显示注册结果

### 方法2：手动注册

使用curl命令注册设备：

```bash
curl -X POST http://www.nagaflow.top/register-device \
  -H "Content-Type: application/json" \
  -d '{
    "deviceId": "esp32-001",
    "deviceName": "我的ESP32设备",
    "localIP": "192.168.1.100",
    "deviceType": "ESP32"
  }'
```

### 方法3：通过Web界面

1. 访问 http://www.nagaflow.top
2. 在设备管理页面点击"添加设备"
3. 填写设备信息并提交

## 📱 管理已注册设备

### 查看设备列表
```bash
curl http://www.nagaflow.top/devices
```

### 切换默认设备
```bash
curl -X POST http://www.nagaflow.top/switch-device \
  -H "Content-Type: application/json" \
  -d '{"deviceId": "esp32-001"}'
```

## 🔍 故障排除

### 1. ESP32无法连接
- 确保ESP32已连接Wi-Fi
- 检查ESP32的IP地址是否正确
- 确保ESP32的HTTP服务器正在运行

### 2. 注册失败
- 检查网络连接
- 确认云服务器地址正确
- 验证JSON格式是否正确

### 3. Web界面无法访问设备
- 确保设备已正确注册
- 检查设备是否在线
- 尝试切换到其他设备

## 📋 API接口

### 注册设备
- **URL**: `POST /register-device`
- **参数**:
  ```json
  {
    "deviceId": "设备唯一ID",
    "deviceName": "设备显示名称",
    "localIP": "设备局域网IP",
    "deviceType": "设备类型(默认ESP32)"
  }
  ```

### 获取设备列表
- **URL**: `GET /devices`
- **返回**: 已注册设备列表

### 切换设备
- **URL**: `POST /switch-device`
- **参数**: `{"deviceId": "目标设备ID"}`

## 🎯 下一步

1. **确保ESP32在线**：ESP32连接Wi-Fi并启动HTTP服务器
2. **注册设备**：使用上述任一方法注册ESP32设备
3. **访问Web界面**：通过 http://www.nagaflow.top 管理设备
4. **享受远程控制**：随时随地控制您的ESP32设备

## 💡 提示

- 支持注册多个ESP32设备
- 可以随时切换控制的设备
- 设备信息会自动保存
- 支持设备在线状态监控

现在您可以轻松地将局域网中的ESP32设备连接到云服务器了！
