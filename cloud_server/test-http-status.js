// 使用HTTP直接测试设备状态更新
const axios = require('axios');

async function testHttpStatus() {
  try {
    console.log('🧪 开始测试HTTP设备状态更新...');
    
    const deviceId = 'esp32-78421c92e49c';
    const statusData = {
      deviceId: deviceId,
      status: 'online',
      heap_free: 165000,
      uptime: 200
    };
    
    console.log('1. 发送POST请求到/device-status...');
    const response = await axios.post('http://localhost:3000/device-status', statusData, {
      headers: {
        'Content-Type': 'application/json'
      },
      timeout: 5000
    });
    
    console.log('✅ HTTP请求成功:', response.data);
    console.log('🎉 测试通过！');
  } catch (error) {
    console.error('❌ 测试失败:', error.message);
    if (error.response) {
      console.error('响应状态:', error.response.status);
      console.error('响应数据:', error.response.data);
    }
  }
}

testHttpStatus();
