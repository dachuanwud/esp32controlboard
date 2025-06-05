// Supabase服务模块
const { deviceService } = require('../supabase-config');
const logger = require('../utils/logger');

class SupabaseService {
  constructor() {
    this.deviceService = deviceService;
  }

  /**
   * 注册设备
   */
  async registerDevice(deviceData) {
    try {
      // 转换字段名称格式 (驼峰 -> 下划线)
      const supabaseData = {
        device_id: deviceData.deviceId,
        device_name: deviceData.deviceName,
        local_ip: deviceData.localIP,
        device_type: deviceData.deviceType,
        firmware_version: deviceData.firmwareVersion,
        hardware_version: deviceData.hardwareVersion,
        mac_address: deviceData.macAddress
      };

      logger.info(`📱 注册设备: ${supabaseData.device_name} (${supabaseData.local_ip})`);
      logger.debug('转换后的Supabase数据:', supabaseData);

      const result = await this.deviceService.registerDevice(supabaseData);

      logger.info(`✅ 设备注册成功: ${supabaseData.device_id}`);
      return {
        status: 'success',
        message: '设备注册成功',
        device: result
      };
    } catch (error) {
      logger.error(`❌ 设备注册失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 更新设备状态
   */
  async updateDeviceStatus(deviceId, statusData) {
    try {
      logger.debug(`📊 更新设备状态: ${deviceId}`);
      
      const result = await this.deviceService.updateDeviceStatus(deviceId, statusData);
      
      logger.debug(`✅ 设备状态更新成功: ${deviceId}`);
      return result;
    } catch (error) {
      logger.error(`❌ 设备状态更新失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 发送指令到设备
   */
  async sendCommand(deviceId, command, data = {}) {
    try {
      logger.info(`📤 发送指令: ${deviceId} - ${command}`);
      
      const result = await this.deviceService.sendCommand(deviceId, command, data);
      
      logger.info(`✅ 指令发送成功: ${deviceId} - ${command}`);
      return {
        status: 'success',
        message: '指令已添加到队列',
        data: result
      };
    } catch (error) {
      logger.error(`❌ 指令发送失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取注册设备列表
   */
  async getRegisteredDevices() {
    try {
      logger.debug('📋 获取注册设备列表');
      
      const devices = await this.deviceService.getRegisteredDevices();
      
      logger.debug(`✅ 获取到 ${devices.length} 个注册设备`);
      return {
        status: 'success',
        devices,
        count: devices.length
      };
    } catch (error) {
      logger.error(`❌ 获取设备列表失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取在线设备列表
   */
  async getOnlineDevices() {
    try {
      logger.debug('📋 获取在线设备列表');
      
      const devices = await this.deviceService.getOnlineDevices();
      
      logger.debug(`✅ 获取到 ${devices.length} 个在线设备`);
      return {
        status: 'success',
        devices,
        count: devices.length
      };
    } catch (error) {
      logger.error(`❌ 获取在线设备失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 获取设备的待处理指令
   */
  async getPendingCommands(deviceId) {
    try {
      logger.debug(`📋 获取设备 ${deviceId} 的待处理指令`);

      const commands = await this.deviceService.getPendingCommands(deviceId);

      if (commands && commands.length > 0) {
        logger.debug(`✅ 获取到 ${commands.length} 个待处理指令`);
      }

      return commands;
    } catch (error) {
      logger.error(`❌ 获取待处理指令失败: ${error.message}`);
      return [];
    }
  }

  /**
   * 标记指令完成
   */
  async markCommandCompleted(commandId, success = true, errorMessage = null) {
    try {
      logger.info(`✅ 标记指令完成: ${commandId} - ${success ? '成功' : '失败'}`);

      const result = await this.deviceService.markCommandCompleted(commandId, success, errorMessage);

      return {
        status: 'success',
        message: '指令状态已更新',
        data: result
      };
    } catch (error) {
      logger.error(`❌ 标记指令完成失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 删除设备
   */
  async deleteDevice(deviceId) {
    try {
      logger.info(`🗑️ 删除设备: ${deviceId}`);

      const result = await this.deviceService.deleteDevice(deviceId);

      logger.info(`✅ 设备删除成功: ${deviceId}`);
      return {
        status: 'success',
        message: '设备删除成功',
        data: result
      };
    } catch (error) {
      logger.error(`❌ 设备删除失败: ${error.message}`);
      throw error;
    }
  }

  /**
   * 批量删除设备
   */
  async deleteDevices(deviceIds) {
    try {
      logger.info(`🗑️ 批量删除设备: ${deviceIds.length} 个`);

      const result = await this.deviceService.deleteDevices(deviceIds);

      logger.info(`✅ 批量删除完成: 成功 ${result.results.length} 个，失败 ${result.errors.length} 个`);
      return {
        status: result.errors.length === 0 ? 'success' : 'partial_success',
        message: `成功删除 ${result.results.length} 个设备，失败 ${result.errors.length} 个`,
        data: result
      };
    } catch (error) {
      logger.error(`❌ 批量删除设备失败: ${error.message}`);
      throw error;
    }
  }
}

module.exports = new SupabaseService();
