// 测试设备状态更新的脚本
const { supabaseAdmin } = require('./supabase-config');

async function testDeviceStatus() {
  try {
    console.log('🧪 开始测试设备状态更新...');

    const deviceId = 'esp32-78421c92e49c';
    const statusData = {
      free_heap: 165000,
      uptime_seconds: 200,
      wifi_connected: true,
      sbus_connected: false,
      can_connected: false
    };

    console.log('1. 直接调用存储过程...');
    const { data, error } = await supabaseAdmin.rpc('update_device_status', {
      p_device_id: deviceId,
      p_status_data: statusData
    });

    if (error) {
      console.error('❌ 存储过程调用失败:', error);
      return;
    }

    console.log('✅ 存储过程调用成功:', data);
    console.log('🎉 测试通过！');
  } catch (error) {
    console.error('❌ 测试失败:', error);
  }
}

testDeviceStatus();
