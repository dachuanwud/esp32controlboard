// 错误处理中间件
const logger = require('../utils/logger');

/**
 * 404错误处理中间件
 */
function notFoundHandler(req, res, next) {
  const error = new Error(`路径未找到: ${req.method} ${req.originalUrl}`);
  error.status = 404;
  next(error);
}

/**
 * 全局错误处理中间件
 */
function errorHandler(error, req, res, next) {
  const status = error.status || 500;
  const message = error.message || '服务器内部错误';
  
  // 记录错误日志
  logger.error(`[${status}] ${message}`, {
    method: req.method,
    url: req.originalUrl,
    ip: req.ip,
    userAgent: req.get('User-Agent'),
    stack: error.stack
  });

  // 开发环境返回详细错误信息
  const isDevelopment = process.env.NODE_ENV !== 'production';
  
  const errorResponse = {
    error: true,
    status: status,
    message: message,
    timestamp: new Date().toISOString()
  };

  // 开发环境添加堆栈信息
  if (isDevelopment) {
    errorResponse.stack = error.stack;
    errorResponse.details = {
      method: req.method,
      url: req.originalUrl,
      headers: req.headers,
      body: req.body
    };
  }

  res.status(status).json(errorResponse);
}

/**
 * 异步错误捕获包装器
 */
function asyncHandler(fn) {
  return (req, res, next) => {
    Promise.resolve(fn(req, res, next)).catch(next);
  };
}

/**
 * 验证错误处理
 */
function validationErrorHandler(errors) {
  const error = new Error('数据验证失败');
  error.status = 400;
  error.errors = errors;
  return error;
}

/**
 * 数据库错误处理
 */
function databaseErrorHandler(error) {
  logger.error('数据库错误:', error);
  
  // Supabase特定错误处理
  if (error.code) {
    switch (error.code) {
      case '23505': // 唯一约束违反
        const dbError = new Error('数据已存在，请检查唯一字段');
        dbError.status = 409;
        return dbError;
      case '23503': // 外键约束违反
        const fkError = new Error('关联数据不存在');
        fkError.status = 400;
        return fkError;
      case '42P01': // 表不存在
        const tableError = new Error('数据表不存在');
        tableError.status = 500;
        return tableError;
      default:
        const genericError = new Error('数据库操作失败');
        genericError.status = 500;
        return genericError;
    }
  }
  
  // 通用数据库错误
  const dbError = new Error('数据库连接失败');
  dbError.status = 500;
  return dbError;
}

/**
 * 网络错误处理
 */
function networkErrorHandler(error) {
  logger.error('网络错误:', error);
  
  if (error.code === 'ECONNREFUSED') {
    const netError = new Error('无法连接到目标服务');
    netError.status = 503;
    return netError;
  }
  
  if (error.code === 'ETIMEDOUT') {
    const timeoutError = new Error('请求超时');
    timeoutError.status = 408;
    return timeoutError;
  }
  
  const netError = new Error('网络连接失败');
  netError.status = 500;
  return netError;
}

module.exports = {
  notFoundHandler,
  errorHandler,
  asyncHandler,
  validationErrorHandler,
  databaseErrorHandler,
  networkErrorHandler
};
