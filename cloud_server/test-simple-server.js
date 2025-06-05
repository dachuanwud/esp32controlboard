// 简单测试服务器
const express = require('express');
const cors = require('cors');

console.log('🚀 开始启动简单测试服务器...');

const app = express();
const PORT = 3000;

// 基础中间件
app.use(cors());
app.use(express.json());

// 健康检查
app.get('/health', (req, res) => {
  console.log('📋 健康检查请求');
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    version: '2.0.0-test'
  });
});

// 启动服务器
const server = app.listen(PORT, '0.0.0.0', () => {
  console.log('✅ 简单测试服务器已启动');
  console.log(`📍 本地访问: http://localhost:${PORT}`);
  console.log(`🌐 外部访问: http://43.167.176.52:${PORT}`);
  console.log(`🔗 域名访问: http://www.nagaflow.top:${PORT}`);
});

// 优雅关闭
process.on('SIGINT', () => {
  console.log('\n🛑 收到关闭信号，正在关闭服务器...');
  server.close(() => {
    console.log('✅ 服务器已关闭');
    process.exit(0);
  });
});

console.log('🎯 服务器配置完成，等待启动...');
