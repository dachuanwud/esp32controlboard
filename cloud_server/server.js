const express = require('express');
const cors = require('cors');
const { createProxyMiddleware } = require('http-proxy-middleware');
const path = require('path');

const app = express();
const PORT = 3000;
let ESP32_IP = 'http://192.168.6.109'; // 默认IP，可以动态更新
const registeredDevices = new Map(); // 存储注册的设备
const deviceCommands = new Map(); // 存储待发送给设备的指令
const deviceStatus = new Map(); // 存储设备状态

// CORS配置
app.use(cors({
  origin: [
    'http://localhost:3000',
    'http://43.167.176.52:3000',
    'http://www.nagaflow.top',
    'https://www.nagaflow.top',
    'http://nagaflow.top',
    'https://nagaflow.top',
    'http://www.naga.top:3000'
  ],
  credentials: true
}));

// 解析JSON
app.use(express.json());

// 健康检查
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString()
  });
});

// 设备注册接口
app.post('/register-device', (req, res) => {
  const { deviceId, deviceName, localIP, deviceType = 'ESP32' } = req.body;

  if (!deviceId || !localIP) {
    return res.status(400).json({
      error: '缺少必要参数',
      message: '需要提供deviceId和localIP'
    });
  }

  // 验证IP格式
  const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipRegex.test(localIP)) {
    return res.status(400).json({
      error: 'IP格式错误',
      message: '请提供有效的IP地址'
    });
  }

  // 注册设备
  const deviceInfo = {
    deviceId,
    deviceName: deviceName || `${deviceType}-${deviceId}`,
    localIP,
    deviceType,
    registeredAt: new Date().toISOString(),
    lastSeen: new Date().toISOString(),
    status: 'online'
  };

  registeredDevices.set(deviceId, deviceInfo);

  // 如果是第一个设备，设为默认设备
  if (registeredDevices.size === 1) {
    ESP32_IP = `http://${localIP}`;
    console.log(`🎯 默认ESP32设备已更新: ${ESP32_IP}`);
  }

  console.log(`📱 设备已注册: ${deviceName} (${localIP})`);

  res.json({
    status: 'success',
    message: '设备注册成功',
    device: deviceInfo
  });
});

// 获取已注册设备列表
app.get('/devices', (req, res) => {
  const devices = Array.from(registeredDevices.values()).map(device => ({
    ...device,
    status: deviceStatus.get(device.deviceId) || 'offline',
    lastSeen: deviceStatus.get(device.deviceId + '_lastSeen') || null
  }));
  res.json({
    status: 'success',
    devices,
    count: devices.length
  });
});

// 切换默认设备
app.post('/switch-device', (req, res) => {
  const { deviceId } = req.body;

  if (!deviceId || !registeredDevices.has(deviceId)) {
    return res.status(404).json({
      error: '设备未找到',
      message: '指定的设备ID不存在'
    });
  }

  const device = registeredDevices.get(deviceId);
  ESP32_IP = `http://${device.localIP}`;

  console.log(`🔄 已切换到设备: ${device.deviceName} (${device.localIP})`);

  res.json({
    status: 'success',
    message: '设备切换成功',
    currentDevice: device
  });
});

// ESP32设备状态更新接口
app.post('/device-status', (req, res) => {
  const { deviceId, status, data } = req.body;

  if (!deviceId) {
    return res.status(400).json({
      status: 'error',
      message: '设备ID不能为空'
    });
  }

  // 更新设备状态
  deviceStatus.set(deviceId, status || 'online');
  deviceStatus.set(deviceId + '_lastSeen', Date.now());
  deviceStatus.set(deviceId + '_data', data || {});

  console.log(`[STATUS] 设备 ${deviceId} 状态更新: ${status}`);

  // 检查是否有待发送的指令
  const commands = deviceCommands.get(deviceId) || [];
  const response = {
    status: 'success',
    message: '状态更新成功',
    commands: commands
  };

  // 清空已发送的指令
  if (commands.length > 0) {
    deviceCommands.set(deviceId, []);
    console.log(`[COMMANDS] 发送 ${commands.length} 个指令到设备 ${deviceId}`);
  }

  res.json(response);
});

// 向ESP32设备发送指令接口
app.post('/send-command', (req, res) => {
  const { deviceId, command, data } = req.body;

  if (!deviceId || !command) {
    return res.status(400).json({
      status: 'error',
      message: '设备ID和指令不能为空'
    });
  }

  // 添加指令到队列
  const commands = deviceCommands.get(deviceId) || [];
  commands.push({
    id: Date.now(),
    command: command,
    data: data || {},
    timestamp: Date.now()
  });
  deviceCommands.set(deviceId, commands);

  console.log(`[COMMAND] 添加指令到设备 ${deviceId}: ${command}`);

  res.json({
    status: 'success',
    message: '指令已添加到队列'
  });
});

// ESP32 API代理
const esp32Proxy = createProxyMiddleware({
  target: ESP32_IP,
  changeOrigin: true,
  timeout: 10000,
  onProxyReq: (proxyReq, req, res) => {
    console.log(`[PROXY] ${req.method} ${req.url} -> ${ESP32_IP}${req.url}`);
    // 简化请求头
    proxyReq.setHeader('User-Agent', 'ESP32-WebClient/1.0');
    proxyReq.setHeader('Accept', 'application/json');
    proxyReq.setHeader('Connection', 'close');
  },
  onError: (err, req, res) => {
    console.error(`[PROXY ERROR] ${req.url}:`, err.message);
    res.status(500).json({
      error: 'ESP32连接失败',
      message: '无法连接到ESP32设备'
    });
  }
});

// API代理路由
app.use('/api', esp32Proxy);

// 静态文件服务 - React构建文件
const staticPath = path.join(__dirname, '../web_client/dist');
app.use(express.static(staticPath));

// SPA路由处理
app.get('*', (req, res) => {
  res.sendFile(path.join(staticPath, 'index.html'));
});

// 启动服务器
app.listen(PORT, '0.0.0.0', () => {
  console.log('🚀 ESP32 Web服务器已启动');
  console.log(`📍 本地访问: http://localhost:${PORT}`);
  console.log(`🌐 外部访问: http://43.167.176.52`);
  console.log(`🔗 新域名访问: http://www.nagaflow.top`);
  console.log(`🔗 旧域名访问: http://www.naga.top:${PORT}`);
  console.log(`🎯 ESP32目标: ${ESP32_IP}`);
  console.log(`📁 静态文件: ${staticPath}`);
  console.log(`⏰ 启动时间: ${new Date().toISOString()}`);
});
