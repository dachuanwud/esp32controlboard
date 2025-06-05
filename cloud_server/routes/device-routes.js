// 设备管理路由模块
const express = require('express');
const router = express.Router();
const supabaseService = require('../services/supabase-service');
const proxyService = require('../services/proxy-service');
const { validateDeviceRegistration, validateCommand, validateDeviceStatus, sanitizeDeviceData, sanitizeStatusData } = require('../utils/validation');
const logger = require('../utils/logger');

/**
 * 设备注册接口 (Supabase版本)
 * POST /register-device
 */
router.post('/register-device', async (req, res) => {
  try {
    logger.info('📱 收到设备注册请求 (Supabase)');
    logger.debug('原始请求数据:', req.body);

    // 数据验证
    const validation = validateDeviceRegistration(req.body);
    logger.debug('数据验证结果:', validation);

    if (!validation.isValid) {
      return res.status(400).json({
        error: '数据验证失败',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    // 数据清理
    const deviceData = sanitizeDeviceData(req.body);
    logger.debug('清理后的数据:', deviceData);

    // 注册到Supabase
    const result = await supabaseService.registerDevice(deviceData);

    res.json(result);
  } catch (error) {
    logger.error(`设备注册失败: ${error.message}`);
    res.status(500).json({
      error: '设备注册失败',
      message: error.message
    });
  }
});

/**
 * 设备注册接口 (本地版本)
 * POST /register-device-local
 */
router.post('/register-device-local', (req, res) => {
  try {
    logger.info('📱 收到设备注册请求 (本地)');
    
    // 数据验证
    const validation = validateDeviceRegistration(req.body);
    if (!validation.isValid) {
      return res.status(400).json({
        error: '数据验证失败',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    // 数据清理
    const deviceData = sanitizeDeviceData(req.body);
    
    // 注册到本地内存
    const result = proxyService.registerDevice(deviceData);
    
    res.json({
      status: 'success',
      message: '设备注册成功 (本地)',
      device: result
    });
  } catch (error) {
    logger.error(`设备注册失败 (本地): ${error.message}`);
    res.status(500).json({
      error: '设备注册失败',
      message: error.message
    });
  }
});

/**
 * 设备状态更新接口 (Supabase版本)
 * POST /device-status
 */
router.post('/device-status', async (req, res) => {
  try {
    const { deviceId, ...statusData } = req.body;
    
    logger.debug(`📊 收到设备状态更新: ${deviceId} (Supabase)`);
    
    // 数据验证
    const validation = validateDeviceStatus({ deviceId, ...statusData });
    if (!validation.isValid) {
      return res.status(400).json({
        status: 'error',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    // 数据清理
    const cleanStatusData = sanitizeStatusData(statusData);
    
    // 更新到Supabase
    const result = await supabaseService.updateDeviceStatus(deviceId, cleanStatusData);
    
    res.json(result);
  } catch (error) {
    logger.error(`设备状态更新失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 设备状态更新接口 (本地版本)
 * POST /device-status-local
 */
router.post('/device-status-local', (req, res) => {
  try {
    const { deviceId, ...statusData } = req.body;
    
    logger.debug(`📊 收到设备状态更新: ${deviceId} (本地)`);
    
    // 数据验证
    const validation = validateDeviceStatus({ deviceId, ...statusData });
    if (!validation.isValid) {
      return res.status(400).json({
        status: 'error',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    // 数据清理
    const cleanStatusData = sanitizeStatusData(statusData);
    
    // 更新到本地内存
    const result = proxyService.updateDeviceStatus(deviceId, cleanStatusData);
    
    res.json(result);
  } catch (error) {
    logger.error(`设备状态更新失败 (本地): ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 发送指令接口 (Supabase版本)
 * POST /send-command
 */
router.post('/send-command', async (req, res) => {
  try {
    logger.info('📤 收到发送指令请求 (Supabase)');
    
    // 数据验证
    const validation = validateCommand(req.body);
    if (!validation.isValid) {
      return res.status(400).json({
        status: 'error',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    const { deviceId, command, data = {} } = req.body;
    
    // 发送到Supabase
    const result = await supabaseService.sendCommand(deviceId, command, data);
    
    res.json(result);
  } catch (error) {
    logger.error(`发送指令失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 发送指令接口 (本地版本)
 * POST /send-command-local
 */
router.post('/send-command-local', (req, res) => {
  try {
    logger.info('📤 收到发送指令请求 (本地)');
    
    // 数据验证
    const validation = validateCommand(req.body);
    if (!validation.isValid) {
      return res.status(400).json({
        status: 'error',
        message: validation.errors.join(', '),
        errors: validation.errors
      });
    }

    const { deviceId, command, data = {} } = req.body;
    
    // 发送到本地内存
    const result = proxyService.sendCommand(deviceId, command, data);
    
    res.json(result);
  } catch (error) {
    logger.error(`发送指令失败 (本地): ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 获取注册设备列表 (Supabase版本)
 * GET /devices
 */
router.get('/devices', async (req, res) => {
  try {
    logger.debug('📋 获取注册设备列表 (Supabase)');
    
    const result = await supabaseService.getRegisteredDevices();
    
    res.json(result);
  } catch (error) {
    logger.error(`获取设备列表失败: ${error.message}`);
    res.status(500).json({
      error: '获取设备列表失败',
      message: error.message
    });
  }
});

/**
 * 获取注册设备列表 (本地版本)
 * GET /devices-local
 */
router.get('/devices-local', (req, res) => {
  try {
    logger.debug('📋 获取注册设备列表 (本地)');
    
    const result = proxyService.getRegisteredDevices();
    
    res.json(result);
  } catch (error) {
    logger.error(`获取设备列表失败 (本地): ${error.message}`);
    res.status(500).json({
      error: '获取设备列表失败',
      message: error.message
    });
  }
});

/**
 * 获取在线设备列表
 * GET /devices/online
 */
router.get('/devices/online', async (req, res) => {
  try {
    logger.debug('📋 获取在线设备列表');
    
    const result = await supabaseService.getOnlineDevices();
    
    res.json(result);
  } catch (error) {
    logger.error(`获取在线设备失败: ${error.message}`);
    res.status(500).json({
      error: '获取在线设备失败',
      message: error.message
    });
  }
});

/**
 * 切换默认设备
 * POST /switch-device
 */
router.post('/switch-device', (req, res) => {
  try {
    const { deviceId } = req.body;

    if (!deviceId) {
      return res.status(400).json({
        error: '参数错误',
        message: '设备ID不能为空'
      });
    }

    const result = proxyService.switchDevice(deviceId);

    res.json(result);
  } catch (error) {
    logger.error(`设备切换失败: ${error.message}`);
    res.status(404).json({
      error: '设备切换失败',
      message: error.message
    });
  }
});

/**
 * 删除设备接口 (Supabase版本)
 * DELETE /devices/:deviceId
 */
router.delete('/devices/:deviceId', async (req, res) => {
  try {
    const { deviceId } = req.params;

    logger.info(`🗑️ 收到设备删除请求: ${deviceId} (Supabase)`);

    if (!deviceId) {
      return res.status(400).json({
        error: '参数错误',
        message: '设备ID不能为空'
      });
    }

    // 删除设备
    const result = await supabaseService.deleteDevice(deviceId);

    res.json(result);
  } catch (error) {
    logger.error(`设备删除失败: ${error.message}`);
    res.status(500).json({
      error: '设备删除失败',
      message: error.message
    });
  }
});

/**
 * 删除设备接口 (本地版本)
 * DELETE /devices-local/:deviceId
 */
router.delete('/devices-local/:deviceId', (req, res) => {
  try {
    const { deviceId } = req.params;

    logger.info(`🗑️ 收到设备删除请求: ${deviceId} (本地)`);

    if (!deviceId) {
      return res.status(400).json({
        error: '参数错误',
        message: '设备ID不能为空'
      });
    }

    // 删除本地设备
    const result = proxyService.deleteDevice(deviceId);

    res.json({
      status: 'success',
      message: '设备删除成功 (本地)',
      data: result
    });
  } catch (error) {
    logger.error(`设备删除失败 (本地): ${error.message}`);
    res.status(500).json({
      error: '设备删除失败',
      message: error.message
    });
  }
});

/**
 * 批量删除设备接口
 * POST /devices/batch-delete
 */
router.post('/devices/batch-delete', async (req, res) => {
  try {
    const { deviceIds } = req.body;

    logger.info(`🗑️ 收到批量删除设备请求: ${deviceIds?.length || 0} 个设备`);

    if (!deviceIds || !Array.isArray(deviceIds) || deviceIds.length === 0) {
      return res.status(400).json({
        error: '参数错误',
        message: '设备ID列表不能为空'
      });
    }

    // 限制批量删除数量
    if (deviceIds.length > 50) {
      return res.status(400).json({
        error: '参数错误',
        message: '单次最多只能删除50个设备'
      });
    }

    const results = [];
    const errors = [];

    // 逐个删除设备
    for (const deviceId of deviceIds) {
      try {
        await supabaseService.deleteDevice(deviceId);
        results.push({ deviceId, status: 'success' });
        logger.info(`✅ 设备删除成功: ${deviceId}`);
      } catch (error) {
        errors.push({ deviceId, error: error.message });
        logger.error(`❌ 设备删除失败: ${deviceId} - ${error.message}`);
      }
    }

    res.json({
      status: errors.length === 0 ? 'success' : 'partial_success',
      message: `成功删除 ${results.length} 个设备，失败 ${errors.length} 个`,
      results,
      errors
    });
  } catch (error) {
    logger.error(`批量删除设备失败: ${error.message}`);
    res.status(500).json({
      error: '批量删除设备失败',
      message: error.message
    });
  }
});

/**
 * 设备注销接口 (ESP32主动注销)
 * POST /unregister-device
 */
router.post('/unregister-device', async (req, res) => {
  try {
    const { deviceId, reason = 'device_shutdown' } = req.body;

    logger.info(`📤 收到设备注销请求: ${deviceId}, 原因: ${reason}`);

    if (!deviceId) {
      return res.status(400).json({
        error: '参数错误',
        message: '设备ID不能为空'
      });
    }

    // 更新设备状态为离线，而不是直接删除
    const result = await supabaseService.updateDeviceStatus(deviceId, {
      status: 'offline',
      unregister_reason: reason,
      unregistered_at: new Date().toISOString()
    });

    res.json({
      status: 'success',
      message: '设备注销成功',
      data: result
    });
  } catch (error) {
    logger.error(`设备注销失败: ${error.message}`);
    res.status(500).json({
      error: '设备注销失败',
      message: error.message
    });
  }
});

module.exports = router;
