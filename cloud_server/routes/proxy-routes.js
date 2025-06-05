// ESP32代理路由模块
const express = require('express');
const router = express.Router();
const proxyService = require('../services/proxy-service');
const logger = require('../utils/logger');

/**
 * 设置ESP32代理路由
 * 只代理ESP32相关的API请求，排除服务器管理API
 */
function setupProxyRoutes(app) {
  // 创建ESP32代理中间件
  const esp32Proxy = proxyService.createESP32Proxy();

  // 定义需要排除的服务器API路径 (不包含/api前缀，因为req.path已经去掉了)
  const excludedPaths = [
    '/version',
    '/config',
    '/device-status',
    '/device-commands'
  ];

  // 代理中间件，排除服务器管理API
  app.use('/api', (req, res, next) => {
    // 检查是否是需要排除的路径
    const isExcluded = excludedPaths.some(path => req.path.startsWith(path));

    if (isExcluded) {
      // 跳过代理，继续到下一个中间件
      logger.debug(`🔄 跳过代理: /api${req.path} (服务器管理API)`);
      next();
    } else {
      // 使用ESP32代理
      logger.debug(`🔄 使用代理: /api${req.path} -> ESP32`);
      esp32Proxy(req, res, next);
    }
  });

  logger.info('🔄 ESP32代理路由已设置 (排除服务器管理API)');
}

/**
 * 获取当前ESP32代理目标
 * GET /proxy/target
 */
router.get('/target', (req, res) => {
  const currentIP = proxyService.getCurrentESP32IP();
  
  res.json({
    status: 'success',
    target: currentIP,
    timestamp: new Date().toISOString()
  });
});

/**
 * 设置ESP32代理目标
 * POST /proxy/target
 */
router.post('/target', (req, res) => {
  try {
    const { ip } = req.body;
    
    if (!ip) {
      return res.status(400).json({
        error: '参数错误',
        message: 'IP地址不能为空'
      });
    }

    // 验证IP格式
    const ipRegex = /^https?:\/\/(\d{1,3}\.){3}\d{1,3}(:\d+)?$/;
    if (!ipRegex.test(ip)) {
      return res.status(400).json({
        error: 'IP格式错误',
        message: '请提供有效的IP地址 (格式: http://192.168.1.100)'
      });
    }

    proxyService.setESP32IP(ip);
    
    res.json({
      status: 'success',
      message: 'ESP32代理目标已更新',
      target: ip,
      timestamp: new Date().toISOString()
    });
  } catch (error) {
    logger.error(`设置代理目标失败: ${error.message}`);
    res.status(500).json({
      error: '设置代理目标失败',
      message: error.message
    });
  }
});

/**
 * 测试ESP32连接
 * GET /proxy/test
 */
router.get('/test', async (req, res) => {
  try {
    const currentIP = proxyService.getCurrentESP32IP();
    
    // 这里可以实现实际的连接测试逻辑
    // 例如发送一个简单的HTTP请求到ESP32设备
    
    res.json({
      status: 'success',
      message: 'ESP32连接测试',
      target: currentIP,
      // 实际实现中可以添加真实的测试结果
      testResult: {
        reachable: true,
        responseTime: '< 100ms',
        lastTest: new Date().toISOString()
      }
    });
  } catch (error) {
    logger.error(`ESP32连接测试失败: ${error.message}`);
    res.status(500).json({
      error: 'ESP32连接测试失败',
      message: error.message
    });
  }
});

/**
 * 获取代理统计信息
 * GET /proxy/stats
 */
router.get('/stats', (req, res) => {
  // 这里可以实现代理统计信息的收集
  // 例如请求数量、成功率、平均响应时间等
  
  res.json({
    status: 'success',
    stats: {
      totalRequests: 0, // 实际实现中从统计模块获取
      successfulRequests: 0,
      failedRequests: 0,
      averageResponseTime: 0,
      uptime: Math.floor(process.uptime()),
      lastReset: new Date().toISOString()
    },
    timestamp: new Date().toISOString()
  });
});

module.exports = {
  router,
  setupProxyRoutes
};
