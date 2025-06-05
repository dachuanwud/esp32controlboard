// 设备删除功能测试脚本
const axios = require('axios');

// 测试配置
const SERVER_URL = 'http://localhost:3000';
const TEST_DEVICE_ID = 'test-device-delete-001';
const TEST_DEVICE_NAME = 'Test Device for Deletion';

// 测试用例
async function testDeviceDeletion() {
  console.log('🧪 开始设备删除功能测试...\n');

  try {
    // 1. 注册测试设备
    console.log('1️⃣ 注册测试设备...');
    const registerResponse = await axios.post(`${SERVER_URL}/register-device`, {
      deviceId: TEST_DEVICE_ID,
      deviceName: TEST_DEVICE_NAME,
      localIP: '192.168.1.100',
      deviceType: 'ESP32',
      firmwareVersion: '2.1.0',
      hardwareVersion: 'v2.1',
      macAddress: '00:11:22:33:44:55'
    });
    
    if (registerResponse.data.status === 'success') {
      console.log('✅ 测试设备注册成功');
    } else {
      throw new Error('设备注册失败');
    }

    // 2. 验证设备存在
    console.log('\n2️⃣ 验证设备存在...');
    const devicesResponse = await axios.get(`${SERVER_URL}/devices`);
    const devices = devicesResponse.data.devices || [];
    const testDevice = devices.find(d => d.device_id === TEST_DEVICE_ID);
    
    if (testDevice) {
      console.log('✅ 测试设备存在于设备列表中');
      console.log(`   设备名称: ${testDevice.device_name}`);
      console.log(`   设备状态: ${testDevice.status}`);
    } else {
      throw new Error('测试设备未找到');
    }

    // 3. 发送一些测试状态数据
    console.log('\n3️⃣ 发送测试状态数据...');
    await axios.post(`${SERVER_URL}/device-status`, {
      deviceId: TEST_DEVICE_ID,
      sbus_connected: true,
      can_connected: false,
      wifi_connected: true,
      wifi_ip: '192.168.1.100',
      wifi_rssi: -45,
      free_heap: 200000,
      total_heap: 320000,
      uptime_seconds: 3600
    });
    console.log('✅ 测试状态数据发送成功');

    // 4. 发送一些测试指令
    console.log('\n4️⃣ 发送测试指令...');
    await axios.post(`${SERVER_URL}/send-command`, {
      deviceId: TEST_DEVICE_ID,
      command: 'test_command',
      data: { test: true }
    });
    console.log('✅ 测试指令发送成功');

    // 5. 等待一下，让数据写入完成
    console.log('\n⏳ 等待数据写入完成...');
    await new Promise(resolve => setTimeout(resolve, 2000));

    // 6. 删除设备
    console.log('\n6️⃣ 删除测试设备...');
    const deleteResponse = await axios.delete(`${SERVER_URL}/devices/${TEST_DEVICE_ID}`);
    
    if (deleteResponse.data.status === 'success') {
      console.log('✅ 测试设备删除成功');
      console.log(`   删除结果: ${deleteResponse.data.message}`);
    } else {
      throw new Error('设备删除失败');
    }

    // 7. 验证设备已被删除
    console.log('\n7️⃣ 验证设备已被删除...');
    const devicesAfterDelete = await axios.get(`${SERVER_URL}/devices`);
    const devicesAfter = devicesAfterDelete.data.devices || [];
    const deletedDevice = devicesAfter.find(d => d.device_id === TEST_DEVICE_ID);
    
    if (!deletedDevice) {
      console.log('✅ 测试设备已从设备列表中删除');
    } else {
      throw new Error('设备删除后仍存在于列表中');
    }

    // 8. 尝试删除不存在的设备
    console.log('\n8️⃣ 测试删除不存在的设备...');
    try {
      await axios.delete(`${SERVER_URL}/devices/non-existent-device`);
      throw new Error('删除不存在的设备应该失败');
    } catch (error) {
      if (error.response && error.response.status >= 400) {
        console.log('✅ 删除不存在的设备正确返回错误');
      } else {
        throw error;
      }
    }

    console.log('\n🎉 所有设备删除功能测试通过！');

  } catch (error) {
    console.error('\n❌ 测试失败:', error.message);
    if (error.response) {
      console.error('   响应状态:', error.response.status);
      console.error('   响应数据:', error.response.data);
    }
    process.exit(1);
  }
}

// 批量删除测试
async function testBatchDeletion() {
  console.log('\n🧪 开始批量删除功能测试...\n');

  const testDeviceIds = [
    'batch-test-001',
    'batch-test-002',
    'batch-test-003'
  ];

  try {
    // 1. 注册多个测试设备
    console.log('1️⃣ 注册多个测试设备...');
    for (let i = 0; i < testDeviceIds.length; i++) {
      const deviceId = testDeviceIds[i];
      await axios.post(`${SERVER_URL}/register-device`, {
        deviceId: deviceId,
        deviceName: `Batch Test Device ${i + 1}`,
        localIP: `192.168.1.${100 + i}`,
        deviceType: 'ESP32',
        firmwareVersion: '2.1.0',
        hardwareVersion: 'v2.1',
        macAddress: `00:11:22:33:44:${(55 + i).toString(16).padStart(2, '0')}`
      });
      console.log(`✅ 设备 ${deviceId} 注册成功`);
    }

    // 2. 批量删除设备
    console.log('\n2️⃣ 批量删除设备...');
    const batchDeleteResponse = await axios.post(`${SERVER_URL}/devices/batch-delete`, {
      deviceIds: testDeviceIds
    });
    
    if (batchDeleteResponse.data.status === 'success') {
      console.log('✅ 批量删除成功');
      console.log(`   删除结果: ${batchDeleteResponse.data.message}`);
    } else {
      throw new Error('批量删除失败');
    }

    // 3. 验证设备已被删除
    console.log('\n3️⃣ 验证设备已被删除...');
    const devicesResponse = await axios.get(`${SERVER_URL}/devices`);
    const devices = devicesResponse.data.devices || [];
    
    for (const deviceId of testDeviceIds) {
      const device = devices.find(d => d.device_id === deviceId);
      if (!device) {
        console.log(`✅ 设备 ${deviceId} 已被删除`);
      } else {
        throw new Error(`设备 ${deviceId} 删除后仍存在`);
      }
    }

    console.log('\n🎉 批量删除功能测试通过！');

  } catch (error) {
    console.error('\n❌ 批量删除测试失败:', error.message);
    if (error.response) {
      console.error('   响应状态:', error.response.status);
      console.error('   响应数据:', error.response.data);
    }
    process.exit(1);
  }
}

// 运行测试
async function runTests() {
  console.log('🚀 ESP32设备删除功能测试套件\n');
  console.log('测试服务器:', SERVER_URL);
  console.log('=' * 50);

  await testDeviceDeletion();
  await testBatchDeletion();

  console.log('\n🎊 所有测试完成！');
}

// 启动测试
if (require.main === module) {
  runTests().catch(error => {
    console.error('测试运行失败:', error);
    process.exit(1);
  });
}

module.exports = {
  testDeviceDeletion,
  testBatchDeletion,
  runTests
};
