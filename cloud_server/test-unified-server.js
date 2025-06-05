// 统一服务器测试脚本
const axios = require('axios');

const SERVER_URL = 'http://localhost:3000';
const TEST_DEVICE_ID = 'test-esp32-' + Date.now();

class UnifiedServerTester {
  constructor() {
    this.testResults = [];
    this.passedTests = 0;
    this.failedTests = 0;
  }

  /**
   * 运行测试
   */
  async runTests() {
    console.log('🧪 开始统一服务器测试...\n');

    // 基础功能测试
    await this.testHealthCheck();
    await this.testStatus();
    await this.testVersion();

    // 设备管理测试
    await this.testDeviceRegistration();
    await this.testDeviceStatus();
    await this.testSendCommand();
    await this.testGetDevices();

    // 代理功能测试
    await this.testProxyTarget();
    await this.testProxyStats();

    // 错误处理测试
    await this.testErrorHandling();

    this.printResults();
  }

  /**
   * 执行单个测试
   */
  async runTest(testName, testFunction) {
    try {
      console.log(`🔍 测试: ${testName}`);
      await testFunction();
      this.passedTests++;
      this.testResults.push({ name: testName, status: 'PASS', error: null });
      console.log(`✅ ${testName} - 通过\n`);
    } catch (error) {
      this.failedTests++;
      this.testResults.push({ name: testName, status: 'FAIL', error: error.message });
      console.log(`❌ ${testName} - 失败: ${error.message}\n`);
    }
  }

  /**
   * 健康检查测试
   */
  async testHealthCheck() {
    await this.runTest('健康检查', async () => {
      const response = await axios.get(`${SERVER_URL}/health`);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (response.data.status !== 'healthy') {
        throw new Error(`期望状态healthy，实际${response.data.status}`);
      }
      
      if (!response.data.version) {
        throw new Error('缺少版本信息');
      }
    });
  }

  /**
   * 状态接口测试
   */
  async testStatus() {
    await this.runTest('系统状态', async () => {
      const response = await axios.get(`${SERVER_URL}/status`);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (!response.data.uptime && response.data.uptime !== 0) {
        throw new Error('缺少运行时间信息');
      }
      
      if (!response.data.memory) {
        throw new Error('缺少内存使用信息');
      }
    });
  }

  /**
   * 版本信息测试
   */
  async testVersion() {
    await this.runTest('版本信息', async () => {
      const response = await axios.get(`${SERVER_URL}/api/version`);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (response.data.version !== '2.0.0') {
        throw new Error(`期望版本2.0.0，实际${response.data.version}`);
      }
      
      if (!Array.isArray(response.data.features)) {
        throw new Error('缺少功能特性列表');
      }
    });
  }

  /**
   * 设备注册测试
   */
  async testDeviceRegistration() {
    await this.runTest('设备注册 (Supabase)', async () => {
      const deviceData = {
        deviceId: TEST_DEVICE_ID,
        deviceName: 'Test ESP32 Device',
        localIP: '192.168.1.100',
        deviceType: 'ESP32',
        firmwareVersion: '2.0.0',
        hardwareVersion: 'v2.1',
        macAddress: '00:11:22:33:44:55'
      };

      const response = await axios.post(`${SERVER_URL}/register-device`, deviceData);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (response.data.status !== 'success') {
        throw new Error(`期望状态success，实际${response.data.status}`);
      }
    });
  }

  /**
   * 设备状态更新测试
   */
  async testDeviceStatus() {
    await this.runTest('设备状态更新', async () => {
      const statusData = {
        deviceId: TEST_DEVICE_ID,
        wifi_connected: true,
        wifi_ip: '192.168.1.100',
        wifi_rssi: -45,
        free_heap: 200000,
        total_heap: 320000,
        uptime_seconds: 3600,
        task_count: 8,
        sbus_connected: true,
        can_connected: false,
        sbus_channels: [1500, 1500, 1000, 1500, 1000, 1000, 1000, 1000]
      };

      const response = await axios.post(`${SERVER_URL}/device-status`, statusData);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
    });
  }

  /**
   * 发送指令测试
   */
  async testSendCommand() {
    await this.runTest('发送指令', async () => {
      const commandData = {
        deviceId: TEST_DEVICE_ID,
        command: 'led_control',
        data: {
          pin: 2,
          state: true
        }
      };

      const response = await axios.post(`${SERVER_URL}/send-command`, commandData);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (response.data.status !== 'success') {
        throw new Error(`期望状态success，实际${response.data.status}`);
      }
    });
  }

  /**
   * 获取设备列表测试
   */
  async testGetDevices() {
    await this.runTest('获取设备列表', async () => {
      const response = await axios.get(`${SERVER_URL}/devices`);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (response.data.status !== 'success') {
        throw new Error(`期望状态success，实际${response.data.status}`);
      }
      
      if (!Array.isArray(response.data.devices)) {
        throw new Error('设备列表应该是数组');
      }
    });
  }

  /**
   * 代理目标测试
   */
  async testProxyTarget() {
    await this.runTest('代理目标管理', async () => {
      // 获取当前代理目标
      const getResponse = await axios.get(`${SERVER_URL}/proxy/target`);
      
      if (getResponse.status !== 200) {
        throw new Error(`获取代理目标失败，状态码${getResponse.status}`);
      }
      
      // 设置新的代理目标
      const setResponse = await axios.post(`${SERVER_URL}/proxy/target`, {
        ip: 'http://192.168.1.200'
      });
      
      if (setResponse.status !== 200) {
        throw new Error(`设置代理目标失败，状态码${setResponse.status}`);
      }
      
      if (setResponse.data.target !== 'http://192.168.1.200') {
        throw new Error('代理目标设置不正确');
      }
    });
  }

  /**
   * 代理统计测试
   */
  async testProxyStats() {
    await this.runTest('代理统计信息', async () => {
      const response = await axios.get(`${SERVER_URL}/proxy/stats`);
      
      if (response.status !== 200) {
        throw new Error(`期望状态码200，实际${response.status}`);
      }
      
      if (!response.data.stats) {
        throw new Error('缺少统计信息');
      }
      
      if (typeof response.data.stats.uptime !== 'number') {
        throw new Error('运行时间应该是数字');
      }
    });
  }

  /**
   * 错误处理测试
   */
  async testErrorHandling() {
    await this.runTest('错误处理', async () => {
      try {
        // 测试无效的设备注册
        await axios.post(`${SERVER_URL}/register-device`, {
          deviceId: '', // 空设备ID
          localIP: 'invalid-ip' // 无效IP
        });
        throw new Error('应该返回400错误');
      } catch (error) {
        if (error.response && error.response.status === 400) {
          // 期望的错误
          return;
        }
        throw error;
      }
    });
  }

  /**
   * 打印测试结果
   */
  printResults() {
    console.log('\n📊 测试结果汇总:');
    console.log('='.repeat(50));
    console.log(`✅ 通过: ${this.passedTests}`);
    console.log(`❌ 失败: ${this.failedTests}`);
    console.log(`📈 成功率: ${Math.round((this.passedTests / (this.passedTests + this.failedTests)) * 100)}%`);
    
    if (this.failedTests > 0) {
      console.log('\n❌ 失败的测试:');
      this.testResults
        .filter(result => result.status === 'FAIL')
        .forEach(result => {
          console.log(`  - ${result.name}: ${result.error}`);
        });
    }
    
    console.log('\n🎉 测试完成!');
  }
}

// 运行测试
if (require.main === module) {
  const tester = new UnifiedServerTester();
  tester.runTests().catch(console.error);
}

module.exports = UnifiedServerTester;
