// Supabase配置文件
const { createClient } = require('@supabase/supabase-js')

// Supabase项目配置
const SUPABASE_URL = 'https://hfmifzmuwcmtgyjfhxvx.supabase.co'
const SUPABASE_ANON_KEY = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhmbWlmem11d2NtdGd5amZoeHZ4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDkwMjIzNTEsImV4cCI6MjA2NDU5ODM1MX0.YPTUXgVdb8YMwwUWmG4nGdGIOvnTe6zvavMieL-RlTE'
const SUPABASE_SERVICE_KEY = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhmbWlmem11d2NtdGd5amZoeHZ4Iiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTc0OTAyMjM1MSwiZXhwIjoyMDY0NTk4MzUxfQ.HpaBASMAYTtkd-RTOXM14Y4cbg41YFeFAralMRPlopI'

// 创建Supabase客户端实例
const supabaseClient = createClient(SUPABASE_URL, SUPABASE_ANON_KEY)
const supabaseAdmin = createClient(SUPABASE_URL, SUPABASE_SERVICE_KEY)

// 设备管理服务类
class SupabaseDeviceService {
  constructor() {
    this.client = supabaseClient
    this.admin = supabaseAdmin
  }

  /**
   * 注册设备
   */
  async registerDevice(deviceData) {
    try {
      const { device_id, device_name, local_ip, device_type = 'ESP32', firmware_version, hardware_version, mac_address } = deviceData

      // 验证必要参数
      if (!device_id || !local_ip) {
        throw new Error('设备ID和本地IP不能为空')
      }

      // 先检查设备是否已存在
      const { data: existingDevice } = await this.client
        .from('esp32_devices')
        .select('device_id')
        .eq('device_id', device_id)
        .single()

      if (existingDevice) {
        // 设备已存在，更新信息
        const { data, error } = await this.admin
          .from('esp32_devices')
          .update({
            device_name: device_name || `${device_type}-${device_id}`,
            local_ip,
            device_type,
            firmware_version,
            hardware_version,
            mac_address,
            status: 'online',
            last_seen: new Date().toISOString(),
            updated_at: new Date().toISOString()
          })
          .eq('device_id', device_id)
          .select()
          .single()

        if (error) {
          console.error('更新设备信息失败:', error)
          throw error
        }

        console.log(`📱 设备信息已更新到Supabase: ${device_name} (${local_ip})`)
        return data
      } else {
        // 设备不存在，创建新设备
        const { data, error } = await this.admin
          .from('esp32_devices')
          .insert({
            device_id,
            device_name: device_name || `${device_type}-${device_id}`,
            local_ip,
            device_type,
            firmware_version,
            hardware_version,
            mac_address,
            status: 'online',
            registered_at: new Date().toISOString(),
            last_seen: new Date().toISOString()
          })
          .select()
          .single()

        if (error) {
          console.error('设备注册失败:', error)
          throw error
        }

        console.log(`📱 设备已注册到Supabase: ${device_name} (${local_ip})`)
        return data
      }
    } catch (error) {
      console.error('注册设备时发生错误:', error)
      throw error
    }
  }

  /**
   * 获取所有注册的设备
   */
  async getRegisteredDevices() {
    try {
      const { data, error } = await this.client
        .from('device_details')
        .select('*')
        .order('registered_at', { ascending: false })

      if (error) {
        console.error('获取设备列表失败:', error)
        throw error
      }

      return data || []
    } catch (error) {
      console.error('获取设备列表时发生错误:', error)
      throw error
    }
  }

  /**
   * 获取在线设备
   */
  async getOnlineDevices() {
    try {
      const { data, error } = await this.client
        .from('online_devices')
        .select('*')
        .order('last_seen', { ascending: false })

      if (error) {
        console.error('获取在线设备失败:', error)
        throw error
      }

      return data || []
    } catch (error) {
      console.error('获取在线设备时发生错误:', error)
      throw error
    }
  }

  /**
   * 更新设备状态
   */
  async updateDeviceStatus(deviceId, statusData) {
    try {
      // 调用存储过程更新状态
      const { data, error } = await this.admin.rpc('update_device_status', {
        p_device_id: deviceId,
        p_status_data: statusData
      })

      if (error) {
        console.error('更新设备状态失败:', error)
        throw error
      }

      console.log(`[STATUS] 设备 ${deviceId} 状态已更新到Supabase`)
      return data
    } catch (error) {
      console.error('更新设备状态时发生错误:', error)
      throw error
    }
  }

  /**
   * 发送指令到设备
   */
  async sendCommand(deviceId, command, commandData = {}) {
    try {
      // 调用存储过程发送指令
      const { data, error } = await this.admin.rpc('send_device_command', {
        p_device_id: deviceId,
        p_command: command,
        p_data: commandData
      })

      if (error) {
        console.error('发送指令失败:', error)
        throw error
      }

      console.log(`[COMMAND] 指令已发送到设备 ${deviceId}: ${command}`)
      return data
    } catch (error) {
      console.error('发送指令时发生错误:', error)
      throw error
    }
  }

  /**
   * 获取设备的最新状态
   */
  async getDeviceStatus(deviceId) {
    try {
      const { data, error } = await this.client
        .from('device_status')
        .select('*')
        .eq('device_id', deviceId)
        .order('timestamp', { ascending: false })
        .limit(1)
        .single()

      if (error && error.code !== 'PGRST116') { // PGRST116 = no rows returned
        console.error('获取设备状态失败:', error)
        throw error
      }

      return data
    } catch (error) {
      console.error('获取设备状态时发生错误:', error)
      throw error
    }
  }

  /**
   * 获取设备的历史状态
   */
  async getDeviceStatusHistory(deviceId, limit = 100) {
    try {
      const { data, error } = await this.client
        .from('device_status')
        .select('*')
        .eq('device_id', deviceId)
        .order('timestamp', { ascending: false })
        .limit(limit)

      if (error) {
        console.error('获取设备历史状态失败:', error)
        throw error
      }

      return data || []
    } catch (error) {
      console.error('获取设备历史状态时发生错误:', error)
      throw error
    }
  }

  /**
   * 获取设备的待处理指令
   */
  async getPendingCommands(deviceId) {
    try {
      const { data, error } = await this.client
        .from('device_commands')
        .select('*')
        .eq('device_id', deviceId)
        .in('status', ['pending', 'sent'])
        .order('created_at', { ascending: true })

      if (error) {
        console.error('获取待处理指令失败:', error)
        throw error
      }

      // 如果有指令，将状态更新为'sent'
      if (data && data.length > 0) {
        const commandIds = data.map(cmd => cmd.id);
        await this.admin
          .from('device_commands')
          .update({ status: 'sent', sent_at: new Date().toISOString() })
          .in('id', commandIds)
          .eq('status', 'pending');
      }

      return data || []
    } catch (error) {
      console.error('获取待处理指令时发生错误:', error)
      throw error
    }
  }

  /**
   * 标记指令为已完成
   */
  async markCommandCompleted(commandId, success = true, errorMessage = null) {
    try {
      const { data, error } = await this.admin
        .from('device_commands')
        .update({
          status: success ? 'completed' : 'failed',
          completed_at: new Date().toISOString(),
          error_message: errorMessage
        })
        .eq('id', commandId)

      if (error) {
        console.error('标记指令状态失败:', error)
        throw error
      }

      return data
    } catch (error) {
      console.error('标记指令状态时发生错误:', error)
      throw error
    }
  }

  /**
   * 删除设备 (级联删除相关数据)
   */
  async deleteDevice(deviceId) {
    try {
      console.log(`🗑️ 开始删除设备: ${deviceId}`)

      // 首先检查设备是否存在
      const { data: existingDevice } = await this.client
        .from('esp32_devices')
        .select('device_id, device_name')
        .eq('device_id', deviceId)
        .single()

      if (!existingDevice) {
        throw new Error(`设备不存在: ${deviceId}`)
      }

      console.log(`📋 找到设备: ${existingDevice.device_name} (${deviceId})`)

      // 删除设备状态历史记录
      const { error: statusError } = await this.admin
        .from('device_status')
        .delete()
        .eq('device_id', deviceId)

      if (statusError) {
        console.warn('删除设备状态记录时出现警告:', statusError)
      } else {
        console.log(`🗑️ 已删除设备状态记录: ${deviceId}`)
      }

      // 删除设备指令记录
      const { error: commandError } = await this.admin
        .from('device_commands')
        .delete()
        .eq('device_id', deviceId)

      if (commandError) {
        console.warn('删除设备指令记录时出现警告:', commandError)
      } else {
        console.log(`🗑️ 已删除设备指令记录: ${deviceId}`)
      }

      // 最后删除设备主记录
      const { data, error } = await this.admin
        .from('esp32_devices')
        .delete()
        .eq('device_id', deviceId)
        .select()

      if (error) {
        console.error('删除设备主记录失败:', error)
        throw error
      }

      console.log(`✅ 设备及相关数据已完全删除: ${existingDevice.device_name} (${deviceId})`)
      return {
        deleted_device: existingDevice,
        deleted_records: data
      }
    } catch (error) {
      console.error('删除设备时发生错误:', error)
      throw error
    }
  }

  /**
   * 批量删除设备
   */
  async deleteDevices(deviceIds) {
    try {
      console.log(`🗑️ 开始批量删除设备: ${deviceIds.length} 个`)

      const results = []
      const errors = []

      for (const deviceId of deviceIds) {
        try {
          const result = await this.deleteDevice(deviceId)
          results.push({ deviceId, result })
        } catch (error) {
          errors.push({ deviceId, error: error.message })
        }
      }

      console.log(`✅ 批量删除完成: 成功 ${results.length} 个，失败 ${errors.length} 个`)
      return { results, errors }
    } catch (error) {
      console.error('批量删除设备时发生错误:', error)
      throw error
    }
  }

  /**
   * 订阅设备状态变化
   */
  subscribeToDeviceChanges(callback) {
    const subscription = this.client
      .channel('device-changes')
      .on('postgres_changes', 
        { event: '*', schema: 'public', table: 'esp32_devices' },
        callback
      )
      .on('postgres_changes',
        { event: '*', schema: 'public', table: 'device_status' },
        callback
      )
      .subscribe()

    return subscription
  }

  /**
   * 取消订阅
   */
  unsubscribe(subscription) {
    if (subscription) {
      this.client.removeChannel(subscription)
    }
  }
}

// 创建全局实例
const deviceService = new SupabaseDeviceService()

module.exports = {
  supabaseClient,
  supabaseAdmin,
  deviceService,
  SUPABASE_URL,
  SUPABASE_ANON_KEY
}
