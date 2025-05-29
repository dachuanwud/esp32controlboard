# 📡 ESP32控制板Web API接口规范

## 🎯 接口概述

ESP32控制板Web API提供了完整的RESTful接口，支持设备信息查询、实时状态监控、OTA固件更新和Wi-Fi网络配置等功能。所有接口均返回JSON格式数据，支持CORS跨域访问。

## 🔧 基础配置

### 服务器信息
- **基础URL**: `http://[ESP32_IP]/api`
- **端口**: 80 (HTTP)
- **协议**: HTTP/1.1
- **数据格式**: JSON
- **字符编码**: UTF-8

### 通用响应格式

所有API接口均遵循统一的响应格式：

```json
{
  "status": "success|error",
  "data": {},
  "message": "描述信息"
}
```

### 状态码说明

| HTTP状态码 | 含义 | 说明 |
|-----------|------|------|
| 200 | OK | 请求成功 |
| 400 | Bad Request | 请求参数错误 |
| 404 | Not Found | 接口不存在 |
| 500 | Internal Server Error | 服务器内部错误 |

## 📱 设备信息接口

### GET /api/device/info

获取ESP32设备的基本信息，包括硬件配置、固件版本、系统资源等。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "data": {
    "device_name": "ESP32 Control Board",
    "firmware_version": "1.0.0-OTA",
    "hardware_version": "v1.0",
    "chip_model": "ESP32-1",
    "flash_size": 16777216,
    "free_heap": 180000,
    "uptime_seconds": 3600,
    "mac_address": "AA:BB:CC:DD:EE:FF"
  }
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| device_name | string | 设备名称 |
| firmware_version | string | 固件版本号 |
| hardware_version | string | 硬件版本号 |
| chip_model | string | 芯片型号 |
| flash_size | number | Flash存储大小(字节) |
| free_heap | number | 可用堆内存(字节) |
| uptime_seconds | number | 系统运行时间(秒) |
| mac_address | string | MAC地址 |

### GET /api/device/status

获取设备实时运行状态，包括连接状态、SBUS数据、电机状态等。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "data": {
    "sbus_connected": true,
    "can_connected": true,
    "wifi_connected": true,
    "wifi_ip": "192.168.1.100",
    "wifi_rssi": -45,
    "sbus_channels": [1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500],
    "motor_left_speed": 0,
    "motor_right_speed": 0,
    "last_sbus_time": 1640995200000,
    "last_cmd_time": 1640995200000
  }
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| sbus_connected | boolean | SBUS连接状态 |
| can_connected | boolean | CAN总线连接状态 |
| wifi_connected | boolean | Wi-Fi连接状态 |
| wifi_ip | string | Wi-Fi IP地址 |
| wifi_rssi | number | Wi-Fi信号强度(dBm) |
| sbus_channels | number[] | SBUS通道值数组(16个通道) |
| motor_left_speed | number | 左电机速度(-100~100) |
| motor_right_speed | number | 右电机速度(-100~100) |
| last_sbus_time | number | 最后SBUS更新时间戳 |
| last_cmd_time | number | 最后命令更新时间戳 |

## 🔄 OTA更新接口

### POST /api/ota/upload

上传固件文件并开始OTA更新过程。

#### 请求参数
- **Content-Type**: `application/octet-stream`
- **Body**: 二进制固件数据(.bin文件)

#### 请求示例
```bash
curl -X POST \
  http://192.168.1.100/api/ota/upload \
  -H 'Content-Type: application/octet-stream' \
  --data-binary '@firmware.bin'
```

#### 响应示例
```json
{
  "status": "success",
  "message": "OTA update completed successfully"
}
```

#### 错误响应示例
```json
{
  "status": "error",
  "message": "Invalid firmware size"
}
```

### GET /api/ota/progress

获取当前OTA更新进度。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "data": {
    "in_progress": true,
    "total_size": 1048576,
    "written_size": 524288,
    "progress_percent": 50,
    "status_message": "Writing firmware: 50%",
    "success": false,
    "error_message": ""
  }
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| in_progress | boolean | 是否正在更新 |
| total_size | number | 固件总大小(字节) |
| written_size | number | 已写入大小(字节) |
| progress_percent | number | 进度百分比(0-100) |
| status_message | string | 状态描述信息 |
| success | boolean | 更新是否成功 |
| error_message | string | 错误信息(如有) |

### POST /api/ota/rollback

回滚到上一个固件版本。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "message": "Firmware rollback initiated"
}
```

## 📶 Wi-Fi配置接口

### GET /api/wifi/status

获取Wi-Fi连接状态信息。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "data": {
    "state": "connected",
    "ip_address": "192.168.1.100",
    "rssi": -45,
    "retry_count": 0,
    "connect_time": 1640995200000
  }
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| state | string | 连接状态: disconnected/connecting/connected/failed |
| ip_address | string | 分配的IP地址 |
| rssi | number | 信号强度(dBm) |
| retry_count | number | 重试次数 |
| connect_time | number | 连接时间戳 |

### POST /api/wifi/connect

连接到指定的Wi-Fi网络。

#### 请求参数
```json
{
  "ssid": "网络名称",
  "password": "网络密码"
}
```

#### 请求示例
```bash
curl -X POST \
  http://192.168.1.100/api/wifi/connect \
  -H 'Content-Type: application/json' \
  -d '{
    "ssid": "MyWiFi",
    "password": "password123"
  }'
```

#### 响应示例
```json
{
  "status": "success",
  "message": "Wi-Fi connection initiated"
}
```

### GET /api/wifi/scan

扫描可用的Wi-Fi网络。

#### 请求参数
无

#### 响应示例
```json
{
  "status": "success",
  "data": [
    {
      "ssid": "MyWiFi",
      "rssi": -45,
      "authmode": "WIFI_AUTH_WPA2_PSK",
      "channel": 6
    },
    {
      "ssid": "GuestNetwork",
      "rssi": -60,
      "authmode": "WIFI_AUTH_OPEN",
      "channel": 11
    }
  ]
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| ssid | string | 网络名称 |
| rssi | number | 信号强度(dBm) |
| authmode | string | 认证模式 |
| channel | number | 信道号 |

## 🔒 错误处理

### 错误响应格式

```json
{
  "status": "error",
  "message": "错误描述信息"
}
```

### 常见错误码

| 错误信息 | 原因 | 解决方案 |
|----------|------|----------|
| "Invalid firmware size" | 固件文件过大或为空 | 检查固件文件大小(≤1MB) |
| "OTA already in progress" | OTA更新正在进行中 | 等待当前更新完成 |
| "Failed to connect to Wi-Fi" | Wi-Fi连接失败 | 检查SSID和密码 |
| "No content provided" | 请求体为空 | 确保发送正确的数据 |

## 🚀 使用示例

### JavaScript/TypeScript示例

```typescript
// 获取设备信息
const getDeviceInfo = async () => {
  try {
    const response = await fetch('/api/device/info');
    const data = await response.json();
    if (data.status === 'success') {
      console.log('设备信息:', data.data);
    }
  } catch (error) {
    console.error('获取设备信息失败:', error);
  }
};

// 上传固件
const uploadFirmware = async (file: File) => {
  try {
    const response = await fetch('/api/ota/upload', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream',
      },
      body: file
    });
    const data = await response.json();
    if (data.status === 'success') {
      console.log('固件上传成功');
    }
  } catch (error) {
    console.error('固件上传失败:', error);
  }
};

// 连接Wi-Fi
const connectWiFi = async (ssid: string, password: string) => {
  try {
    const response = await fetch('/api/wifi/connect', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ ssid, password })
    });
    const data = await response.json();
    if (data.status === 'success') {
      console.log('Wi-Fi连接成功');
    }
  } catch (error) {
    console.error('Wi-Fi连接失败:', error);
  }
};
```

### Python示例

```python
import requests
import json

# ESP32设备IP地址
ESP32_IP = "192.168.1.100"
BASE_URL = f"http://{ESP32_IP}/api"

# 获取设备状态
def get_device_status():
    try:
        response = requests.get(f"{BASE_URL}/device/status")
        data = response.json()
        if data['status'] == 'success':
            print("设备状态:", data['data'])
    except Exception as e:
        print("获取设备状态失败:", e)

# 上传固件
def upload_firmware(firmware_path):
    try:
        with open(firmware_path, 'rb') as f:
            response = requests.post(
                f"{BASE_URL}/ota/upload",
                headers={'Content-Type': 'application/octet-stream'},
                data=f
            )
        data = response.json()
        if data['status'] == 'success':
            print("固件上传成功")
    except Exception as e:
        print("固件上传失败:", e)

# 连接Wi-Fi
def connect_wifi(ssid, password):
    try:
        response = requests.post(
            f"{BASE_URL}/wifi/connect",
            headers={'Content-Type': 'application/json'},
            json={'ssid': ssid, 'password': password}
        )
        data = response.json()
        if data['status'] == 'success':
            print("Wi-Fi连接成功")
    except Exception as e:
        print("Wi-Fi连接失败:", e)
```

## 📝 注意事项

1. **网络要求**: 确保客户端与ESP32设备在同一网络中
2. **固件格式**: OTA更新仅支持.bin格式的固件文件
3. **文件大小**: 固件文件大小限制为1MB
4. **并发限制**: 同时只能进行一个OTA更新操作
5. **超时设置**: 建议设置合适的请求超时时间(10-30秒)
6. **错误重试**: 网络不稳定时建议实现重试机制

---

*本API规范基于ESP32控制板Web OTA系统v1.0.0，如有更新请参考最新版本文档。*
