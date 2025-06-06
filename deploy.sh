#!/bin/bash

echo "🚀 开始部署 ESP32 控制板系统..."

# 构建前端
echo "📦 构建前端代码..."
cd web_client
npm run build
if [ $? -ne 0 ]; then
    echo "❌ 前端构建失败"
    exit 1
fi

# 重启后端服务
echo "🔄 重启后端服务..."
cd ../
pm2 restart esp32-server
if [ $? -ne 0 ]; then
    echo "❌ 服务重启失败"
    exit 1
fi

# 等待服务启动
echo "⏳ 等待服务启动..."
sleep 3

# 健康检查
echo "🔍 进行健康检查..."
response=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/health)
if [ "$response" = "200" ]; then
    echo "✅ 部署成功！服务正常运行"
    echo "🌐 访问地址: http://www.nagaflow.top/"
else
    echo "❌ 健康检查失败，HTTP状态码: $response"
    pm2 logs esp32-server --lines 10
    exit 1
fi
