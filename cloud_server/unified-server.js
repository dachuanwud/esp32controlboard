// ESP32控制板统一服务器
const express = require('express');
const cors = require('cors');
const path = require('path');

// 配置模块
const config = require('./config/server-config');
const corsConfig = require('./config/cors-config');

// 路由模块
const deviceRoutes = require('./routes/device-routes');
const apiRoutes = require('./routes/api-routes');
const { router: proxyRoutes, setupProxyRoutes } = require('./routes/proxy-routes');

// 中间件模块
const { notFoundHandler, errorHandler, asyncHandler } = require('./middleware/error-middleware');
const { requestLogger, sanitizeRequestBody, apiStatsMiddleware, slowRequestDetector } = require('./middleware/logging-middleware');

// 工具模块
const logger = require('./utils/logger');

// 创建Express应用
const app = express();
const PORT = config.server.port;

// 信任代理 (用于获取真实IP)
app.set('trust proxy', true);

// 基础中间件
app.use(cors(corsConfig));
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// 日志中间件
app.use(requestLogger);
app.use(sanitizeRequestBody);
app.use(slowRequestDetector(2000)); // 2秒慢请求阈值

// API统计中间件
const apiStats = apiStatsMiddleware();
app.use(apiStats);

// 健康检查 (在所有路由之前)
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    version: '2.0.0',
    database: 'supabase',
    uptime: Math.floor(process.uptime())
  });
});

// 根路径API路由 (健康检查、状态等)
app.use('/', apiRoutes);

// /api路径下的API路由 (版本、配置等)
app.use('/api', apiRoutes);

// 设备管理路由
app.use('/', deviceRoutes);

// 代理管理路由
app.use('/proxy', proxyRoutes);

// 设置ESP32代理路由 (必须在API路由之后)
setupProxyRoutes(app);

// 静态文件服务
const staticPath = path.join(__dirname, config.static.path);
app.use(express.static(staticPath, {
  maxAge: config.static.maxAge,
  etag: true,
  lastModified: true
}));

// SPA路由处理 (必须在最后)
app.get('*', (req, res) => {
  // 排除API路径
  if (req.path.startsWith('/api') || req.path.startsWith('/proxy')) {
    return res.status(404).json({
      error: 'API路径未找到',
      message: `路径 ${req.path} 不存在`
    });
  }
  
  res.sendFile(path.join(staticPath, 'index.html'));
});

// 错误处理中间件 (必须在最后)
app.use(notFoundHandler);
app.use(errorHandler);

// 启动服务器
const server = app.listen(PORT, config.server.host, () => {
  logger.info('🚀 ESP32统一服务器已启动');
  logger.info(`📍 本地访问: http://localhost:${PORT}`);
  logger.info(`🌐 外部访问: http://43.167.176.52`);
  logger.info(`🔗 域名访问: http://www.nagaflow.top`);
  logger.info(`💾 数据库: Supabase`);
  logger.info(`📁 静态文件: ${staticPath}`);
  logger.info(`🎯 ESP32代理: 已启用`);
  logger.info(`⚙️ 环境: ${config.server.env}`);
  logger.info(`📊 日志级别: ${config.logging.level}`);
  logger.info(`⏰ 启动时间: ${new Date().toISOString()}`);
  logger.info('✅ 所有功能模块已加载完成');
});

// 优雅关闭处理
function gracefulShutdown(signal) {
  logger.info(`\n🛑 收到 ${signal} 信号，开始优雅关闭...`);
  
  server.close((err) => {
    if (err) {
      logger.error('服务器关闭时发生错误:', err);
      process.exit(1);
    }
    
    logger.info('✅ 服务器已优雅关闭');
    process.exit(0);
  });
  
  // 强制关闭超时
  setTimeout(() => {
    logger.error('⚠️ 强制关闭服务器 (超时)');
    process.exit(1);
  }, 10000);
}

// 监听关闭信号
process.on('SIGINT', () => gracefulShutdown('SIGINT'));
process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));

// 未捕获异常处理
process.on('uncaughtException', (error) => {
  logger.error('未捕获的异常:', error);
  gracefulShutdown('uncaughtException');
});

process.on('unhandledRejection', (reason, promise) => {
  logger.error('未处理的Promise拒绝:', { reason, promise });
  gracefulShutdown('unhandledRejection');
});

module.exports = app;
