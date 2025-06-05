# 🗑️ ESP32设备删除功能使用指南

## 📋 功能概述

ESP32设备管理系统现已支持完整的设备删除功能，包括：

- **前端界面删除** - 通过Web界面安全删除设备
- **API接口删除** - 通过REST API删除设备
- **批量删除** - 一次性删除多个设备
- **设备主动注销** - ESP32设备主动从云端注销
- **级联删除** - 自动删除相关的状态和指令数据

## 🎯 删除方式

### 1. 前端Web界面删除

#### 本地设备删除
1. 打开设备管理界面
2. 切换到"设备管理"标签页
3. 找到要删除的设备
4. 点击"🗑️ 删除"按钮
5. 在确认对话框中确认删除

#### 云设备删除
1. 打开云设备管理界面
2. 找到要删除的设备
3. 点击"🗑️ 删除"按钮
4. 在确认对话框中输入设备名称确认删除

**安全特性：**
- 详细的删除确认对话框
- 需要输入设备名称进行二次确认
- 清楚说明删除的影响和后果
- 操作不可撤销的明确提示

### 2. API接口删除

#### 单个设备删除
```bash
# 删除指定设备
DELETE /devices/{deviceId}

# 示例
curl -X DELETE http://localhost:3000/devices/esp32-001
```

#### 批量设备删除
```bash
# 批量删除多个设备
POST /devices/batch-delete
Content-Type: application/json

{
  "deviceIds": ["esp32-001", "esp32-002", "esp32-003"]
}

# 示例
curl -X POST http://localhost:3000/devices/batch-delete \
  -H "Content-Type: application/json" \
  -d '{"deviceIds": ["esp32-001", "esp32-002"]}'
```

#### 设备主动注销
```bash
# ESP32设备主动注销
POST /unregister-device
Content-Type: application/json

{
  "deviceId": "esp32-001",
  "reason": "device_shutdown"
}
```

### 3. ESP32设备端主动注销

#### Arduino代码示例
```cpp
// 设备注销函数
void unregisterDevice(String reason = "device_shutdown") {
  HTTPClient http;
  http.begin(String(serverURL) + "/unregister-device");
  http.addHeader("Content-Type", "application/json");
  
  DynamicJsonDocument doc(512);
  doc["deviceId"] = deviceId;
  doc["reason"] = reason;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  // 处理响应...
  
  http.end();
}

// 优雅关闭
void gracefulShutdown() {
  Serial.println("设备准备关闭，执行优雅注销...");
  
  // 发送最后一次状态更新
  sendStatusUpdate();
  delay(1000);
  
  // 注销设备
  unregisterDevice("device_restart");
  delay(1000);
  
  Serial.println("优雅注销完成");
}
```

#### ESP-IDF代码示例
```c
// 注销设备
esp_err_t cloud_client_unregister_device(const char* reason);

// 优雅关闭（包含注销）
esp_err_t cloud_client_graceful_shutdown(const char* reason);

// 使用示例
void app_shutdown_handler() {
    ESP_LOGI(TAG, "应用程序关闭，执行优雅注销...");
    cloud_client_graceful_shutdown("system_shutdown");
}
```

## 🛡️ 安全特性

### 数据级联删除
删除设备时会自动删除以下相关数据：
- 设备状态历史记录 (`device_status` 表)
- 设备指令记录 (`device_commands` 表)
- 设备主记录 (`esp32_devices` 表)

### 删除确认机制
- **Web界面**: 需要输入设备名称进行二次确认
- **API接口**: 返回详细的删除结果信息
- **批量删除**: 限制单次最多删除50个设备

### 错误处理
- 删除不存在的设备会返回适当的错误信息
- 批量删除支持部分成功的情况
- 详细的日志记录所有删除操作

## 📊 API响应格式

### 成功删除响应
```json
{
  "status": "success",
  "message": "设备删除成功",
  "data": {
    "deleted_device": {
      "device_id": "esp32-001",
      "device_name": "ESP32控制板-001"
    },
    "deleted_records": [...]
  }
}
```

### 批量删除响应
```json
{
  "status": "success",
  "message": "成功删除 2 个设备，失败 0 个",
  "results": [
    {"deviceId": "esp32-001", "status": "success"},
    {"deviceId": "esp32-002", "status": "success"}
  ],
  "errors": []
}
```

### 错误响应
```json
{
  "error": "设备删除失败",
  "message": "设备不存在: esp32-999"
}
```

## 🧪 测试功能

### 运行删除功能测试
```bash
# 进入云服务器目录
cd cloud_server

# 启动服务器
npm start

# 在另一个终端运行测试
node test-device-deletion.js
```

### 测试内容
- 设备注册和删除流程
- 级联删除验证
- 批量删除功能
- 错误处理测试
- 不存在设备删除测试

## ⚠️ 注意事项

### 重要提醒
1. **删除操作不可撤销** - 一旦删除，设备及其所有数据将永久丢失
2. **影响范围** - 删除会清除设备的所有历史状态和指令数据
3. **网络要求** - ESP32设备注销需要网络连接

### 最佳实践
1. **删除前备份** - 如需保留数据，请在删除前进行备份
2. **确认设备信息** - 删除前仔细确认设备ID和名称
3. **优雅注销** - ESP32设备重启或关闭前应主动注销
4. **监控日志** - 关注服务器日志以确认删除操作成功

### 故障排除
1. **删除失败** - 检查设备ID是否正确，网络连接是否正常
2. **部分删除** - 批量删除时部分失败是正常的，检查错误信息
3. **数据残留** - 如发现数据残留，检查数据库级联删除配置

## 🔗 相关文档

- [设备管理API文档](../protocols/http-api.md)
- [Supabase数据库配置](../development/supabase-setup.md)
- [ESP32云客户端开发指南](../development/esp32-cloud-client.md)
- [系统架构文档](../architecture/system-architecture.md)
