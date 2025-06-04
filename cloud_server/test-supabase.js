const { deviceService } = require('./supabase-config');

async function testSupabase() {
  try {
    console.log('🧪 测试Supabase连接...');
    
    // 测试获取设备列表
    console.log('📱 获取设备列表...');
    const devices = await deviceService.getRegisteredDevices();
    console.log('✅ 设备列表获取成功:', devices.length, '个设备');
    
    // 测试注册设备
    console.log('📝 测试设备注册...');
    const testDevice = {
      device_id: 'test-esp32-001',
      device_name: 'Test ESP32 Device',
      local_ip: '192.168.1.100',
      device_type: 'ESP32',
      firmware_version: '1.0.0',
      hardware_version: 'v1.0',
      mac_address: '00:11:22:33:44:55'
    };
    
    const registerResult = await deviceService.registerDevice(testDevice);
    console.log('✅ 设备注册成功:', registerResult);
    
    // 测试状态更新
    console.log('📊 测试状态更新...');
    const statusData = {
      sbus_connected: true,
      can_connected: false,
      wifi_connected: true,
      wifi_ip: '192.168.1.100',
      wifi_rssi: -45,
      free_heap: 123456,
      total_heap: 327680,
      uptime_seconds: 3600,
      task_count: 8,
      can_tx_count: 0,
      can_rx_count: 0,
      sbus_channels: [1500, 1500, 1000, 1500, 1000, 1000, 1000, 1000],
      motor_left_speed: 0,
      motor_right_speed: 0,
      last_sbus_time: Date.now(),
      last_cmd_time: Date.now()
    };
    
    const statusResult = await deviceService.updateDeviceStatus('test-esp32-001', statusData);
    console.log('✅ 状态更新成功:', statusResult);
    
    // 测试发送指令
    console.log('📤 测试发送指令...');
    const commandResult = await deviceService.sendCommand('test-esp32-001', 'test_command', { param: 'value' });
    console.log('✅ 指令发送成功:', commandResult);
    
    // 再次获取设备列表
    console.log('📱 再次获取设备列表...');
    const updatedDevices = await deviceService.getRegisteredDevices();
    console.log('✅ 更新后的设备列表:', updatedDevices.length, '个设备');
    
    console.log('🎉 所有测试通过！Supabase集成正常工作');
    
  } catch (error) {
    console.error('❌ 测试失败:', error);
    console.error('错误详情:', error.message);
  }
}

testSupabase();
