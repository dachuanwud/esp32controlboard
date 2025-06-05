// ESP32代理服务模块
const { createProxyMiddleware } = require('http-proxy-middleware');
const config = require('../config/server-config');
const logger = require('../utils/logger');

class ProxyService {
  constructor() {
    this.currentESP32IP = config.esp32.defaultIP;
    this.registeredDevices = new Map();
    this.deviceStatus = new Map();
    this.deviceCommands = new Map();
  }

  /**
   * 创建ESP32代理中间件
   */
  createESP32Proxy() {
    return createProxyMiddleware({
      target: this.currentESP32IP,
      changeOrigin: true,
      timeout: config.esp32.timeout,
      onProxyReq: (proxyReq, req, res) => {
        logger.debug(`[PROXY] ${req.method} ${req.url} -> ${this.currentESP32IP}${req.url}`);
        
        // 优化请求头
        proxyReq.setHeader('User-Agent', 'ESP32-WebClient/2.0');
        proxyReq.setHeader('Accept', 'application/json');
        proxyReq.setHeader('Connection', 'close');
        
        // 添加超时处理
        proxyReq.setTimeout(config.esp32.timeout, () => {
          logger.error(`[PROXY] 请求超时: ${req.url}`);
          proxyReq.destroy();
        });
      },
      onProxyRes: (proxyRes, req, res) => {
        logger.debug(`[PROXY] 响应: ${proxyRes.statusCode} ${req.url}`);
      },
      onError: (err, req, res) => {
        logger.error(`[PROXY ERROR] ${req.url}: ${err.message}`);
        
        if (!res.headersSent) {
          res.status(500).json({
            error: 'ESP32连接失败',
            message: '无法连接到ESP32设备',
            details: err.message
          });
        }
      }
    });
  }

  /**
   * 注册设备 (本地内存存储)
   */
  registerDevice(deviceData) {
    const { deviceId, deviceName, localIP, deviceType, firmwareVersion, hardwareVersion, macAddress } = deviceData;
    
    // 验证必要参数
    if (!deviceId || !localIP) {
      throw new Error('设备ID和本地IP不能为空');
    }

    // 验证IP格式
    const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
    if (!ipRegex.test(localIP)) {
      throw new Error('IP地址格式无效');
    }

    const device = {
      deviceId,
      deviceName: deviceName || `${deviceType || 'ESP32'}-${deviceId}`,
      localIP,
      deviceType: deviceType || 'ESP32',
      firmwareVersion,
      hardwareVersion,
      macAddress,
      registeredAt: new Date().toISOString(),
      lastSeen: new Date().toISOString(),
      status: 'online'
    };

    this.registeredDevices.set(deviceId, device);
    logger.info(`📱 设备已注册 (本地): ${deviceName} (${localIP})`);

    return device;
  }

  /**
   * 更新设备状态 (本地内存存储)
   */
  updateDeviceStatus(deviceId, statusData) {
    if (!deviceId) {
      throw new Error('设备ID不能为空');
    }

    // 更新设备状态
    this.deviceStatus.set(deviceId, 'online');
    this.deviceStatus.set(deviceId + '_lastSeen', Date.now());
    this.deviceStatus.set(deviceId + '_data', statusData || {});

    // 更新注册设备的最后见到时间
    if (this.registeredDevices.has(deviceId)) {
      const device = this.registeredDevices.get(deviceId);
      device.lastSeen = new Date().toISOString();
      device.status = 'online';
      this.registeredDevices.set(deviceId, device);
    }

    logger.debug(`[STATUS] 设备 ${deviceId} 状态更新 (本地)`);

    // 检查是否有待发送的指令
    const commands = this.deviceCommands.get(deviceId) || [];
    const response = {
      status: 'success',
      message: '状态更新成功',
      commands: commands
    };

    // 清空已发送的指令
    if (commands.length > 0) {
      this.deviceCommands.set(deviceId, []);
      logger.info(`[COMMANDS] 发送 ${commands.length} 个指令到设备 ${deviceId} (本地)`);
    }

    return response;
  }

  /**
   * 发送指令到设备 (本地内存存储)
   */
  sendCommand(deviceId, command, data = {}) {
    if (!deviceId || !command) {
      throw new Error('设备ID和指令不能为空');
    }

    // 添加指令到队列
    const commands = this.deviceCommands.get(deviceId) || [];
    const newCommand = {
      id: Date.now().toString(),
      command: command,
      data: data,
      timestamp: Date.now(),
      status: 'pending'
    };

    commands.push(newCommand);
    this.deviceCommands.set(deviceId, commands);

    logger.info(`[COMMAND] 添加指令到设备 ${deviceId}: ${command} (本地)`);

    return {
      status: 'success',
      message: '指令已添加到队列',
      commandId: newCommand.id
    };
  }

  /**
   * 切换ESP32设备
   */
  switchDevice(deviceId) {
    if (!deviceId || !this.registeredDevices.has(deviceId)) {
      throw new Error('指定的设备ID不存在');
    }

    const device = this.registeredDevices.get(deviceId);
    this.currentESP32IP = `http://${device.localIP}`;

    logger.info(`🔄 已切换到设备: ${device.deviceName} (${device.localIP})`);

    return {
      status: 'success',
      message: '设备切换成功',
      currentDevice: device
    };
  }

  /**
   * 获取注册设备列表 (本地内存)
   */
  getRegisteredDevices() {
    const devices = Array.from(this.registeredDevices.values());
    return {
      status: 'success',
      devices,
      count: devices.length
    };
  }

  /**
   * 获取当前ESP32 IP
   */
  getCurrentESP32IP() {
    return this.currentESP32IP;
  }

  /**
   * 设置ESP32 IP
   */
  setESP32IP(ip) {
    this.currentESP32IP = ip;
    logger.info(`🎯 ESP32 IP已更新: ${ip}`);
  }
}

module.exports = new ProxyService();
