#!/bin/bash

echo "🚀 启动ESP32控制板Web服务器..."

# 检查端口是否被占用
if lsof -Pi :3000 -sTCP:LISTEN -t >/dev/null ; then
    echo "⚠️  端口3000已被占用，正在停止现有进程..."
    pkill -f "node server.js" || true
    sleep 2
fi

# 进入服务器目录
cd cloud_server

# 检查依赖
if [ ! -d "node_modules" ]; then
    echo "📦 安装依赖..."
    npm install
fi

# 构建React应用（如果需要）
echo "🔨 检查React应用构建..."
if [ ! -d "../web_client/dist" ] || [ "../web_client/src" -nt "../web_client/dist" ]; then
    echo "📦 构建React应用..."
    cd ../web_client
    npm install
    npm run build
    cd ../cloud_server
fi

# 启动服务器
echo "🌐 启动Web服务器..."
echo "📍 本地访问: http://localhost:3000"
echo "🌍 外部访问: http://43.167.176.52"
echo "🔗 新域名访问: http://www.nagaflow.top"
echo "🔗 旧域名访问: http://www.naga.top:3000"
echo ""
echo "💡 ESP32设备注册："
echo "   ./register-esp32.sh"
echo ""
echo "按 Ctrl+C 停止服务器"
echo "=========================="

node server.js
