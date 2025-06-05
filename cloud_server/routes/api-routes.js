// API路由模块
const express = require('express');
const router = express.Router();
const supabaseService = require('../services/supabase-service');
const logger = require('../utils/logger');

/**
 * 健康检查接口
 * GET /health
 */
router.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    database: 'supabase',
    version: '2.0.0'
  });
});

/**
 * 系统状态接口
 * GET /status
 */
router.get('/status', (req, res) => {
  const uptime = process.uptime();
  const memoryUsage = process.memoryUsage();
  
  res.json({
    status: 'running',
    uptime: Math.floor(uptime),
    memory: {
      rss: Math.round(memoryUsage.rss / 1024 / 1024) + ' MB',
      heapTotal: Math.round(memoryUsage.heapTotal / 1024 / 1024) + ' MB',
      heapUsed: Math.round(memoryUsage.heapUsed / 1024 / 1024) + ' MB',
      external: Math.round(memoryUsage.external / 1024 / 1024) + ' MB'
    },
    timestamp: new Date().toISOString()
  });
});

/**
 * 获取设备状态 (通过设备ID)
 * GET /api/device-status?deviceId=xxx
 */
router.get('/device-status', async (req, res) => {
  try {
    const { deviceId } = req.query;
    
    if (!deviceId) {
      return res.status(400).json({
        status: 'error',
        message: '设备ID不能为空'
      });
    }

    // 这里可以实现获取特定设备状态的逻辑
    // 目前返回基本响应
    res.json({
      status: 'success',
      message: '设备状态获取成功',
      deviceId: deviceId,
      data: {
        // 可以从Supabase获取具体状态数据
        online: true,
        lastSeen: new Date().toISOString()
      }
    });
  } catch (error) {
    logger.error(`获取设备状态失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 标记指令完成
 * POST /api/device-commands/:commandId/complete
 */
router.post('/device-commands/:commandId/complete', async (req, res) => {
  try {
    const commandId = req.params.commandId;
    const { success = true, errorMessage = null } = req.body;

    if (!commandId) {
      return res.status(400).json({
        status: 'error',
        message: '指令ID不能为空'
      });
    }

    const result = await supabaseService.markCommandCompleted(commandId, success, errorMessage);

    res.json(result);
  } catch (error) {
    logger.error(`标记指令完成失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 删除设备
 * DELETE /api/devices/:deviceId
 */
router.delete('/devices/:deviceId', async (req, res) => {
  try {
    const deviceId = req.params.deviceId;

    if (!deviceId) {
      return res.status(400).json({
        status: 'error',
        message: '设备ID不能为空'
      });
    }

    const result = await supabaseService.deleteDevice(deviceId);

    res.json(result);
  } catch (error) {
    logger.error(`删除设备失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 获取API版本信息
 * GET /api/version
 */
router.get('/version', (req, res) => {
  res.json({
    version: '2.0.0',
    name: 'ESP32 Unified Server',
    description: 'ESP32控制板统一服务器架构',
    features: [
      'Supabase集成',
      'ESP32代理',
      '设备管理',
      '指令队列',
      '状态监控',
      'OTA固件管理'
    ],
    timestamp: new Date().toISOString()
  });
});

/**
 * 上传固件文件
 * POST /api/firmware/upload
 */
router.post('/firmware/upload', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.uploadFirmware(req, res);
    res.json(result);
  } catch (error) {
    logger.error(`固件上传失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 获取固件列表
 * GET /api/firmware/list
 */
router.get('/firmware/list', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.getFirmwareList();
    res.json(result);
  } catch (error) {
    logger.error(`获取固件列表失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 删除固件
 * DELETE /api/firmware/:firmwareId
 */
router.delete('/firmware/:firmwareId', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.deleteFirmware(req.params.firmwareId);
    res.json(result);
  } catch (error) {
    logger.error(`删除固件失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 触发设备OTA升级
 * POST /api/firmware/deploy
 */
router.post('/firmware/deploy', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.deployFirmware(req.body);
    res.json(result);
  } catch (error) {
    logger.error(`固件部署失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 获取OTA升级状态
 * GET /api/firmware/deployment-status/:deploymentId
 */
router.get('/firmware/deployment-status/:deploymentId', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.getDeploymentStatus(req.params.deploymentId);
    res.json(result);
  } catch (error) {
    logger.error(`获取部署状态失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 获取部署历史
 * GET /api/firmware/deployments
 */
router.get('/firmware/deployments', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.getDeploymentHistory();
    res.json(result);
  } catch (error) {
    logger.error(`获取部署历史失败: ${error.message}`);
    res.status(500).json({
      status: 'error',
      message: error.message
    });
  }
});

/**
 * 下载固件文件
 * GET /api/firmware/download/:firmwareId
 */
router.get('/firmware/download/:firmwareId', async (req, res) => {
  try {
    const firmwareService = require('../services/firmware-service');
    const result = await firmwareService.downloadFirmware(req.params.firmwareId, res);
    // 响应已在downloadFirmware中处理
  } catch (error) {
    logger.error(`固件下载失败: ${error.message}`);
    if (!res.headersSent) {
      res.status(500).json({
        status: 'error',
        message: error.message
      });
    }
  }
});

/**
 * 获取服务器配置信息 (仅开发环境)
 * GET /api/config
 */
router.get('/config', (req, res) => {
  if (process.env.NODE_ENV === 'production') {
    return res.status(403).json({
      error: '禁止访问',
      message: '生产环境不允许访问配置信息'
    });
  }

  const config = require('../config/server-config');
  
  // 移除敏感信息
  const safeConfig = {
    server: config.server,
    esp32: {
      timeout: config.esp32.timeout,
      retryAttempts: config.esp32.retryAttempts
    },
    static: config.static,
    logging: config.logging
  };

  res.json({
    status: 'success',
    config: safeConfig,
    timestamp: new Date().toISOString()
  });
});

module.exports = router;
