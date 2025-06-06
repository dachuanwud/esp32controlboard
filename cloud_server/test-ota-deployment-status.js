#!/usr/bin/env node

/**
 * OTA部署状态测试脚本
 * 测试部署历史组件的状态、进度、耗时功能
 */

const axios = require('axios');

const BASE_URL = 'http://localhost:3000';

async function testDeploymentStatus() {
  console.log('🧪 开始测试OTA部署状态功能...\n');

  try {
    // 1. 测试获取部署历史
    console.log('1️⃣ 测试获取部署历史...');
    const historyResponse = await axios.get(`${BASE_URL}/api/firmware/deployments`);
    console.log(`✅ 部署历史API响应状态: ${historyResponse.status}`);
    console.log(`📊 部署记录数量: ${historyResponse.data.count}`);
    
    if (historyResponse.data.deployments.length > 0) {
      const deployment = historyResponse.data.deployments[0];
      console.log(`📋 最新部署: ${deployment.deployment_name}`);
      console.log(`📈 状态: ${deployment.status}`);
      console.log(`📊 进度: ${deployment.completion_percentage}%`);
      console.log(`⏱️ 耗时: ${deployment.duration_seconds ? deployment.duration_seconds + '秒' : '未完成'}`);
    }
    console.log('');

    // 2. 测试获取实时部署状态
    console.log('2️⃣ 测试获取实时部署状态...');
    const realtimeResponse = await axios.get(`${BASE_URL}/api/firmware/deployments/realtime`);
    console.log(`✅ 实时状态API响应状态: ${realtimeResponse.status}`);
    console.log(`📊 实时部署记录数量: ${realtimeResponse.data.count}`);
    
    if (realtimeResponse.data.deployments.length > 0) {
      const deployment = realtimeResponse.data.deployments[0];
      console.log(`📋 最新部署: ${deployment.deployment_name}`);
      console.log(`📈 状态: ${deployment.status}`);
      console.log(`📊 进度: ${deployment.completion_percentage}%`);
      console.log(`⏱️ 实时耗时: ${deployment.duration_seconds ? deployment.duration_seconds + '秒' : '未开始'}`);
      
      // 检查进行中的部署
      if (deployment.status === 'in_progress') {
        console.log('🔄 发现进行中的部署，测试实时更新...');
        
        // 等待3秒后再次获取，验证实时更新
        await new Promise(resolve => setTimeout(resolve, 3000));
        
        const updatedResponse = await axios.get(`${BASE_URL}/api/firmware/deployments/realtime`);
        const updatedDeployment = updatedResponse.data.deployments.find(d => d.id === deployment.id);
        
        if (updatedDeployment) {
          console.log(`⏱️ 更新后耗时: ${updatedDeployment.duration_seconds}秒`);
          console.log(`📈 耗时差异: ${updatedDeployment.duration_seconds - deployment.duration_seconds}秒`);
          
          if (updatedDeployment.duration_seconds > deployment.duration_seconds) {
            console.log('✅ 实时耗时更新正常');
          } else {
            console.log('⚠️ 实时耗时更新可能有问题');
          }
        }
      }
    }
    console.log('');

    // 3. 测试数据完整性
    console.log('3️⃣ 测试数据完整性...');
    const deployments = realtimeResponse.data.deployments;
    
    for (const deployment of deployments) {
      console.log(`\n📋 检查部署: ${deployment.deployment_name}`);
      
      // 检查必要字段
      const requiredFields = ['id', 'deployment_name', 'firmware_version', 'status', 'total_devices'];
      const missingFields = requiredFields.filter(field => deployment[field] === undefined || deployment[field] === null);
      
      if (missingFields.length === 0) {
        console.log('✅ 必要字段完整');
      } else {
        console.log(`❌ 缺少字段: ${missingFields.join(', ')}`);
      }
      
      // 检查进度计算
      if (deployment.total_devices > 0) {
        const expectedProgress = Math.round((deployment.completed_devices / deployment.total_devices) * 100);
        if (Math.abs(deployment.completion_percentage - expectedProgress) <= 1) {
          console.log('✅ 进度计算正确');
        } else {
          console.log(`❌ 进度计算错误: 期望${expectedProgress}%, 实际${deployment.completion_percentage}%`);
        }
      }
      
      // 检查状态逻辑
      if (deployment.status === 'completed' && deployment.completion_percentage !== 100) {
        console.log('⚠️ 状态与进度不一致：已完成但进度不是100%');
      } else if (deployment.status === 'failed' && deployment.failed_devices === 0) {
        console.log('⚠️ 状态与失败设备数不一致');
      } else {
        console.log('✅ 状态逻辑正确');
      }
      
      // 检查时间逻辑
      if (deployment.started_at && deployment.completed_at) {
        const startTime = new Date(deployment.started_at);
        const endTime = new Date(deployment.completed_at);
        const calculatedDuration = Math.floor((endTime - startTime) / 1000);
        
        if (Math.abs(deployment.duration_seconds - calculatedDuration) <= 1) {
          console.log('✅ 耗时计算正确');
        } else {
          console.log(`❌ 耗时计算错误: 期望${calculatedDuration}秒, 实际${deployment.duration_seconds}秒`);
        }
      } else if (deployment.status === 'in_progress' && deployment.started_at && deployment.duration_seconds > 0) {
        console.log('✅ 进行中部署的实时耗时计算正常');
      }
    }

    console.log('\n🎉 OTA部署状态功能测试完成！');
    
  } catch (error) {
    console.error('❌ 测试失败:', error.message);
    if (error.response) {
      console.error('响应状态:', error.response.status);
      console.error('响应数据:', error.response.data);
    }
  }
}

// 运行测试
testDeploymentStatus();
