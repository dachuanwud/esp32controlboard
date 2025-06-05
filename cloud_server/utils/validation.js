// 数据验证工具模块

/**
 * 验证设备注册数据
 */
function validateDeviceRegistration(data) {
  const errors = [];

  // 必要字段验证
  if (!data.deviceId) {
    errors.push('设备ID不能为空');
  }

  if (!data.localIP) {
    errors.push('本地IP地址不能为空');
  }

  // IP地址格式验证
  if (data.localIP) {
    const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
    if (!ipRegex.test(data.localIP)) {
      errors.push('IP地址格式无效');
    } else {
      // 验证IP地址范围
      const parts = data.localIP.split('.');
      for (const part of parts) {
        const num = parseInt(part, 10);
        if (num < 0 || num > 255) {
          errors.push('IP地址范围无效');
          break;
        }
      }
    }
  }

  // 设备ID格式验证
  if (data.deviceId && data.deviceId.length > 50) {
    errors.push('设备ID长度不能超过50个字符');
  }

  // 设备名称验证
  if (data.deviceName && data.deviceName.length > 100) {
    errors.push('设备名称长度不能超过100个字符');
  }

  return {
    isValid: errors.length === 0,
    errors
  };
}

/**
 * 验证指令数据
 */
function validateCommand(data) {
  const errors = [];

  if (!data.deviceId) {
    errors.push('设备ID不能为空');
  }

  if (!data.command) {
    errors.push('指令不能为空');
  }

  // 指令名称验证
  if (data.command && typeof data.command !== 'string') {
    errors.push('指令必须是字符串类型');
  }

  if (data.command && data.command.length > 50) {
    errors.push('指令名称长度不能超过50个字符');
  }

  // 指令数据验证
  if (data.data && typeof data.data !== 'object') {
    errors.push('指令数据必须是对象类型');
  }

  return {
    isValid: errors.length === 0,
    errors
  };
}

/**
 * 验证设备状态数据
 */
function validateDeviceStatus(data) {
  const errors = [];

  if (!data.deviceId) {
    errors.push('设备ID不能为空');
  }

  // 验证数值类型字段
  const numericFields = ['wifi_rssi', 'free_heap', 'total_heap', 'uptime_seconds', 'task_count'];
  for (const field of numericFields) {
    if (data[field] !== undefined && typeof data[field] !== 'number') {
      errors.push(`${field} 必须是数值类型`);
    }
  }

  // 验证布尔类型字段
  const booleanFields = ['sbus_connected', 'can_connected', 'wifi_connected'];
  for (const field of booleanFields) {
    if (data[field] !== undefined && typeof data[field] !== 'boolean') {
      errors.push(`${field} 必须是布尔类型`);
    }
  }

  // 验证SBUS通道数据
  if (data.sbus_channels && !Array.isArray(data.sbus_channels)) {
    errors.push('SBUS通道数据必须是数组类型');
  }

  if (data.sbus_channels && data.sbus_channels.length > 16) {
    errors.push('SBUS通道数量不能超过16个');
  }

  return {
    isValid: errors.length === 0,
    errors
  };
}

/**
 * 清理和标准化设备数据
 */
function sanitizeDeviceData(data) {
  const sanitized = {};

  // 字符串字段清理 (包括空字符串和null值)
  const stringFields = ['deviceId', 'deviceName', 'localIP', 'deviceType', 'firmwareVersion', 'hardwareVersion', 'macAddress'];
  for (const field of stringFields) {
    if (data[field] !== undefined && data[field] !== null) {
      sanitized[field] = String(data[field]).trim();
    }
  }

  // 设置默认值
  sanitized.deviceType = sanitized.deviceType || 'ESP32';
  if (sanitized.deviceId && !sanitized.deviceName) {
    sanitized.deviceName = `${sanitized.deviceType}-${sanitized.deviceId}`;
  }

  return sanitized;
}

/**
 * 清理和标准化状态数据
 */
function sanitizeStatusData(data) {
  const sanitized = { ...data };

  // 确保数值字段是数字类型
  const numericFields = ['wifi_rssi', 'free_heap', 'total_heap', 'uptime_seconds', 'task_count', 'can_tx_count', 'can_rx_count'];
  for (const field of numericFields) {
    if (sanitized[field] !== undefined) {
      sanitized[field] = Number(sanitized[field]) || 0;
    }
  }

  // 确保布尔字段是布尔类型
  const booleanFields = ['sbus_connected', 'can_connected', 'wifi_connected'];
  for (const field of booleanFields) {
    if (sanitized[field] !== undefined) {
      sanitized[field] = Boolean(sanitized[field]);
    }
  }

  // 清理SBUS通道数据
  if (sanitized.sbus_channels && Array.isArray(sanitized.sbus_channels)) {
    sanitized.sbus_channels = sanitized.sbus_channels.slice(0, 16).map(ch => Number(ch) || 0);
  }

  return sanitized;
}

module.exports = {
  validateDeviceRegistration,
  validateCommand,
  validateDeviceStatus,
  sanitizeDeviceData,
  sanitizeStatusData
};
