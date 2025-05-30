import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react'
import { Device, deviceManagementAPI, createApiInstance } from '../services/api'
import DeviceStorageService from '../services/deviceStorage'

interface DeviceContextType {
  devices: Device[]
  selectedDevice: Device | null
  loading: boolean
  error: string | null
  
  // 设备管理方法
  addDevice: (device: Device) => void
  updateDevice: (device: Device) => void
  deleteDevice: (deviceId: string) => void
  selectDevice: (device: Device | null) => void
  refreshDevices: () => Promise<void>
  testDeviceConnection: (device: Device) => Promise<boolean>
  
  // API实例获取
  getCurrentApiInstance: () => ReturnType<typeof createApiInstance> | null
}

const DeviceContext = createContext<DeviceContextType | undefined>(undefined)

interface DeviceProviderProps {
  children: ReactNode
}

export const DeviceProvider: React.FC<DeviceProviderProps> = ({ children }) => {
  const [devices, setDevices] = useState<Device[]>([])
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // 初始化设备数据
  useEffect(() => {
    loadDevicesFromStorage()
  }, [])

  // 从存储加载设备
  const loadDevicesFromStorage = () => {
    try {
      const storedDevices = DeviceStorageService.getDevices()
      setDevices(storedDevices)

      // 恢复选中的设备
      const selectedDeviceId = DeviceStorageService.getSelectedDeviceId()
      if (selectedDeviceId) {
        const device = storedDevices.find(d => d.id === selectedDeviceId)
        if (device) {
          setSelectedDevice(device)
        }
      } else if (storedDevices.length > 0) {
        // 如果没有选中的设备，默认选择第一个在线设备
        const onlineDevice = storedDevices.find(d => d.status === 'online') || storedDevices[0]
        setSelectedDevice(onlineDevice)
        DeviceStorageService.setSelectedDeviceId(onlineDevice.id)
      }
    } catch (err) {
      console.error('Failed to load devices from storage:', err)
      setError('加载设备列表失败')
    }
  }

  // 添加设备
  const addDevice = (device: Device) => {
    setDevices(prev => {
      const updated = [...prev, device]
      DeviceStorageService.saveDevices(updated)
      return updated
    })

    // 如果是第一个设备，自动选中
    if (devices.length === 0) {
      selectDevice(device)
    }
  }

  // 更新设备
  const updateDevice = (updatedDevice: Device) => {
    setDevices(prev => {
      const updated = prev.map(d => d.id === updatedDevice.id ? updatedDevice : d)
      DeviceStorageService.saveDevices(updated)
      return updated
    })

    // 如果更新的是当前选中的设备，也更新选中状态
    if (selectedDevice?.id === updatedDevice.id) {
      setSelectedDevice(updatedDevice)
    }
  }

  // 删除设备
  const deleteDevice = (deviceId: string) => {
    setDevices(prev => {
      const updated = prev.filter(d => d.id !== deviceId)
      DeviceStorageService.saveDevices(updated)
      return updated
    })

    // 如果删除的是当前选中的设备，选择其他设备
    if (selectedDevice?.id === deviceId) {
      const remainingDevices = devices.filter(d => d.id !== deviceId)
      if (remainingDevices.length > 0) {
        const nextDevice = remainingDevices.find(d => d.status === 'online') || remainingDevices[0]
        selectDevice(nextDevice)
      } else {
        selectDevice(null)
      }
    }
  }

  // 选择设备
  const selectDevice = (device: Device | null) => {
    setSelectedDevice(device)
    DeviceStorageService.setSelectedDeviceId(device?.id)
    setError(null)
  }

  // 刷新所有设备状态
  const refreshDevices = async () => {
    if (devices.length === 0) return

    setLoading(true)
    setError(null)

    try {
      const updatedDevices = await Promise.all(
        devices.map(async (device) => {
          try {
            const deviceInfo = await deviceManagementAPI.getDeviceInfo(device.ip)
            return {
              ...device,
              status: 'online' as const,
              lastSeen: Date.now(),
              deviceInfo
            }
          } catch (err) {
            return {
              ...device,
              status: 'offline' as const
            }
          }
        })
      )

      setDevices(updatedDevices)
      DeviceStorageService.saveDevices(updatedDevices)

      // 更新选中设备的状态
      if (selectedDevice) {
        const updatedSelectedDevice = updatedDevices.find(d => d.id === selectedDevice.id)
        if (updatedSelectedDevice) {
          setSelectedDevice(updatedSelectedDevice)
        }
      }
    } catch (err) {
      setError('刷新设备状态失败')
    } finally {
      setLoading(false)
    }
  }

  // 测试设备连接
  const testDeviceConnection = async (device: Device): Promise<boolean> => {
    try {
      await deviceManagementAPI.testConnection(device.ip)
      
      // 更新设备状态
      const updatedDevice = {
        ...device,
        status: 'online' as const,
        lastSeen: Date.now()
      }
      updateDevice(updatedDevice)
      
      return true
    } catch (err) {
      // 更新设备状态为离线
      const updatedDevice = {
        ...device,
        status: 'offline' as const
      }
      updateDevice(updatedDevice)
      
      return false
    }
  }

  // 获取当前设备的API实例
  const getCurrentApiInstance = () => {
    if (!selectedDevice) return null
    return createApiInstance(selectedDevice.ip)
  }

  // 定期检查设备状态
  useEffect(() => {
    if (devices.length === 0) return

    const interval = setInterval(() => {
      // 每30秒检查一次设备状态
      refreshDevices()
    }, 30000)

    return () => clearInterval(interval)
  }, [devices.length])

  const contextValue: DeviceContextType = {
    devices,
    selectedDevice,
    loading,
    error,
    addDevice,
    updateDevice,
    deleteDevice,
    selectDevice,
    refreshDevices,
    testDeviceConnection,
    getCurrentApiInstance
  }

  return (
    <DeviceContext.Provider value={contextValue}>
      {children}
    </DeviceContext.Provider>
  )
}

// 自定义Hook
export const useDevice = (): DeviceContextType => {
  const context = useContext(DeviceContext)
  if (context === undefined) {
    throw new Error('useDevice must be used within a DeviceProvider')
  }
  return context
}

// 自定义Hook：获取当前设备的API实例
export const useDeviceAPI = () => {
  const { selectedDevice, getCurrentApiInstance } = useDevice()
  
  return {
    selectedDevice,
    apiInstance: getCurrentApiInstance(),
    isConnected: selectedDevice?.status === 'online'
  }
}

export default DeviceContext
