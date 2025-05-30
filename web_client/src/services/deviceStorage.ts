import { Device } from './api'

const STORAGE_KEY = 'esp32_devices'

export interface DeviceStorageData {
  devices: Device[]
  selectedDeviceId?: string
  lastUpdated: number
}

/**
 * 设备存储管理服务
 */
export class DeviceStorageService {
  /**
   * 获取所有已保存的设备
   */
  static getDevices(): Device[] {
    try {
      const data = localStorage.getItem(STORAGE_KEY)
      if (!data) return []
      
      const parsed: DeviceStorageData = JSON.parse(data)
      return parsed.devices || []
    } catch (error) {
      console.error('Failed to load devices from storage:', error)
      return []
    }
  }

  /**
   * 保存设备列表
   */
  static saveDevices(devices: Device[]): void {
    try {
      const data: DeviceStorageData = {
        devices,
        selectedDeviceId: this.getSelectedDeviceId(),
        lastUpdated: Date.now()
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data))
    } catch (error) {
      console.error('Failed to save devices to storage:', error)
    }
  }

  /**
   * 添加新设备
   */
  static addDevice(device: Device): void {
    const devices = this.getDevices()
    const existingIndex = devices.findIndex(d => d.id === device.id)
    
    if (existingIndex >= 0) {
      // 更新现有设备
      devices[existingIndex] = device
    } else {
      // 添加新设备
      devices.push(device)
    }
    
    this.saveDevices(devices)
  }

  /**
   * 删除设备
   */
  static removeDevice(deviceId: string): void {
    const devices = this.getDevices()
    const filteredDevices = devices.filter(d => d.id !== deviceId)
    this.saveDevices(filteredDevices)
    
    // 如果删除的是当前选中的设备，清除选择
    if (this.getSelectedDeviceId() === deviceId) {
      this.setSelectedDeviceId(undefined)
    }
  }

  /**
   * 更新设备信息
   */
  static updateDevice(deviceId: string, updates: Partial<Device>): void {
    const devices = this.getDevices()
    const deviceIndex = devices.findIndex(d => d.id === deviceId)
    
    if (deviceIndex >= 0) {
      devices[deviceIndex] = { ...devices[deviceIndex], ...updates }
      this.saveDevices(devices)
    }
  }

  /**
   * 获取指定设备
   */
  static getDevice(deviceId: string): Device | undefined {
    const devices = this.getDevices()
    return devices.find(d => d.id === deviceId)
  }

  /**
   * 检查设备是否存在
   */
  static deviceExists(ip: string): boolean {
    const devices = this.getDevices()
    return devices.some(d => d.ip === ip)
  }

  /**
   * 根据IP查找设备
   */
  static findDeviceByIp(ip: string): Device | undefined {
    const devices = this.getDevices()
    return devices.find(d => d.ip === ip)
  }

  /**
   * 获取当前选中的设备ID
   */
  static getSelectedDeviceId(): string | undefined {
    try {
      const data = localStorage.getItem(STORAGE_KEY)
      if (!data) return undefined
      
      const parsed: DeviceStorageData = JSON.parse(data)
      return parsed.selectedDeviceId
    } catch (error) {
      console.error('Failed to get selected device ID:', error)
      return undefined
    }
  }

  /**
   * 设置当前选中的设备ID
   */
  static setSelectedDeviceId(deviceId: string | undefined): void {
    try {
      const devices = this.getDevices()
      const data: DeviceStorageData = {
        devices,
        selectedDeviceId: deviceId,
        lastUpdated: Date.now()
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data))
    } catch (error) {
      console.error('Failed to set selected device ID:', error)
    }
  }

  /**
   * 获取当前选中的设备
   */
  static getSelectedDevice(): Device | undefined {
    const selectedId = this.getSelectedDeviceId()
    if (!selectedId) return undefined
    
    return this.getDevice(selectedId)
  }

  /**
   * 清除所有数据
   */
  static clearAll(): void {
    try {
      localStorage.removeItem(STORAGE_KEY)
    } catch (error) {
      console.error('Failed to clear device storage:', error)
    }
  }

  /**
   * 导出设备数据
   */
  static exportDevices(): string {
    const data: DeviceStorageData = {
      devices: this.getDevices(),
      selectedDeviceId: this.getSelectedDeviceId(),
      lastUpdated: Date.now()
    }
    return JSON.stringify(data, null, 2)
  }

  /**
   * 导入设备数据
   */
  static importDevices(jsonData: string): boolean {
    try {
      const data: DeviceStorageData = JSON.parse(jsonData)
      
      // 验证数据格式
      if (!Array.isArray(data.devices)) {
        throw new Error('Invalid device data format')
      }
      
      // 验证每个设备的必要字段
      for (const device of data.devices) {
        if (!device.id || !device.name || !device.ip) {
          throw new Error('Invalid device format: missing required fields')
        }
      }
      
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data))
      return true
    } catch (error) {
      console.error('Failed to import devices:', error)
      return false
    }
  }

  /**
   * 生成唯一的设备ID
   */
  static generateDeviceId(ip: string, deviceName?: string): string {
    const timestamp = Date.now()
    const random = Math.random().toString(36).substr(2, 9)
    const ipSuffix = ip.split('.').pop() || '000'
    const nameSuffix = deviceName ? deviceName.replace(/[^a-zA-Z0-9]/g, '').toLowerCase() : 'esp32'
    
    return `${nameSuffix}-${ipSuffix}-${random}`
  }

  /**
   * 验证IP地址格式
   */
  static isValidIP(ip: string): boolean {
    const ipRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/
    return ipRegex.test(ip)
  }

  /**
   * 获取存储统计信息
   */
  static getStorageStats(): {
    deviceCount: number
    lastUpdated: number
    storageSize: number
  } {
    try {
      const data = localStorage.getItem(STORAGE_KEY)
      const devices = this.getDevices()
      
      return {
        deviceCount: devices.length,
        lastUpdated: data ? JSON.parse(data).lastUpdated || 0 : 0,
        storageSize: data ? data.length : 0
      }
    } catch (error) {
      return {
        deviceCount: 0,
        lastUpdated: 0,
        storageSize: 0
      }
    }
  }
}

export default DeviceStorageService
