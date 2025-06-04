#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi配置
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// 云服务器配置
const char* serverURL = "http://www.nagaflow.top";  // 或者使用 http://43.167.176.52

// 设备信息
String deviceId = "ESP32-" + String(ESP.getEfuseMac(), HEX);
String deviceName = "ESP32 Control Board";
String deviceType = "ESP32";
String firmwareVersion = "2.0.0";
String hardwareVersion = "v2.1";

// 状态更新间隔
unsigned long lastStatusUpdate = 0;
const unsigned long statusUpdateInterval = 5000; // 5秒

// 模拟传感器数据
struct DeviceStatus {
  bool sbus_connected = true;
  bool can_connected = false;
  bool wifi_connected = true;
  String wifi_ip;
  int wifi_rssi;
  uint32_t free_heap;
  uint32_t total_heap;
  uint32_t uptime_seconds;
  int task_count = 8;
  uint32_t can_tx_count = 0;
  uint32_t can_rx_count = 0;
  int sbus_channels[8] = {1500, 1500, 1000, 1500, 1000, 1000, 1000, 1000};
  int motor_left_speed = 0;
  int motor_right_speed = 0;
  uint32_t last_sbus_time;
  uint32_t last_cmd_time;
};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Supabase集成示例启动...");
  
  // 连接WiFi
  WiFi.begin(ssid, password);
  Serial.print("连接WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi连接成功!");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  
  // 注册设备到云服务器
  registerDevice();
}

void loop() {
  // 检查WiFi连接
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi连接丢失，尝试重连...");
    WiFi.reconnect();
    delay(5000);
    return;
  }
  
  // 定期发送状态更新
  if (millis() - lastStatusUpdate >= statusUpdateInterval) {
    sendStatusUpdate();
    lastStatusUpdate = millis();
  }
  
  delay(100);
}

void registerDevice() {
  HTTPClient http;
  http.begin(String(serverURL) + "/register-device");
  http.addHeader("Content-Type", "application/json");
  
  // 构建注册数据
  DynamicJsonDocument doc(1024);
  doc["deviceId"] = deviceId;
  doc["deviceName"] = deviceName;
  doc["localIP"] = WiFi.localIP().toString();
  doc["deviceType"] = deviceType;
  doc["firmwareVersion"] = firmwareVersion;
  doc["hardwareVersion"] = hardwareVersion;
  doc["macAddress"] = WiFi.macAddress();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("注册设备到云服务器...");
  Serial.println("注册数据: " + jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("注册响应码: " + String(httpResponseCode));
    Serial.println("注册响应: " + response);
    
    // 解析响应
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);
    
    if (responseDoc["status"] == "success") {
      Serial.println("✅ 设备注册成功!");
    } else {
      Serial.println("❌ 设备注册失败: " + responseDoc["message"].as<String>());
    }
  } else {
    Serial.println("❌ 注册请求失败，错误码: " + String(httpResponseCode));
  }
  
  http.end();
}

void sendStatusUpdate() {
  HTTPClient http;
  http.begin(String(serverURL) + "/device-status");
  http.addHeader("Content-Type", "application/json");
  
  // 获取当前状态
  DeviceStatus status;
  status.wifi_connected = (WiFi.status() == WL_CONNECTED);
  status.wifi_ip = WiFi.localIP().toString();
  status.wifi_rssi = WiFi.RSSI();
  status.free_heap = ESP.getFreeHeap();
  status.total_heap = ESP.getHeapSize();
  status.uptime_seconds = millis() / 1000;
  status.last_sbus_time = millis();
  status.last_cmd_time = millis();
  
  // 模拟SBUS通道数据变化
  for (int i = 0; i < 8; i++) {
    status.sbus_channels[i] = 1000 + random(0, 1000);
  }
  
  // 构建状态数据
  DynamicJsonDocument doc(2048);
  doc["deviceId"] = deviceId;
  doc["sbus_connected"] = status.sbus_connected;
  doc["can_connected"] = status.can_connected;
  doc["wifi_connected"] = status.wifi_connected;
  doc["wifi_ip"] = status.wifi_ip;
  doc["wifi_rssi"] = status.wifi_rssi;
  doc["free_heap"] = status.free_heap;
  doc["total_heap"] = status.total_heap;
  doc["uptime_seconds"] = status.uptime_seconds;
  doc["task_count"] = status.task_count;
  doc["can_tx_count"] = status.can_tx_count;
  doc["can_rx_count"] = status.can_rx_count;
  doc["motor_left_speed"] = status.motor_left_speed;
  doc["motor_right_speed"] = status.motor_right_speed;
  doc["last_sbus_time"] = status.last_sbus_time;
  doc["last_cmd_time"] = status.last_cmd_time;
  
  // SBUS通道数组
  JsonArray channels = doc.createNestedArray("sbus_channels");
  for (int i = 0; i < 8; i++) {
    channels.add(status.sbus_channels[i]);
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("发送状态更新...");
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("状态更新响应码: " + String(httpResponseCode));
    
    // 解析响应，检查是否有待处理的指令
    DynamicJsonDocument responseDoc(2048);
    deserializeJson(responseDoc, response);
    
    if (responseDoc["status"] == "success") {
      Serial.println("✅ 状态更新成功");
      
      // 检查是否有待处理的指令
      if (responseDoc.containsKey("commands") && responseDoc["commands"].is<JsonArray>()) {
        JsonArray commands = responseDoc["commands"];
        for (JsonVariant cmd : commands) {
          processCommand(cmd);
        }
      }
    } else {
      Serial.println("❌ 状态更新失败: " + responseDoc["message"].as<String>());
    }
  } else {
    Serial.println("❌ 状态更新请求失败，错误码: " + String(httpResponseCode));
  }
  
  http.end();
}

void processCommand(JsonVariant command) {
  String cmdId = command["id"];
  String cmdName = command["command"];
  JsonObject cmdData = command["data"];
  
  Serial.println("📤 收到指令: " + cmdName + " (ID: " + cmdId + ")");
  
  // 处理不同类型的指令
  if (cmdName == "led_control") {
    int pin = cmdData["pin"] | 2;
    bool state = cmdData["state"] | false;
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);
    Serial.println("LED控制: Pin " + String(pin) + " = " + (state ? "ON" : "OFF"));
    
  } else if (cmdName == "motor_speed") {
    int leftSpeed = cmdData["left"] | 0;
    int rightSpeed = cmdData["right"] | 0;
    
    // 这里应该是实际的电机控制代码
    Serial.println("电机速度: 左=" + String(leftSpeed) + ", 右=" + String(rightSpeed));
    
  } else if (cmdName == "restart") {
    Serial.println("收到重启指令，3秒后重启...");
    delay(3000);
    ESP.restart();
    
  } else if (cmdName == "test_command") {
    Serial.println("收到测试指令: " + cmdData["param"].as<String>());
    
  } else {
    Serial.println("未知指令: " + cmdName);
  }
  
  // 标记指令完成（这里简化处理，实际应该根据执行结果设置success）
  markCommandCompleted(cmdId, true);
}

void markCommandCompleted(String commandId, bool success) {
  HTTPClient http;
  http.begin(String(serverURL) + "/api/device-commands/" + commandId + "/complete");
  http.addHeader("Content-Type", "application/json");
  
  DynamicJsonDocument doc(256);
  doc["success"] = success;
  if (!success) {
    doc["errorMessage"] = "指令执行失败";
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    Serial.println("✅ 指令完成状态已更新: " + commandId);
  } else {
    Serial.println("❌ 更新指令状态失败: " + String(httpResponseCode));
  }
  
  http.end();
}
