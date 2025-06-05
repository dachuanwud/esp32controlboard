// 数据库设置脚本
const { createClient } = require('@supabase/supabase-js');
const fs = require('fs').promises;
const path = require('path');

// Supabase配置
const SUPABASE_URL = process.env.SUPABASE_URL || 'https://hfmifzmuwcmtgyjfhxvx.supabase.co';
const SUPABASE_SERVICE_KEY = process.env.SUPABASE_SERVICE_KEY || 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhmbWlmem11d2NtdGd5amZoeHZ4Iiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTczMzQ3MTk3NCwiZXhwIjoyMDQ5MDQ3OTc0fQ.Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8Ej8';

const supabase = createClient(SUPABASE_URL, SUPABASE_SERVICE_KEY);

async function setupDatabase() {
  console.log('🚀 开始设置数据库...');
  
  try {
    // 读取SQL文件
    const sqlPath = path.join(__dirname, 'database', 'firmware-tables.sql');
    const sqlContent = await fs.readFile(sqlPath, 'utf8');
    
    // 分割SQL语句（简单的分割，基于分号）
    const statements = sqlContent
      .split(';')
      .map(stmt => stmt.trim())
      .filter(stmt => stmt.length > 0 && !stmt.startsWith('--'));
    
    console.log(`📝 找到 ${statements.length} 个SQL语句`);
    
    // 执行每个SQL语句
    for (let i = 0; i < statements.length; i++) {
      const statement = statements[i];
      if (statement.trim()) {
        try {
          console.log(`⚡ 执行语句 ${i + 1}/${statements.length}...`);
          const { error } = await supabase.rpc('exec_sql', { sql: statement });
          
          if (error) {
            console.warn(`⚠️ 语句 ${i + 1} 执行警告:`, error.message);
          } else {
            console.log(`✅ 语句 ${i + 1} 执行成功`);
          }
        } catch (err) {
          console.error(`❌ 语句 ${i + 1} 执行失败:`, err.message);
        }
      }
    }
    
    // 验证表是否创建成功
    console.log('\n🔍 验证表结构...');
    
    const tables = ['firmware', 'firmware_deployments', 'device_deployment_details', 'firmware_version_history'];
    
    for (const table of tables) {
      try {
        const { data, error } = await supabase
          .from(table)
          .select('*')
          .limit(1);
        
        if (error) {
          console.error(`❌ 表 ${table} 验证失败:`, error.message);
        } else {
          console.log(`✅ 表 ${table} 验证成功`);
        }
      } catch (err) {
        console.error(`❌ 表 ${table} 验证异常:`, err.message);
      }
    }
    
    console.log('\n🎉 数据库设置完成！');
    console.log('\n📋 创建的表：');
    console.log('  - firmware: 固件信息表');
    console.log('  - firmware_deployments: 固件部署记录表');
    console.log('  - device_deployment_details: 设备部署详情表');
    console.log('  - firmware_version_history: 固件版本历史表');
    console.log('\n📋 创建的视图：');
    console.log('  - firmware_deployment_overview: 固件部署概览视图');
    console.log('\n📋 创建的函数：');
    console.log('  - get_device_current_firmware: 获取设备当前固件版本');
    console.log('  - cleanup_old_firmware: 清理过期固件文件');
    
  } catch (error) {
    console.error('❌ 数据库设置失败:', error.message);
    process.exit(1);
  }
}

// 手动创建表的备用方案
async function createTablesManually() {
  console.log('🔧 使用备用方案手动创建表...');
  
  try {
    // 1. 创建固件信息表
    console.log('📦 创建固件信息表...');
    const { error: firmwareError } = await supabase
      .from('firmware')
      .select('id')
      .limit(1);
    
    if (firmwareError && firmwareError.code === 'PGRST116') {
      console.log('⚠️ 固件表不存在，需要手动在Supabase控制台创建');
      console.log('请在Supabase SQL编辑器中执行以下SQL:');
      console.log(`
CREATE TABLE firmware (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    filename VARCHAR(255) NOT NULL,
    original_name VARCHAR(255) NOT NULL,
    version VARCHAR(50) NOT NULL,
    description TEXT DEFAULT '',
    device_type VARCHAR(50) DEFAULT 'ESP32',
    file_size BIGINT NOT NULL,
    file_hash VARCHAR(64) NOT NULL,
    file_path TEXT NOT NULL,
    upload_time TIMESTAMPTZ DEFAULT NOW(),
    status VARCHAR(20) DEFAULT 'available',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
      `);
    }
    
    // 2. 创建固件部署记录表
    console.log('🚀 创建固件部署记录表...');
    const { error: deploymentError } = await supabase
      .from('firmware_deployments')
      .select('id')
      .limit(1);
    
    if (deploymentError && deploymentError.code === 'PGRST116') {
      console.log('⚠️ 部署表不存在，需要手动在Supabase控制台创建');
      console.log('请在Supabase SQL编辑器中执行以下SQL:');
      console.log(`
CREATE TABLE firmware_deployments (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    deployment_name VARCHAR(255) NOT NULL,
    firmware_id UUID REFERENCES firmware(id) ON DELETE CASCADE,
    target_devices TEXT[] NOT NULL,
    status VARCHAR(20) DEFAULT 'pending',
    total_devices INTEGER NOT NULL DEFAULT 0,
    completed_devices INTEGER DEFAULT 0,
    failed_devices INTEGER DEFAULT 0,
    error_message TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
      `);
    }
    
    console.log('\n📝 请按照上述提示在Supabase控制台手动创建表结构');
    console.log('🔗 Supabase控制台: https://supabase.com/dashboard/project/hfmifzmuwcmtgyjfhxvx/sql');
    
  } catch (error) {
    console.error('❌ 备用方案也失败了:', error.message);
  }
}

// 检查数据库连接
async function checkConnection() {
  console.log('🔗 检查数据库连接...');
  
  try {
    const { data, error } = await supabase
      .from('esp32_devices')
      .select('count')
      .limit(1);
    
    if (error) {
      console.error('❌ 数据库连接失败:', error.message);
      return false;
    }
    
    console.log('✅ 数据库连接成功');
    return true;
  } catch (err) {
    console.error('❌ 数据库连接异常:', err.message);
    return false;
  }
}

// 主函数
async function main() {
  console.log('🎯 ESP32控制板 - 数据库设置工具');
  console.log('=====================================\n');
  
  // 检查连接
  const connected = await checkConnection();
  if (!connected) {
    console.log('❌ 请检查Supabase配置和网络连接');
    process.exit(1);
  }
  
  // 尝试自动设置
  try {
    await setupDatabase();
  } catch (error) {
    console.log('\n⚠️ 自动设置失败，尝试备用方案...');
    await createTablesManually();
  }
}

// 如果直接运行此脚本
if (require.main === module) {
  main().catch(console.error);
}

module.exports = {
  setupDatabase,
  createTablesManually,
  checkConnection
};
