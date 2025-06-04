const axios = require('axios');

// 服务器配置
const SERVER_URL = 'http://localhost:3000';

// 模拟设备信息
const devices = [
  {
    deviceId: 'ESP32-001',
    deviceName: '客厅控制板',
    localIP: '192.168.1.101',
    deviceType: 'ESP32',
    firmwareVersion: '2.0.0',
    hardwareVersion: 'v2.1',
    macAddress: '00:11:22:33:44:01'
  },
  {
    deviceId: 'ESP32-002', 
    deviceName: '卧室控制板',
    localIP: '192.168.1.102',
    deviceType: 'ESP32',
    firmwareVersion: '2.0.0',
    hardwareVersion: 'v2.1',
    macAddress: '00:11:22:33:44:02'
  },
  {
    deviceId: 'ESP32-003',
    deviceName: '车库控制板',
    localIP: '192.168.1.103',
    deviceType: 'ESP32',
    firmwareVersion: '2.0.1',
    hardwareVersion: 'v2.2',
    macAddress: '00:11:22:33:44:03'
  }
];

// 注册设备
async function registerDevice(device) {
  try {
    console.log(`📱 注册设备: ${device.deviceName} (${device.deviceId})`);
    
    const response = await axios.post(`${SERVER_URL}/register-device`, {
      deviceId: device.deviceId,
      deviceName: device.deviceName,
      localIP: device.localIP,
      deviceType: device.deviceType,
      firmwareVersion: device.firmwareVersion,
      hardwareVersion: device.hardwareVersion,
      macAddress: device.macAddress
    });
    
    if (response.data.status === 'success') {
      console.log(`✅ ${device.deviceName} 注册成功`);
    } else {
      console.log(`❌ ${device.deviceName} 注册失败:`, response.data.message);
    }
  } catch (error) {
    console.error(`❌ ${device.deviceName} 注册错误:`, error.message);
  }
}

// 发送状态更新
async function sendStatusUpdate(device) {
  try {
    // 生成模拟状态数据
    const statusData = {
      deviceId: device.deviceId,
      sbus_connected: Math.random() > 0.2, // 80%概率连接
      can_connected: Math.random() > 0.5,  // 50%概率连接
      wifi_connected: true,
      wifi_ip: device.localIP,
      wifi_rssi: -30 - Math.floor(Math.random() * 40), // -30 到 -70 dBm
      free_heap: 100000 + Math.floor(Math.random() * 50000),
      total_heap: 327680,
      uptime_seconds: Math.floor(Date.now() / 1000) % 86400, // 模拟运行时间
      task_count: 6 + Math.floor(Math.random() * 4),
      can_tx_count: Math.floor(Math.random() * 1000),
      can_rx_count: Math.floor(Math.random() * 1000),
      sbus_channels: Array.from({length: 8}, () => 1000 + Math.floor(Math.random() * 1000)),
      motor_left_speed: Math.floor(Math.random() * 200) - 100,
      motor_right_speed: Math.floor(Math.random() * 200) - 100,
      last_sbus_time: Date.now(),
      last_cmd_time: Date.now()
    };
    
    const response = await axios.post(`${SERVER_URL}/device-status`, statusData);
    
    if (response.data.status === 'success') {
      console.log(`📊 ${device.deviceName} 状态更新成功`);
      
      // 检查是否有待处理的指令
      if (response.data.commands && response.data.commands.length > 0) {
        console.log(`📤 ${device.deviceName} 收到 ${response.data.commands.length} 个指令`);
        for (const command of response.data.commands) {
          await processCommand(device, command);
        }
      }
    } else {
      console.log(`❌ ${device.deviceName} 状态更新失败:`, response.data.message);
    }
  } catch (error) {
    console.error(`❌ ${device.deviceName} 状态更新错误:`, error.message);
  }
}

// 处理指令
async function processCommand(device, command) {
  console.log(`🎯 ${device.deviceName} 处理指令: ${command.command} (ID: ${command.id})`);
  
  // 模拟指令处理时间
  await new Promise(resolve => setTimeout(resolve, 100 + Math.random() * 500));
  
  // 模拟指令执行结果（90%成功率）
  const success = Math.random() > 0.1;
  
  try {
    // 这里应该调用标记指令完成的API，但由于我们的简化服务器没有实现这个路由参数版本
    // 我们就简单打印结果
    console.log(`${success ? '✅' : '❌'} ${device.deviceName} 指令 ${command.command} ${success ? '执行成功' : '执行失败'}`);
  } catch (error) {
    console.error(`❌ ${device.deviceName} 标记指令完成失败:`, error.message);
  }
}

// 发送测试指令
async function sendTestCommand(deviceId, command, data = {}) {
  try {
    console.log(`📤 向设备 ${deviceId} 发送指令: ${command}`);
    
    const response = await axios.post(`${SERVER_URL}/send-command`, {
      deviceId,
      command,
      data
    });
    
    if (response.data.status === 'success') {
      console.log(`✅ 指令发送成功`);
    } else {
      console.log(`❌ 指令发送失败:`, response.data.message);
    }
  } catch (error) {
    console.error(`❌ 发送指令错误:`, error.message);
  }
}

// 主测试函数
async function runSimulation() {
  console.log('🚀 开始ESP32设备模拟测试...\n');
  
  // 1. 注册所有设备
  console.log('📱 第一步: 注册设备');
  for (const device of devices) {
    await registerDevice(device);
    await new Promise(resolve => setTimeout(resolve, 500)); // 间隔500ms
  }
  
  console.log('\n⏱️ 等待2秒...\n');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  // 2. 发送一些测试指令
  console.log('📤 第二步: 发送测试指令');
  await sendTestCommand('ESP32-001', 'led_control', { pin: 2, state: true });
  await sendTestCommand('ESP32-002', 'motor_speed', { left: 50, right: -30 });
  await sendTestCommand('ESP32-003', 'test_command', { param: 'hello world' });
  
  console.log('\n⏱️ 等待1秒...\n');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // 3. 开始状态更新循环
  console.log('📊 第三步: 开始状态更新循环 (每5秒一次)');
  
  let updateCount = 0;
  const maxUpdates = 10; // 最多更新10次
  
  const updateInterval = setInterval(async () => {
    updateCount++;
    console.log(`\n--- 状态更新 #${updateCount} ---`);
    
    // 并行更新所有设备状态
    const updatePromises = devices.map(device => sendStatusUpdate(device));
    await Promise.all(updatePromises);
    
    // 偶尔发送一些随机指令
    if (updateCount % 3 === 0) {
      const randomDevice = devices[Math.floor(Math.random() * devices.length)];
      const commands = ['led_control', 'motor_speed', 'test_command'];
      const randomCommand = commands[Math.floor(Math.random() * commands.length)];
      
      let commandData = {};
      if (randomCommand === 'led_control') {
        commandData = { pin: 2, state: Math.random() > 0.5 };
      } else if (randomCommand === 'motor_speed') {
        commandData = { 
          left: Math.floor(Math.random() * 200) - 100,
          right: Math.floor(Math.random() * 200) - 100
        };
      } else {
        commandData = { param: `random_${Math.floor(Math.random() * 1000)}` };
      }
      
      await sendTestCommand(randomDevice.deviceId, randomCommand, commandData);
    }
    
    if (updateCount >= maxUpdates) {
      clearInterval(updateInterval);
      console.log('\n🎉 模拟测试完成!');
      console.log('💡 现在可以访问 http://www.nagaflow.top/devices 查看设备列表');
      console.log('💡 访问 http://www.nagaflow.top/cloud-status 查看设备状态');
    }
  }, 5000);
}

// 启动模拟
runSimulation().catch(console.error);
