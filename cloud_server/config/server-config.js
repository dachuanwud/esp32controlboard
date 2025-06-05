// 服务器配置文件
module.exports = {
  // 服务器基础配置
  server: {
    port: process.env.PORT || 3000,
    host: '0.0.0.0',
    env: process.env.NODE_ENV || 'development'
  },

  // ESP32代理配置
  esp32: {
    defaultIP: process.env.ESP32_DEFAULT_IP || 'http://192.168.6.109',
    timeout: 10000,
    retryAttempts: 3
  },

  // 静态文件配置
  static: {
    path: '../web_client/dist',
    maxAge: process.env.NODE_ENV === 'production' ? '1d' : 0
  },

  // 日志配置
  logging: {
    level: process.env.LOG_LEVEL || 'debug',
    format: 'combined',
    enableConsole: true,
    enableFile: process.env.NODE_ENV === 'production'
  },

  // 安全配置
  security: {
    enableHelmet: process.env.NODE_ENV === 'production',
    rateLimitWindowMs: 15 * 60 * 1000, // 15分钟
    rateLimitMax: 100 // 每个IP最多100个请求
  }
};
