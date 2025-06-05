// 日志工具模块
const config = require('../config/server-config');

class Logger {
  constructor() {
    this.level = config.logging.level;
    this.enableConsole = config.logging.enableConsole;
    this.enableFile = config.logging.enableFile;
  }

  /**
   * 格式化日志消息
   */
  formatMessage(level, message, meta = {}) {
    const timestamp = new Date().toISOString();
    const metaStr = Object.keys(meta).length > 0 ? ` ${JSON.stringify(meta)}` : '';
    return `[${timestamp}] [${level.toUpperCase()}] ${message}${metaStr}`;
  }

  /**
   * 输出日志
   */
  log(level, message, meta = {}) {
    if (!this.shouldLog(level)) {
      return;
    }

    const formattedMessage = this.formatMessage(level, message, meta);

    if (this.enableConsole) {
      switch (level) {
        case 'error':
          console.error(formattedMessage);
          break;
        case 'warn':
          console.warn(formattedMessage);
          break;
        case 'info':
          console.info(formattedMessage);
          break;
        case 'debug':
          console.debug(formattedMessage);
          break;
        default:
          console.log(formattedMessage);
      }
    }

    // TODO: 如果启用文件日志，写入文件
    if (this.enableFile) {
      // 实现文件日志写入
    }
  }

  /**
   * 判断是否应该记录日志
   */
  shouldLog(level) {
    const levels = ['error', 'warn', 'info', 'debug'];
    const currentLevelIndex = levels.indexOf(this.level);
    const messageLevelIndex = levels.indexOf(level);
    
    return messageLevelIndex <= currentLevelIndex;
  }

  /**
   * 错误日志
   */
  error(message, meta = {}) {
    this.log('error', message, meta);
  }

  /**
   * 警告日志
   */
  warn(message, meta = {}) {
    this.log('warn', message, meta);
  }

  /**
   * 信息日志
   */
  info(message, meta = {}) {
    this.log('info', message, meta);
  }

  /**
   * 调试日志
   */
  debug(message, meta = {}) {
    this.log('debug', message, meta);
  }
}

module.exports = new Logger();
