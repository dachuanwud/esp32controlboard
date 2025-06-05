# ESP32控制板 - OTA数据库设置指南

## 📋 概述

本指南将帮助您在Supabase中设置OTA（Over-The-Air）固件升级所需的数据库表结构。

## 🔧 设置步骤

### 1. 访问Supabase控制台

1. 打开浏览器，访问：https://supabase.com/dashboard
2. 登录您的账户
3. 选择项目：`esp32-device-manager`
4. 点击左侧菜单中的 "SQL Editor"

### 2. 执行SQL脚本

在SQL编辑器中，复制并执行以下SQL语句：

```sql
-- 1. 固件信息表
CREATE TABLE IF NOT EXISTS firmware (
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

-- 2. 固件部署记录表
CREATE TABLE IF NOT EXISTS firmware_deployments (
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

-- 3. 设备部署详情表
CREATE TABLE IF NOT EXISTS device_deployment_details (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    deployment_id UUID REFERENCES firmware_deployments(id) ON DELETE CASCADE,
    device_id VARCHAR(50) NOT NULL,
    device_ip VARCHAR(15),
    status VARCHAR(20) DEFAULT 'pending',
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    error_message TEXT,
    progress_percent INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 4. 固件版本历史表
CREATE TABLE IF NOT EXISTS firmware_version_history (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    old_version VARCHAR(50),
    new_version VARCHAR(50) NOT NULL,
    firmware_id UUID REFERENCES firmware(id),
    deployment_id UUID REFERENCES firmware_deployments(id),
    upgrade_time TIMESTAMPTZ DEFAULT NOW(),
    upgrade_status VARCHAR(20) DEFAULT 'success',
    rollback_reason TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

### 3. 创建索引（可选，提高性能）

```sql
-- 创建索引以提高查询性能
CREATE INDEX IF NOT EXISTS idx_firmware_version ON firmware(version);
CREATE INDEX IF NOT EXISTS idx_firmware_device_type ON firmware(device_type);
CREATE INDEX IF NOT EXISTS idx_firmware_upload_time ON firmware(upload_time);
CREATE INDEX IF NOT EXISTS idx_deployments_status ON firmware_deployments(status);
CREATE INDEX IF NOT EXISTS idx_deployments_created_at ON firmware_deployments(created_at);
CREATE INDEX IF NOT EXISTS idx_device_details_deployment ON device_deployment_details(deployment_id);
CREATE INDEX IF NOT EXISTS idx_device_details_device ON device_deployment_details(device_id);
CREATE INDEX IF NOT EXISTS idx_version_history_device ON firmware_version_history(device_id);
CREATE INDEX IF NOT EXISTS idx_version_history_time ON firmware_version_history(upgrade_time);
```

### 4. 创建更新时间触发器

```sql
-- 创建更新时间触发器函数
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- 为相关表添加更新时间触发器
CREATE TRIGGER update_firmware_updated_at BEFORE UPDATE ON firmware
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_deployments_updated_at BEFORE UPDATE ON firmware_deployments
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_device_details_updated_at BEFORE UPDATE ON device_deployment_details
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
```

### 5. 设置行级安全策略（RLS）

```sql
-- 启用行级安全策略
ALTER TABLE firmware ENABLE ROW LEVEL SECURITY;
ALTER TABLE firmware_deployments ENABLE ROW LEVEL SECURITY;
ALTER TABLE device_deployment_details ENABLE ROW LEVEL SECURITY;
ALTER TABLE firmware_version_history ENABLE ROW LEVEL SECURITY;

-- 创建策略：允许所有操作（可根据需要调整）
CREATE POLICY "Allow all operations on firmware" ON firmware
    FOR ALL USING (true);

CREATE POLICY "Allow all operations on firmware_deployments" ON firmware_deployments
    FOR ALL USING (true);

CREATE POLICY "Allow all operations on device_deployment_details" ON device_deployment_details
    FOR ALL USING (true);

CREATE POLICY "Allow all operations on firmware_version_history" ON firmware_version_history
    FOR ALL USING (true);
```

### 6. 创建有用的视图

```sql
-- 创建视图：固件部署概览
CREATE OR REPLACE VIEW firmware_deployment_overview AS
SELECT 
    fd.id,
    fd.deployment_name,
    f.version as firmware_version,
    f.description as firmware_description,
    fd.status,
    fd.total_devices,
    fd.completed_devices,
    fd.failed_devices,
    ROUND((fd.completed_devices::FLOAT / fd.total_devices::FLOAT) * 100, 2) as completion_percentage,
    fd.created_at,
    fd.started_at,
    fd.completed_at,
    CASE 
        WHEN fd.completed_at IS NOT NULL AND fd.started_at IS NOT NULL 
        THEN EXTRACT(EPOCH FROM (fd.completed_at - fd.started_at))
        ELSE NULL 
    END as duration_seconds
FROM firmware_deployments fd
JOIN firmware f ON fd.firmware_id = f.id
ORDER BY fd.created_at DESC;
```

## ✅ 验证设置

执行完上述SQL后，您可以通过以下查询验证表是否创建成功：

```sql
-- 检查表是否存在
SELECT table_name 
FROM information_schema.tables 
WHERE table_schema = 'public' 
AND table_name IN ('firmware', 'firmware_deployments', 'device_deployment_details', 'firmware_version_history');

-- 检查视图是否存在
SELECT table_name 
FROM information_schema.views 
WHERE table_schema = 'public' 
AND table_name = 'firmware_deployment_overview';
```

## 🎉 完成

数据库设置完成后，您的ESP32控制板系统将支持以下OTA功能：

- ✅ 云端固件文件管理
- ✅ 固件版本控制
- ✅ 批量设备部署
- ✅ 部署进度监控
- ✅ 部署历史记录
- ✅ 固件升级历史追踪

现在您可以通过Web界面的"🚀 云端OTA"页面来管理ESP32设备的固件升级了！

## 📝 注意事项

1. 确保您的ESP32设备已经包含OTA升级功能的代码
2. 设备需要能够访问云服务器的IP地址
3. 固件文件大小限制为2MB
4. 支持的固件格式：.bin文件
5. 建议在升级前备份当前固件版本
