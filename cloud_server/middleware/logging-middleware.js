// 日志中间件
const logger = require('../utils/logger');

/**
 * HTTP请求日志中间件
 */
function requestLogger(req, res, next) {
  const startTime = Date.now();
  const { method, originalUrl, ip } = req;
  const userAgent = req.get('User-Agent') || 'Unknown';
  
  // 记录请求开始
  logger.info(`📥 ${method} ${originalUrl}`, {
    ip,
    userAgent: userAgent.substring(0, 100) // 限制长度
  });

  // 监听响应完成
  res.on('finish', () => {
    const duration = Date.now() - startTime;
    const { statusCode } = res;
    
    // 根据状态码选择日志级别
    const logLevel = statusCode >= 400 ? 'warn' : 'info';
    const statusEmoji = getStatusEmoji(statusCode);
    
    logger[logLevel](`📤 ${statusEmoji} ${method} ${originalUrl} ${statusCode} ${duration}ms`, {
      ip,
      statusCode,
      duration,
      userAgent: userAgent.substring(0, 100)
    });
  });

  next();
}

/**
 * 根据状态码获取表情符号
 */
function getStatusEmoji(statusCode) {
  if (statusCode >= 200 && statusCode < 300) return '✅';
  if (statusCode >= 300 && statusCode < 400) return '🔄';
  if (statusCode >= 400 && statusCode < 500) return '⚠️';
  if (statusCode >= 500) return '❌';
  return '📄';
}

/**
 * 敏感信息过滤中间件
 */
function sanitizeRequestBody(req, res, next) {
  if (req.body) {
    // 创建请求体的副本用于日志
    const sanitizedBody = { ...req.body };
    
    // 移除敏感字段
    const sensitiveFields = ['password', 'token', 'secret', 'key', 'auth'];
    sensitiveFields.forEach(field => {
      if (sanitizedBody[field]) {
        sanitizedBody[field] = '[REDACTED]';
      }
    });
    
    // 将清理后的数据附加到请求对象
    req.sanitizedBody = sanitizedBody;
  }
  
  next();
}

/**
 * API调用统计中间件
 */
function apiStatsMiddleware() {
  const stats = {
    totalRequests: 0,
    requestsByMethod: {},
    requestsByPath: {},
    responseTimeSum: 0,
    errorCount: 0
  };

  return (req, res, next) => {
    const startTime = Date.now();
    
    // 增加总请求数
    stats.totalRequests++;
    
    // 按方法统计
    stats.requestsByMethod[req.method] = (stats.requestsByMethod[req.method] || 0) + 1;
    
    // 按路径统计 (简化路径，移除参数)
    const simplePath = req.route ? req.route.path : req.path;
    stats.requestsByPath[simplePath] = (stats.requestsByPath[simplePath] || 0) + 1;

    // 监听响应完成
    res.on('finish', () => {
      const duration = Date.now() - startTime;
      stats.responseTimeSum += duration;
      
      if (res.statusCode >= 400) {
        stats.errorCount++;
      }
    });

    // 将统计信息附加到请求对象
    req.apiStats = stats;
    
    next();
  };
}

/**
 * 获取API统计信息
 */
function getApiStats(req) {
  if (!req.apiStats) {
    return null;
  }
  
  const stats = req.apiStats;
  const avgResponseTime = stats.totalRequests > 0 ? 
    Math.round(stats.responseTimeSum / stats.totalRequests) : 0;
  
  return {
    totalRequests: stats.totalRequests,
    averageResponseTime: avgResponseTime,
    errorRate: stats.totalRequests > 0 ? 
      Math.round((stats.errorCount / stats.totalRequests) * 100) : 0,
    requestsByMethod: stats.requestsByMethod,
    topPaths: Object.entries(stats.requestsByPath)
      .sort(([,a], [,b]) => b - a)
      .slice(0, 10)
      .reduce((obj, [path, count]) => {
        obj[path] = count;
        return obj;
      }, {})
  };
}

/**
 * 慢请求检测中间件
 */
function slowRequestDetector(threshold = 1000) {
  return (req, res, next) => {
    const startTime = Date.now();
    
    res.on('finish', () => {
      const duration = Date.now() - startTime;
      
      if (duration > threshold) {
        logger.warn(`🐌 慢请求检测: ${req.method} ${req.originalUrl} ${duration}ms`, {
          method: req.method,
          url: req.originalUrl,
          duration,
          threshold,
          ip: req.ip
        });
      }
    });
    
    next();
  };
}

module.exports = {
  requestLogger,
  sanitizeRequestBody,
  apiStatsMiddleware,
  getApiStats,
  slowRequestDetector
};
