#!/bin/bash

# 测试云服务器API功能
echo "🧪 测试ESP32云服务器API功能"
echo "================================"

CLOUD_SERVER="http://www.nagaflow.top"
DEVICE_ID="esp32-test-device"
DEVICE_NAME="测试ESP32设备"
LOCAL_IP="192.168.1.100"

echo ""
echo "🔍 1. 测试健康检查..."
curl -s "$CLOUD_SERVER/health" | jq '.' || echo "健康检查失败"

echo ""
echo "📡 2. 测试设备注册..."
REGISTER_RESPONSE=$(curl -s -X POST "$CLOUD_SERVER/register-device" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\",
        \"deviceName\": \"$DEVICE_NAME\",
        \"localIP\": \"$LOCAL_IP\",
        \"deviceType\": \"ESP32\"
    }")

echo "$REGISTER_RESPONSE" | jq '.' || echo "设备注册失败"

echo ""
echo "📋 3. 测试获取设备列表..."
curl -s "$CLOUD_SERVER/devices" | jq '.' || echo "获取设备列表失败"

echo ""
echo "📊 4. 测试设备状态更新..."
STATUS_RESPONSE=$(curl -s -X POST "$CLOUD_SERVER/device-status" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\",
        \"status\": \"online\",
        \"data\": {
            \"firmware_version\": \"1.0.0\",
            \"free_heap\": 123456,
            \"uptime\": 3600
        }
    }")

echo "$STATUS_RESPONSE" | jq '.' || echo "状态更新失败"

echo ""
echo "📤 5. 测试发送指令..."
COMMAND_RESPONSE=$(curl -s -X POST "$CLOUD_SERVER/send-command" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\",
        \"command\": \"test_command\",
        \"data\": {
            \"param1\": \"value1\",
            \"param2\": 42
        }
    }")

echo "$COMMAND_RESPONSE" | jq '.' || echo "发送指令失败"

echo ""
echo "📥 6. 再次测试状态更新（应该返回指令）..."
STATUS_WITH_COMMANDS=$(curl -s -X POST "$CLOUD_SERVER/device-status" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\",
        \"status\": \"online\",
        \"data\": {
            \"firmware_version\": \"1.0.0\",
            \"free_heap\": 123456,
            \"uptime\": 3610
        }
    }")

echo "$STATUS_WITH_COMMANDS" | jq '.' || echo "状态更新失败"

echo ""
echo "🔄 7. 测试切换设备..."
SWITCH_RESPONSE=$(curl -s -X POST "$CLOUD_SERVER/switch-device" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\"
    }")

echo "$SWITCH_RESPONSE" | jq '.' || echo "切换设备失败"

echo ""
echo "✅ API测试完成！"
echo ""
echo "💡 提示："
echo "   - 如果看到JSON格式的响应，说明API工作正常"
echo "   - 如果看到错误信息，请检查云服务器是否正在运行"
echo "   - 确保域名 www.nagaflow.top 可以正常访问"
echo ""
