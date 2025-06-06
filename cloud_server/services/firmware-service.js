// 固件管理服务
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const multer = require('multer');

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

      // 并发部署到所有设备，但增加实时进度更新
      const deploymentPromises = devices.map(async (device, index) => {
        try {
          await this.deployToSingleDevice(device, firmware, deploymentId);
          completedCount++;

          // 实时更新部署进度
          await this.updateDeploymentProgress(deploymentId, completedCount, failedCount, devices.length);

          logger.info(`✅ 设备 ${device.device_id} 部署成功 (${completedCount}/${devices.length})`);
        } catch (error) {
          failedCount++;

          // 实时更新部署进度
          await this.updateDeploymentProgress(deploymentId, completedCount, failedCount, devices.length);

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
   * 更新部署进度
   */
  async updateDeploymentProgress(deploymentId, completedCount, failedCount, totalCount) {
    try {
      await supabaseClient
        .from('firmware_deployments')
        .update({
          completed_devices: completedCount,
          failed_devices: failedCount,
          updated_at: new Date().toISOString()
        })
        .eq('id', deploymentId);
    } catch (error) {
      logger.warn(`更新部署进度失败: ${error.message}`);
    }
  }

  /**
   * 部署固件到单个设备
   */
  async deployToSingleDevice(device, firmware, deploymentId = null) {
    try {
      logger.info(`📤 开始向设备 ${device.device_id} (${device.local_ip}) 发送OTA指令`);

      // 构建固件下载URL (云服务器提供固件下载服务)
      const firmwareUrl = `http://www.nagaflow.top/api/firmware/download/${firmware.id}`;

      // 通过Supabase指令队列发送OTA升级指令
      const { data, error } = await supabaseClient
        .from('device_commands')
        .insert({
          device_id: device.device_id,
          command: 'ota_update',
          data: {
            firmware_url: firmwareUrl,
            firmware_size: firmware.file_size,
            firmware_version: firmware.version,
            firmware_hash: firmware.file_hash,
            deployment_id: deploymentId // 添加部署ID用于跟踪
          },
          status: 'pending',
          created_at: new Date().toISOString()
        })
        .select()
        .single();

      if (error) {
        throw new Error(`发送OTA指令失败: ${error.message}`);
      }

      logger.info(`✅ OTA指令已发送到设备 ${device.device_id} (指令ID: ${data.id})`);

      // 等待设备处理指令 (最多等待5分钟)
      const maxWaitTime = 5 * 60 * 1000; // 5分钟
      const checkInterval = 10 * 1000; // 10秒检查一次
      let waitTime = 0;

      while (waitTime < maxWaitTime) {
        await new Promise(resolve => setTimeout(resolve, checkInterval));
        waitTime += checkInterval;

        // 检查指令状态
        const { data: commandStatus, error: statusError } = await supabaseClient
          .from('device_commands')
          .select('status, error_message')
          .eq('id', data.id)
          .single();

        if (statusError) {
          logger.warn(`检查指令状态失败: ${statusError.message}`);
          continue;
        }

        if (commandStatus.status === 'completed') {
          logger.info(`✅ 设备 ${device.device_id} OTA升级完成`);
          return { status: 'success', message: 'OTA升级完成' };
        } else if (commandStatus.status === 'failed') {
          throw new Error(`OTA升级失败: ${commandStatus.error_message || '未知错误'}`);
        }

        logger.info(`⏳ 等待设备 ${device.device_id} 处理OTA指令... (${Math.round(waitTime/1000)}s)`);
      }

      // 超时处理
      throw new Error('OTA升级超时，设备可能离线或升级失败');

    } catch (error) {
      logger.error(`设备 ${device.device_id} OTA部署失败: ${error.message}`);
      throw error;
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

  /**
   * 获取实时部署状态（包含进行中的部署实时信息）
   */
  async getRealtimeDeploymentStatus(limit = 50) {
    try {
      // 获取基础部署数据
      const { data: deployments, error } = await supabaseClient
        .from('firmware_deployment_overview')
        .select('*')
        .order('created_at', { ascending: false })
        .limit(limit);

      if (error) {
        throw error;
      }

      // 为进行中的部署计算实时耗时和进度
      const enhancedDeployments = await Promise.all(deployments.map(async deployment => {
        let enhancedDeployment = { ...deployment };

        // 如果是进行中的部署，计算实时耗时
        if (deployment.status === 'in_progress' && deployment.started_at) {
          const startTime = new Date(deployment.started_at);
          const currentTime = new Date();
          const durationSeconds = Math.floor((currentTime - startTime) / 1000);
          enhancedDeployment.duration_seconds = durationSeconds;

          // 获取实时进度信息
          const progressInfo = await this.getDeploymentProgress(deployment.id);
          if (progressInfo) {
            enhancedDeployment.completion_percentage = progressInfo.progress;
            enhancedDeployment.completed_devices = progressInfo.completed_devices;
            enhancedDeployment.failed_devices = progressInfo.failed_devices;
          }
        }

        // 确保进度百分比不为null
        if (enhancedDeployment.completion_percentage === null || enhancedDeployment.completion_percentage === undefined) {
          enhancedDeployment.completion_percentage = 0;
        }

        return enhancedDeployment;
      }));

      return {
        status: 'success',
        deployments: enhancedDeployments,
        count: enhancedDeployments.length
      };
    } catch (error) {
      logger.error(`获取实时部署状态失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取部署进度信息
   */
  async getDeploymentProgress(deploymentId) {
    try {
      // 查询相关的设备指令进度
      const { data: commands, error } = await supabaseClient
        .from('device_commands')
        .select('status, data')
        .eq('command', 'ota_update')
        .like('data->>deployment_id', deploymentId);

      if (error) {
        logger.warn(`获取部署进度失败: ${error.message}`);
        return null;
      }

      if (!commands || commands.length === 0) {
        return null;
      }

      // 计算总体进度
      let totalProgress = 0;
      let completedDevices = 0;
      let failedDevices = 0;

      commands.forEach(command => {
        if (command.status === 'completed') {
          completedDevices++;
          totalProgress += 100;
        } else if (command.status === 'failed') {
          failedDevices++;
        } else if (command.data && command.data.progress) {
          totalProgress += command.data.progress;
        }
      });

      const averageProgress = commands.length > 0 ? Math.round(totalProgress / commands.length) : 0;

      return {
        progress: averageProgress,
        completed_devices: completedDevices,
        failed_devices: failedDevices,
        total_devices: commands.length
      };
    } catch (error) {
      logger.warn(`获取部署进度失败: ${error.message}`);
      return null;
    }
  }

  /**
   * 更新OTA进度
   */
  async updateOTAProgress(deviceId, commandId, progressData) {
    try {
      const { progress, message } = progressData;

      // 首先获取当前的data字段
      const { data: currentCommand, error: fetchError } = await supabaseClient
        .from('device_commands')
        .select('data')
        .eq('id', commandId)
        .eq('device_id', deviceId)
        .single();

      if (fetchError) {
        throw fetchError;
      }

      // 合并进度信息
      const updatedData = {
        ...(currentCommand.data || {}),
        progress: progress,
        status_message: message || '',
        last_progress_update: new Date().toISOString()
      };

      // 更新指令的进度信息
      const { error } = await supabaseClient
        .from('device_commands')
        .update({
          data: updatedData,
          updated_at: new Date().toISOString()
        })
        .eq('id', commandId)
        .eq('device_id', deviceId);

      if (error) {
        throw error;
      }

      logger.info(`📊 设备 ${deviceId} OTA进度更新: ${progress}% - ${message}`);

      return {
        status: 'success',
        message: 'OTA进度更新成功'
      };
    } catch (error) {
      logger.error(`更新OTA进度失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 下载固件文件
   */
  async downloadFirmware(firmwareId, res) {
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

      // 检查文件是否存在
      try {
        await fs.access(firmware.file_path);
      } catch (fileError) {
        throw new Error('固件文件不存在');
      }

      logger.info(`📤 开始下载固件: ${firmware.filename} (${firmware.version})`);

      // 设置响应头
      res.setHeader('Content-Type', 'application/octet-stream');
      res.setHeader('Content-Disposition', `attachment; filename="${firmware.original_name}"`);
      res.setHeader('Content-Length', firmware.file_size);
      res.setHeader('X-Firmware-Version', firmware.version);
      res.setHeader('X-Firmware-Hash', firmware.file_hash);

      // 创建文件流并发送
      const fileStream = require('fs').createReadStream(firmware.file_path);

      fileStream.on('error', (error) => {
        logger.error(`固件文件读取失败: ${error.message}`);
        if (!res.headersSent) {
          res.status(500).json({
            status: 'error',
            message: '文件读取失败'
          });
        }
      });

      fileStream.on('end', () => {
        logger.info(`✅ 固件下载完成: ${firmware.filename}`);
      });

      // 管道传输文件
      fileStream.pipe(res);

    } catch (error) {
      logger.error(`固件下载失败: ${error.message}`);
      throw error;
    }
  }
}

module.exports = new FirmwareService();
