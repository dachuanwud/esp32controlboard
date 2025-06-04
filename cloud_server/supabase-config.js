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

      // 调用存储过程注册设备
      const { data, error } = await this.admin.rpc('register_device', {
        p_device_id: device_id,
        p_device_name: device_name,
        p_local_ip: local_ip,
        p_device_type: device_type,
        p_firmware_version: firmware_version,
        p_hardware_version: hardware_version,
        p_mac_address: mac_address
      })

      if (error) {
        console.error('设备注册失败:', error)
        throw error
      }

      console.log(`📱 设备已注册到Supabase: ${device_name} (${local_ip})`)
      return data
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
        .eq('status', 'pending')
        .order('created_at', { ascending: true })

      if (error) {
        console.error('获取待处理指令失败:', error)
        throw error
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
   * 删除设备
   */
  async deleteDevice(deviceId) {
    try {
      const { data, error } = await this.admin
        .from('esp32_devices')
        .delete()
        .eq('device_id', deviceId)

      if (error) {
        console.error('删除设备失败:', error)
        throw error
      }

      console.log(`🗑️ 设备已从Supabase删除: ${deviceId}`)
      return data
    } catch (error) {
      console.error('删除设备时发生错误:', error)
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
