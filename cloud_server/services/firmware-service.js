// 固件管理服务
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const multer = require('multer');
const axios = require('axios');
const logger = require('../utils/logger');


// 使用统一的Supabase配置
const { supabaseAdmin } = require('../supabase-config');

const supabaseClient = supabaseAdmin;

// 固件存储目录
const FIRMWARE_DIR = path.join(__dirname, '../firmware');
const MAX_FIRMWARE_SIZE = 2 * 1024 * 1024; // 2MB

// 确保固件目录存在
async function ensureFirmwareDir() {
  try {
    await fs.access(FIRMWARE_DIR);
  } catch (error) {
    await fs.mkdir(FIRMWARE_DIR, { recursive: true });
    logger.info(`📁 创建固件目录: ${FIRMWARE_DIR}`);
  }
}

// 配置multer用于文件上传
const storage = multer.diskStorage({
  destination: async (req, file, cb) => {
    await ensureFirmwareDir();
    cb(null, FIRMWARE_DIR);
  },
  filename: (req, file, cb) => {
    const timestamp = Date.now();
    const hash = crypto.randomBytes(8).toString('hex');
    const filename = `firmware_${timestamp}_${hash}.bin`;
    cb(null, filename);
  }
});

const upload = multer({
  storage: storage,
  limits: {
    fileSize: MAX_FIRMWARE_SIZE
  },
  fileFilter: (req, file, cb) => {
    if (file.mimetype === 'application/octet-stream' || file.originalname.endsWith('.bin')) {
      cb(null, true);
    } else {
      cb(new Error('只支持.bin格式的固件文件'));
    }
  }
});

class FirmwareService {
  constructor() {
    this.uploadMiddleware = upload.single('firmware');
  }

  /**
   * 上传固件文件
   */
  async uploadFirmware(req, res) {
    return new Promise((resolve, reject) => {
      this.uploadMiddleware(req, res, async (err) => {
        if (err) {
          return reject(new Error(`文件上传失败: ${err.message}`));
        }

        if (!req.file) {
          return reject(new Error('未选择固件文件'));
        }

        try {
          const file = req.file;
          const { version, description, deviceType = 'ESP32' } = req.body;

          if (!version) {
            // 删除已上传的文件
            await fs.unlink(file.path);
            return reject(new Error('固件版本号不能为空'));
          }

          // 计算文件哈希
          const fileBuffer = await fs.readFile(file.path);
          const hash = crypto.createHash('sha256').update(fileBuffer).digest('hex');

          // 保存固件信息到数据库
          const firmwareData = {
            filename: file.filename,
            original_name: file.originalname,
            version: version,
            description: description || '',
            device_type: deviceType,
            file_size: file.size,
            file_hash: hash,
            file_path: file.path,
            upload_time: new Date().toISOString(),
            status: 'available'
          };

          const result = await this.saveFirmwareInfo(firmwareData);

          logger.info(`📦 固件上传成功: ${file.originalname} (${version})`);
          
          resolve({
            status: 'success',
            message: '固件上传成功',
            firmware: result
          });
        } catch (error) {
          // 删除已上传的文件
          if (req.file) {
            await fs.unlink(req.file.path).catch(() => {});
          }
          reject(error);
        }
      });
    });
  }

  /**
   * 保存固件信息到数据库
   */
  async saveFirmwareInfo(firmwareData) {
    try {
      // 这里使用Supabase存储固件信息
      // 需要创建firmware表
      const { data, error } = await supabaseClient
        .from('firmware')
        .insert(firmwareData)
        .select()
        .single();

      if (error) {
        throw error;
      }

      return data;
    } catch (error) {
      logger.error(`保存固件信息失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取固件列表
   */
  async getFirmwareList() {
    try {
      const { data, error } = await supabaseClient
        .from('firmware')
        .select('*')
        .order('upload_time', { ascending: false });

      if (error) {
        throw error;
      }

      return {
        status: 'success',
        firmware: data,
        count: data.length
      };
    } catch (error) {
      logger.error(`获取固件列表失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 删除固件
   */
  async deleteFirmware(firmwareId) {
    try {
      // 获取固件信息
      const { data: firmware, error: fetchError } = await supabaseClient
        .from('firmware')
        .select('*')
        .eq('id', firmwareId)
        .single();

      if (fetchError || !firmware) {
        throw new Error('固件不存在');
      }

      // 删除文件
      try {
        await fs.unlink(firmware.file_path);
      } catch (fileError) {
        logger.warn(`删除固件文件失败: ${fileError.message}`);
      }

      // 删除数据库记录
      const { error: deleteError } = await supabaseClient
        .from('firmware')
        .delete()
        .eq('id', firmwareId);

      if (deleteError) {
        throw deleteError;
      }

      logger.info(`🗑️ 固件删除成功: ${firmware.filename}`);

      return {
        status: 'success',
        message: '固件删除成功'
      };
    } catch (error) {
      logger.error(`删除固件失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 部署固件到设备
   */
  async deployFirmware({ firmwareId, deviceIds, deploymentName }) {
    try {
      if (!firmwareId || !deviceIds || !Array.isArray(deviceIds) || deviceIds.length === 0) {
        throw new Error('参数错误：需要提供固件ID和设备列表');
      }

      // 获取固件信息
      const { data: firmware, error: firmwareError } = await supabaseClient
        .from('firmware')
        .select('*')
        .eq('id', firmwareId)
        .single();

      if (firmwareError || !firmware) {
        throw new Error('固件不存在');
      }

      // 获取设备信息
      const { data: devices, error: devicesError } = await supabaseClient
        .from('esp32_devices')
        .select('*')
        .in('device_id', deviceIds)
        .eq('status', 'online');

      if (devicesError) {
        throw devicesError;
      }

      if (devices.length === 0) {
        throw new Error('没有找到在线设备');
      }

      // 创建部署记录
      const deploymentData = {
        deployment_name: deploymentName || `部署_${firmware.version}_${new Date().toISOString()}`,
        firmware_id: firmwareId,
        target_devices: deviceIds,
        status: 'pending',
        created_at: new Date().toISOString(),
        total_devices: deviceIds.length,
        completed_devices: 0,
        failed_devices: 0
      };

      const { data: deployment, error: deploymentError } = await supabaseClient
        .from('firmware_deployments')
        .insert(deploymentData)
        .select()
        .single();

      if (deploymentError) {
        throw deploymentError;
      }

      // 异步执行部署
      this.executeDeployment(deployment.id, firmware, devices);

      logger.info(`🚀 开始固件部署: ${deploymentName} (${devices.length}个设备)`);

      return {
        status: 'success',
        message: '固件部署已启动',
        deployment: deployment
      };
    } catch (error) {
      logger.error(`固件部署失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 执行固件部署
   */
  async executeDeployment(deploymentId, firmware, devices) {
    try {
      // 更新部署状态为进行中
      await supabaseClient
        .from('firmware_deployments')
        .update({ status: 'in_progress', started_at: new Date().toISOString() })
        .eq('id', deploymentId);

      let completedCount = 0;
      let failedCount = 0;

      // 并发部署到所有设备
      const deploymentPromises = devices.map(async (device) => {
        try {
          await this.deployToSingleDevice(device, firmware);
          completedCount++;
          logger.info(`✅ 设备 ${device.device_id} 部署成功`);
        } catch (error) {
          failedCount++;
          logger.error(`❌ 设备 ${device.device_id} 部署失败: ${error.message}`);
        }
      });

      await Promise.allSettled(deploymentPromises);

      // 更新最终状态
      const finalStatus = failedCount === 0 ? 'completed' : 
                         completedCount === 0 ? 'failed' : 'partial';

      await supabaseClient
        .from('firmware_deployments')
        .update({
          status: finalStatus,
          completed_at: new Date().toISOString(),
          completed_devices: completedCount,
          failed_devices: failedCount
        })
        .eq('id', deploymentId);

      logger.info(`🏁 部署完成: ${completedCount}成功, ${failedCount}失败`);
    } catch (error) {
      logger.error(`部署执行失败: ${error.message}`);
      
      // 更新部署状态为失败
      await supabaseClient
        .from('firmware_deployments')
        .update({
          status: 'failed',
          completed_at: new Date().toISOString(),
          error_message: error.message
        })
        .eq('id', deploymentId);
    }
  }

  /**
   * 部署固件到单个设备
   */
  async deployToSingleDevice(device, firmware) {
    try {
      const deviceUrl = `http://${device.local_ip}`;
      const firmwareBuffer = await fs.readFile(firmware.file_path);

      logger.info(`📤 开始向设备 ${device.device_id} (${device.local_ip}) 部署固件`);

      // 发送固件到设备
      const response = await axios.post(`${deviceUrl}/api/ota/upload`, firmwareBuffer, {
        headers: {
          'Content-Type': 'application/octet-stream',
          'Content-Length': firmwareBuffer.length
        },
        timeout: 60000, // 60秒超时
        maxContentLength: MAX_FIRMWARE_SIZE,
        maxBodyLength: MAX_FIRMWARE_SIZE
      });

      if (response.data.status !== 'success') {
        throw new Error(response.data.message || '设备OTA升级失败');
      }

      return response.data;
    } catch (error) {
      if (error.code === 'ECONNREFUSED') {
        throw new Error('设备连接失败');
      } else if (error.code === 'ETIMEDOUT') {
        throw new Error('设备响应超时');
      } else {
        throw new Error(`部署失败: ${error.message}`);
      }
    }
  }

  /**
   * 获取部署状态
   */
  async getDeploymentStatus(deploymentId) {
    try {
      const { data, error } = await supabaseClient
        .from('firmware_deployments')
        .select(`
          *,
          firmware (
            version,
            description,
            device_type
          )
        `)
        .eq('id', deploymentId)
        .single();

      if (error) {
        throw error;
      }

      return {
        status: 'success',
        deployment: data
      };
    } catch (error) {
      logger.error(`获取部署状态失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取部署历史
   */
  async getDeploymentHistory(limit = 50) {
    try {
      const { data, error } = await supabaseClient
        .from('firmware_deployment_overview')
        .select('*')
        .order('created_at', { ascending: false })
        .limit(limit);

      if (error) {
        throw error;
      }

      return {
        status: 'success',
        deployments: data,
        count: data.length
      };
    } catch (error) {
      logger.error(`获取部署历史失败: ${error.message}`);
      throw error;
    }
  }
}

module.exports = new FirmwareService();
