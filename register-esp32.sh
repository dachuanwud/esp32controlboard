#!/bin/bash

# ESP32设备注册脚本
# 用于将ESP32设备注册到云服务器

echo "🚀 ESP32设备注册工具"
echo "===================="

# 配置参数
CLOUD_SERVER="http://www.nagaflow.top"
DEVICE_TYPE="ESP32"

# 获取用户输入
read -p "请输入ESP32设备名称 (例如: ESP32-客厅): " DEVICE_NAME
read -p "请输入ESP32的IP地址 (例如: 192.168.1.100): " ESP32_IP
read -p "请输入设备ID (例如: esp32-001): " DEVICE_ID

# 验证输入
if [ -z "$DEVICE_NAME" ] || [ -z "$ESP32_IP" ] || [ -z "$DEVICE_ID" ]; then
    echo "❌ 错误: 所有字段都是必填的"
    exit 1
fi

# 验证IP格式
if ! [[ $ESP32_IP =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
    echo "❌ 错误: IP地址格式不正确"
    exit 1
fi

echo ""
echo "📋 注册信息:"
echo "设备名称: $DEVICE_NAME"
echo "设备IP: $ESP32_IP"
echo "设备ID: $DEVICE_ID"
echo "云服务器: $CLOUD_SERVER"
echo ""

# 测试ESP32连接
echo "🔍 测试ESP32设备连接..."
if curl -s --connect-timeout 5 "http://$ESP32_IP/api/device/info" > /dev/null; then
    echo "✅ ESP32设备连接正常"
else
    echo "⚠️  警告: 无法连接到ESP32设备，但仍会尝试注册"
fi

# 注册设备到云服务器
echo "📡 正在注册设备到云服务器..."

RESPONSE=$(curl -s -X POST "$CLOUD_SERVER/register-device" \
    -H "Content-Type: application/json" \
    -d "{
        \"deviceId\": \"$DEVICE_ID\",
        \"deviceName\": \"$DEVICE_NAME\",
        \"localIP\": \"$ESP32_IP\",
        \"deviceType\": \"$DEVICE_TYPE\"
    }")

# 检查注册结果
if echo "$RESPONSE" | grep -q "success"; then
    echo "✅ 设备注册成功!"
    echo "🌐 现在可以通过 $CLOUD_SERVER 访问您的ESP32设备"
    echo ""
    echo "📱 设备信息:"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
else
    echo "❌ 设备注册失败:"
    echo "$RESPONSE"
    exit 1
fi

echo ""
echo "🎉 注册完成! 您现在可以通过Web界面管理ESP32设备了"
