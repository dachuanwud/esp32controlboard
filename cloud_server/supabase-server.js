const express = require('express');
const cors = require('cors');
const path = require('path');
const { deviceService } = require('./supabase-config');

const app = express();
const PORT = 3001;

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
    timestamp: new Date().toISOString(),
    database: 'supabase'
  });
});

// 设备注册接口
app.post('/register-device', async (req, res) => {
  try {
    const { deviceId, deviceName, localIP, deviceType = 'ESP32', firmwareVersion, hardwareVersion, macAddress } = req.body;

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

    // 注册设备到Supabase
    const result = await deviceService.registerDevice({
      device_id: deviceId,
      device_name: deviceName || `${deviceType}-${deviceId}`,
      local_ip: localIP,
      device_type: deviceType,
      firmware_version: firmwareVersion,
      hardware_version: hardwareVersion,
      mac_address: macAddress
    });

    console.log(`📱 设备已注册: ${deviceName || deviceId} (${localIP})`);

    res.json({
      status: 'success',
      message: '设备注册成功',
      device: result
    });
  } catch (error) {
    console.error('设备注册失败:', error);
    res.status(500).json({
      error: '设备注册失败',
      message: error.message
    });
  }
});

// 获取已注册设备列表
app.get('/devices', async (req, res) => {
  try {
    const devices = await deviceService.getRegisteredDevices();
    
    res.json({
      status: 'success',
      devices,
      count: devices.length
    });
  } catch (error) {
    console.error('获取设备列表失败:', error);
    res.status(500).json({
      error: '获取设备列表失败',
      message: error.message
    });
  }
});

// 获取在线设备列表
app.get('/devices/online', async (req, res) => {
  try {
    const devices = await deviceService.getOnlineDevices();
    
    res.json({
      status: 'success',
      devices,
      count: devices.length
    });
  } catch (error) {
    console.error('获取在线设备失败:', error);
    res.status(500).json({
      error: '获取在线设备失败',
      message: error.message
    });
  }
});

// ESP32设备状态更新接口
app.post('/device-status', async (req, res) => {
  try {
    const { deviceId, ...statusData } = req.body;

    if (!deviceId) {
      return res.status(400).json({
        status: 'error',
        message: '设备ID不能为空'
      });
    }

    // 更新设备状态到Supabase
    const result = await deviceService.updateDeviceStatus(deviceId, statusData);

    console.log(`[STATUS] 设备 ${deviceId} 状态更新成功`);

    res.json(result);
  } catch (error) {
    console.error('更新设备状态失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 向ESP32设备发送指令接口
app.post('/send-command', async (req, res) => {
  try {
    const { deviceId, command, data = {} } = req.body;

    if (!deviceId || !command) {
      return res.status(400).json({
        status: 'error',
        message: '设备ID和指令不能为空'
      });
    }

    // 发送指令到Supabase
    const result = await deviceService.sendCommand(deviceId, command, data);

    console.log(`[COMMAND] 指令已添加到队列: ${deviceId} - ${command}`);

    res.json(result);
  } catch (error) {
    console.error('发送指令失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 获取设备状态
app.get('/api/device-status/:deviceId', async (req, res) => {
  try {
    const deviceId = req.params.deviceId;
    const status = await deviceService.getDeviceStatus(deviceId);

    if (!status) {
      return res.status(404).json({
        status: 'error',
        message: '设备状态未找到'
      });
    }

    res.json({
      status: 'success',
      data: status
    });
  } catch (error) {
    console.error('获取设备状态失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 获取设备状态历史
app.get('/api/device-status/:deviceId/history', async (req, res) => {
  try {
    const deviceId = req.params.deviceId;
    const limit = req.query.limit || 100;

    const history = await deviceService.getDeviceStatusHistory(deviceId, parseInt(limit));

    res.json({
      status: 'success',
      data: history,
      count: history.length
    });
  } catch (error) {
    console.error('获取设备状态历史失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 获取设备待处理指令
app.get('/api/device-commands/:deviceId/pending', async (req, res) => {
  try {
    const deviceId = req.params.deviceId;
    const commands = await deviceService.getPendingCommands(deviceId);

    res.json({
      status: 'success',
      commands,
      count: commands.length
    });
  } catch (error) {
    console.error('获取待处理指令失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 标记指令完成
app.post('/api/device-commands/:commandId/complete', async (req, res) => {
  try {
    const commandId = req.params.commandId;
    const success = req.body.success !== undefined ? req.body.success : true;
    const errorMessage = req.body.errorMessage || null;

    const result = await deviceService.markCommandCompleted(commandId, success, errorMessage);

    res.json({
      status: 'success',
      message: '指令状态已更新',
      data: result
    });
  } catch (error) {
    console.error('标记指令完成失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 删除设备
app.delete('/api/devices/:deviceId', async (req, res) => {
  try {
    const deviceId = req.params.deviceId;

    await deviceService.deleteDevice(deviceId);

    res.json({
      status: 'success',
      message: '设备删除成功'
    });
  } catch (error) {
    console.error('删除设备失败:', error);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

// 静态文件服务 - React构建文件
const staticPath = path.join(__dirname, '../web_client/dist');
app.use(express.static(staticPath));

// SPA路由处理
app.get('*', (req, res) => {
  res.sendFile(path.join(staticPath, 'index.html'));
});

// 启动服务器
app.listen(PORT, '0.0.0.0', () => {
  console.log('🚀 ESP32 Supabase服务器已启动');
  console.log(`📍 本地访问: http://localhost:${PORT}`);
  console.log(`🌐 外部访问: http://43.167.176.52`);
  console.log(`🔗 新域名访问: http://www.nagaflow.top`);
  console.log(`🔗 旧域名访问: http://www.naga.top:${PORT}`);
  console.log(`💾 数据库: Supabase`);
  console.log(`📁 静态文件: ${staticPath}`);
  console.log(`⏰ 启动时间: ${new Date().toISOString()}`);
});

// 优雅关闭
process.on('SIGINT', () => {
  console.log('\n🛑 正在关闭服务器...');
  process.exit(0);
});

process.on('SIGTERM', () => {
  console.log('\n🛑 正在关闭服务器...');
  process.exit(0);
});
