import axios, { AxiosInstance } from 'axios'

// 设备管理相关类型定义
export interface Device {
  id: string
  name: string
  ip: string
  status: 'online' | 'offline' | 'checking'
  lastSeen?: number
  deviceInfo?: DeviceInfo
}

export interface DeviceDiscoveryResult {
  ip: string
  deviceInfo?: DeviceInfo
  responseTime?: number
}

// API基础配置
const API_BASE_URL = '/api'

// 创建默认API实例
const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
})

// 创建可配置的API实例工厂
export const createApiInstance = (baseURL: string): AxiosInstance => {
  return axios.create({
    baseURL: `http://${baseURL}/api`,
    timeout: 8000,
    headers: {
      'Content-Type': 'application/json',
    },
  })
}

// 请求拦截器
api.interceptors.request.use(
  (config) => {
    console.log('API Request:', config.method?.toUpperCase(), config.url)
    return config
  },
  (error) => {
    console.error('API Request Error:', error)
    return Promise.reject(error)
  }
)

// 响应拦截器
api.interceptors.response.use(
  (response) => {
    console.log('API Response:', response.status, response.config.url)
    return response
  },
  (error) => {
    console.error('API Response Error:', error.response?.status, error.config?.url, error.message)
    return Promise.reject(error)
  }
)

// 数据类型定义
export interface DeviceInfo {
  device_name: string
  firmware_version: string
  hardware_version: string
  chip_model: string
  flash_size: number
  free_heap: number
  uptime_seconds: number
  mac_address: string
}

export interface DeviceUptime {
  uptime_seconds: number
  timestamp: number
}

export interface DeviceStatus {
  sbus_connected: boolean
  can_connected: boolean
  wifi_connected: boolean
  wifi_ip: string
  wifi_rssi: number
  sbus_channels: number[]
  motor_left_speed: number
  motor_right_speed: number
  last_sbus_time: number
  last_cmd_time: number
  // 添加缺少的字段
  free_heap: number
  total_heap: number
  uptime_seconds: number
  task_count: number
  can_tx_count: number
  can_rx_count: number
}

export interface OTAProgress {
  in_progress: boolean
  total_size: number
  written_size: number
  progress_percent: number
  status_message: string
  success: boolean
  error_message?: string
}

export interface WiFiStatus {
  state: 'disconnected' | 'connecting' | 'connected' | 'failed'
  ip_address: string
  rssi: number
  retry_count: number
  connect_time: number
}

export interface APIResponse<T> {
  status: 'success' | 'error'
  data?: T
  message?: string
}

// API函数
export const deviceAPI = {
  // 获取设备信息
  getInfo: async (): Promise<DeviceInfo> => {
    const response = await api.get<APIResponse<DeviceInfo>>('/device/info')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device info')
  },

  // 获取设备状态
  getStatus: async (): Promise<DeviceStatus> => {
    const response = await api.get<APIResponse<DeviceStatus>>('/device/status')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device status')
  },
}

export const otaAPI = {
  // 上传固件
  uploadFirmware: async (file: File, onProgress?: (progress: number) => void): Promise<void> => {
    const formData = new FormData()
    formData.append('firmware', file)

    const response = await api.post<APIResponse<void>>('/ota/upload', file, {
      headers: {
        'Content-Type': 'application/octet-stream',
      },
      onUploadProgress: (progressEvent) => {
        if (progressEvent.total && onProgress) {
          const progress = Math.round((progressEvent.loaded * 100) / progressEvent.total)
          onProgress(progress)
        }
      },
    })

    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to upload firmware')
    }
  },

  // 获取OTA进度
  getProgress: async (): Promise<OTAProgress> => {
    const response = await api.get<APIResponse<OTAProgress>>('/ota/progress')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get OTA progress')
  },

  // 回滚固件
  rollback: async (): Promise<void> => {
    const response = await api.post<APIResponse<void>>('/ota/rollback')
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to rollback firmware')
    }
  },
}

export const wifiAPI = {
  // 获取Wi-Fi状态
  getStatus: async (): Promise<WiFiStatus> => {
    const response = await api.get<APIResponse<WiFiStatus>>('/wifi/status')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get Wi-Fi status')
  },

  // 连接Wi-Fi
  connect: async (ssid: string, password: string): Promise<void> => {
    const response = await api.post<APIResponse<void>>('/wifi/connect', {
      ssid,
      password,
    })
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to connect to Wi-Fi')
    }
  },

  // 扫描Wi-Fi网络
  scan: async (): Promise<any[]> => {
    const response = await api.get<APIResponse<any[]>>('/wifi/scan')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to scan Wi-Fi networks')
  },
}

// 云设备类型定义
export interface CloudDevice {
  id: string
  device_id: string
  device_name: string
  local_ip: string
  device_type: string
  firmware_version?: string
  hardware_version?: string
  mac_address?: string
  status: 'online' | 'offline'
  registered_at: string
  last_seen?: string
  created_at: string
  updated_at: string
  // 状态信息
  sbus_connected?: boolean
  can_connected?: boolean
  wifi_connected?: boolean
  wifi_ip?: string
  wifi_rssi?: number
  free_heap?: number
  total_heap?: number
  uptime_seconds?: number
  task_count?: number
  can_tx_count?: number
  can_rx_count?: number
  sbus_channels?: number[]
  motor_left_speed?: number
  motor_right_speed?: number
  last_sbus_time?: number
  last_cmd_time?: number
  status_timestamp?: string
}

export interface DeviceCommand {
  id: string
  device_id: string
  command: string
  data: any
  status: 'pending' | 'sent' | 'completed' | 'failed'
  created_at: string
  sent_at?: string
  completed_at?: string
  error_message?: string
}

// 云服务器设备管理API (基于Supabase)
export const cloudDeviceAPI = {
  // 获取云服务器注册的设备列表
  getRegisteredDevices: async (): Promise<CloudDevice[]> => {
    const response = await axios.get('/devices')
    if (response.data.status === 'success' && response.data.devices) {
      return response.data.devices
    }
    throw new Error(response.data.message || 'Failed to get registered devices')
  },

  // 获取在线设备列表
  getOnlineDevices: async (): Promise<CloudDevice[]> => {
    const response = await axios.get('/devices/online')
    if (response.data.status === 'success' && response.data.devices) {
      return response.data.devices
    }
    throw new Error(response.data.message || 'Failed to get online devices')
  },

  // 发送指令到设备
  sendCommand: async (deviceId: string, command: string, data: any = {}): Promise<void> => {
    const response = await axios.post('/send-command', { deviceId, command, data })
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to send command')
    }
  },

  // 获取设备状态
  getDeviceStatus: async (deviceId: string): Promise<DeviceStatus> => {
    const response = await axios.get(`/api/device-status?deviceId=${deviceId}`)
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device status')
  },

  // 获取设备状态历史
  getDeviceStatusHistory: async (deviceId: string, limit: number = 100): Promise<DeviceStatus[]> => {
    const response = await axios.get(`/api/device-status-history?deviceId=${deviceId}&limit=${limit}`)
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device status history')
  },

  // 获取设备待处理指令
  getPendingCommands: async (deviceId: string): Promise<DeviceCommand[]> => {
    const response = await axios.get(`/api/device-commands?deviceId=${deviceId}`)
    if (response.data.status === 'success' && response.data.commands) {
      return response.data.commands
    }
    throw new Error(response.data.message || 'Failed to get pending commands')
  },

  // 标记指令完成
  markCommandCompleted: async (commandId: string, success: boolean = true, errorMessage?: string): Promise<void> => {
    const response = await axios.post(`/api/device-commands/${commandId}/complete`, {
      success,
      errorMessage
    })
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to mark command as completed')
    }
  },

  // 删除设备
  deleteDevice: async (deviceId: string): Promise<void> => {
    const response = await axios.delete(`/devices/${deviceId}`)
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to delete device')
    }
  },

  // 批量删除设备
  deleteDevices: async (deviceIds: string[]): Promise<void> => {
    const response = await axios.post('/devices/batch-delete', { deviceIds })
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to delete devices')
    }
  },

  // 设备注销 (ESP32主动注销)
  unregisterDevice: async (deviceId: string, reason: string = 'device_shutdown'): Promise<void> => {
    const response = await axios.post('/unregister-device', { deviceId, reason })
    if (response.data.status !== 'success') {
      throw new Error(response.data.message || 'Failed to unregister device')
    }
  }
}

// 设备管理API
export const deviceManagementAPI = {
  // 测试设备连接
  testConnection: async (ip: string): Promise<DeviceInfo> => {
    const deviceApi = createApiInstance(ip)
    const response = await deviceApi.get<APIResponse<DeviceInfo>>('/device/info')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to connect to device')
  },

  // 发现网络中的ESP32设备
  discoverDevices: async (ipRange: string = '192.168.1'): Promise<DeviceDiscoveryResult[]> => {
    const results: DeviceDiscoveryResult[] = []
    const promises: Promise<void>[] = []

    // 扫描IP范围 1-254
    for (let i = 1; i <= 254; i++) {
      const ip = `${ipRange}.${i}`

      const promise = (async () => {
        try {
          const startTime = Date.now()
          const deviceInfo = await deviceManagementAPI.testConnection(ip)
          const responseTime = Date.now() - startTime

          results.push({
            ip,
            deviceInfo,
            responseTime
          })
        } catch (error) {
          // 忽略连接失败的设备
        }
      })()

      promises.push(promise)
    }

    // 等待所有扫描完成，但设置超时
    await Promise.allSettled(promises)
    return results.sort((a, b) => (a.responseTime || 0) - (b.responseTime || 0))
  },

  // 快速发现设备（仅扫描常用IP段）
  quickDiscoverDevices: async (): Promise<DeviceDiscoveryResult[]> => {
    const commonIPs = [
      '192.168.1.100', '192.168.1.101', '192.168.1.102', '192.168.1.103',
      '192.168.4.1', '192.168.4.2', // ESP32 AP模式常用IP
      '192.168.0.100', '192.168.0.101', '192.168.0.102',
      '10.0.0.100', '10.0.0.101', '10.0.0.102'
    ]

    const results: DeviceDiscoveryResult[] = []
    const promises = commonIPs.map(async (ip) => {
      try {
        const startTime = Date.now()
        const deviceInfo = await deviceManagementAPI.testConnection(ip)
        const responseTime = Date.now() - startTime

        results.push({
          ip,
          deviceInfo,
          responseTime
        })
      } catch (error) {
        // 忽略连接失败的设备
      }
    })

    await Promise.allSettled(promises)
    return results.sort((a, b) => (a.responseTime || 0) - (b.responseTime || 0))
  },

  // 获取指定设备的信息
  getDeviceInfo: async (ip: string): Promise<DeviceInfo> => {
    const deviceApi = createApiInstance(ip)
    const response = await deviceApi.get<APIResponse<DeviceInfo>>('/device/info')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device info')
  },

  // 获取指定设备的状态
  getDeviceStatus: async (ip: string): Promise<DeviceStatus> => {
    const deviceApi = createApiInstance(ip)
    const response = await deviceApi.get<APIResponse<DeviceStatus>>('/device/status')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device status')
  },

  // 获取指定设备的运行时间（轻量级API）
  getDeviceUptime: async (ip: string): Promise<DeviceUptime> => {
    const deviceApi = createApiInstance(ip)
    const response = await deviceApi.get<APIResponse<DeviceUptime>>('/device/uptime')
    if (response.data.status === 'success' && response.data.data) {
      return response.data.data
    }
    throw new Error(response.data.message || 'Failed to get device uptime')
  },
}

export default api
