import React, { useState, useEffect } from 'react'
import { Card, Badge, Spinner, Alert, Button, ProgressBar } from 'react-bootstrap'
import { DeviceInfo as DeviceInfoType, deviceManagementAPI } from '../services/api'
import { useDeviceAPI } from '../contexts/DeviceContext'

const DeviceInfo: React.FC = () => {
  const [deviceInfo, setDeviceInfo] = useState<DeviceInfoType | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [refreshing, setRefreshing] = useState(false)
  
  // 运行时间相关状态
  const [currentUptime, setCurrentUptime] = useState<number>(0)
  const [lastSyncTime, setLastSyncTime] = useState<number>(0)
  const [uptimeTimerActive, setUptimeTimerActive] = useState(false)

  const { selectedDevice, apiInstance, isConnected } = useDeviceAPI()

  const fetchDeviceInfo = async (isManualRefresh = false) => {
    if (!selectedDevice || !apiInstance) {
      setError('未选择设备或设备未连接')
      setLoading(false)
      return
    }

    try {
      if (isManualRefresh) {
        setRefreshing(true)
      } else {
        setLoading(true)
      }
      setError(null)

      // 使用设备管理API获取信息
      const info = await deviceManagementAPI.getDeviceInfo(selectedDevice.ip)
      setDeviceInfo(info)
      
      // 更新运行时间基准
      setCurrentUptime(info.uptime_seconds)
      setLastSyncTime(Date.now())
      setUptimeTimerActive(true)
    } catch (err) {
      setError(err instanceof Error ? err.message : '获取设备信息失败')
      setUptimeTimerActive(false)
    } finally {
      setLoading(false)
      setRefreshing(false)
    }
  }

  // 运行时间实时更新定时器
  useEffect(() => {
    let uptimeInterval: ReturnType<typeof setInterval> | null = null

    if (uptimeTimerActive && deviceInfo) {
      // 每秒更新运行时间显示
      uptimeInterval = setInterval(() => {
        const elapsedSeconds = Math.floor((Date.now() - lastSyncTime) / 1000)
        setCurrentUptime(deviceInfo.uptime_seconds + elapsedSeconds)
      }, 1000)
    }

    return () => {
      if (uptimeInterval) {
        clearInterval(uptimeInterval)
      }
    }
  }, [uptimeTimerActive, deviceInfo, lastSyncTime])

  // 定期同步运行时间（每5分钟）
  useEffect(() => {
    let syncInterval: ReturnType<typeof setInterval> | null = null

    if (selectedDevice && isConnected && deviceInfo) {
      syncInterval = setInterval(async () => {
        try {
          // 使用轻量级运行时间API进行同步，减少数据传输
          const uptimeData = await deviceManagementAPI.getDeviceUptime(selectedDevice.ip)
          setCurrentUptime(uptimeData.uptime_seconds)
          setLastSyncTime(Date.now())
        } catch (err) {
          console.warn('运行时间同步失败:', err)
        }
      }, 300000) // 5分钟同步一次
    }

    return () => {
      if (syncInterval) {
        clearInterval(syncInterval)
      }
    }
  }, [selectedDevice, isConnected, deviceInfo])

  useEffect(() => {
    if (selectedDevice && isConnected) {
      fetchDeviceInfo()
      // 每30秒刷新一次设备信息（不包括运行时间）
      const interval = setInterval(() => fetchDeviceInfo(), 30000)
      return () => clearInterval(interval)
    } else {
      setDeviceInfo(null)
      setLoading(false)
      setError(selectedDevice ? '设备未连接' : '未选择设备')
      setUptimeTimerActive(false)
    }
  }, [selectedDevice, isConnected])

  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes'
    const k = 1024
    const sizes = ['Bytes', 'KB', 'MB', 'GB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  const formatUptime = (seconds: number): string => {
    const days = Math.floor(seconds / 86400)
    const hours = Math.floor((seconds % 86400) / 3600)
    const minutes = Math.floor((seconds % 3600) / 60)
    const secs = seconds % 60

    if (days > 0) {
      return `${days}天 ${hours}小时 ${minutes}分钟`
    } else if (hours > 0) {
      return `${hours}小时 ${minutes}分钟`
    } else if (minutes > 0) {
      return `${minutes}分钟 ${secs}秒`
    } else {
      return `${secs}秒`
    }
  }

  // 计算内存使用率
  const getMemoryUsagePercent = (freeHeap: number, totalHeap = 320000): number => {
    return Math.round(((totalHeap - freeHeap) / totalHeap) * 100)
  }

  // 获取内存状态颜色
  const getMemoryStatusColor = (freeHeap: number): string => {
    if (freeHeap < 30000) return 'danger'
    if (freeHeap < 50000) return 'warning'
    return 'success'
  }

  if (loading) {
    return (
      <div className="text-center py-5">
        <div className="loading-shimmer" style={{ width: '100%', height: '300px', borderRadius: '12px' }}>
          <div className="d-flex flex-column justify-content-center align-items-center h-100">
            <Spinner animation="border" variant="primary" style={{ width: '3rem', height: '3rem' }} />
            <p className="mt-3 text-muted">正在获取设备信息...</p>
          </div>
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <Alert variant="danger" className="card-custom">
        <Alert.Heading>❌ 连接错误</Alert.Heading>
        <p className="mb-3">{error}</p>
        <Button variant="outline-danger" onClick={() => fetchDeviceInfo()} className="btn-custom">
          🔄 重新连接
        </Button>
      </Alert>
    )
  }

  if (!deviceInfo) {
    return (
      <Alert variant="warning" className="card-custom">
        <Alert.Heading>⚠️ 数据异常</Alert.Heading>
        <p>未能获取到设备信息，请检查设备连接状态</p>
      </Alert>
    )
  }

  return (
    <div className="card-enter">
      {/* 操作栏 */}
      <div className="d-flex justify-content-between align-items-center mb-4">
        <div className="d-flex align-items-center">
          <span className="status-indicator status-connected me-2"></span>
          <span className="text-muted">设备在线 • 最后更新: {new Date().toLocaleTimeString()}</span>
        </div>
        <Button
          variant="outline-primary"
          onClick={() => fetchDeviceInfo(true)}
          disabled={refreshing}
          className="btn-custom"
        >
          {refreshing ? (
            <>
              <Spinner size="sm" className="me-2" />
              刷新中...
            </>
          ) : (
            <>🔄 刷新数据</>
          )}
        </Button>
      </div>

      {/* 设备信息网格 */}
      <div className="data-grid">
        {/* 基本信息卡片 */}
        <Card className="card-custom">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">🔧 基本信息</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            <div className="data-item mb-3">
              <div className="data-label">设备名称</div>
              <div className="data-value">{deviceInfo.device_name}</div>
            </div>
            <div className="data-item mb-3">
              <div className="data-label">固件版本</div>
              <div className="data-value">
                <Badge bg="primary" className="badge-custom">{deviceInfo.firmware_version}</Badge>
              </div>
            </div>
            <div className="data-item mb-3">
              <div className="data-label">硬件版本</div>
              <div className="data-value">
                <Badge bg="secondary" className="badge-custom">{deviceInfo.hardware_version}</Badge>
              </div>
            </div>
            <div className="data-item mb-3">
              <div className="data-label">芯片型号</div>
              <div className="data-value">{deviceInfo.chip_model}</div>
            </div>
            <div className="data-item">
              <div className="data-label">MAC地址</div>
              <div className="data-value">
                <code style={{ fontFamily: 'var(--font-family-mono)', fontSize: '0.9rem' }}>
                  {deviceInfo.mac_address}
                </code>
              </div>
            </div>
          </Card.Body>
        </Card>

        {/* 系统资源卡片 */}
        <Card className="card-custom">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">💾 系统资源</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            <div className="data-item mb-3">
              <div className="data-label">Flash存储</div>
              <div className="data-value">{formatBytes(deviceInfo.flash_size)}</div>
            </div>

            <div className="data-item mb-3">
              <div className="data-label">内存状态</div>
              <div className="data-value">
                <div className="d-flex align-items-center mb-2">
                  <span className={`text-${getMemoryStatusColor(deviceInfo.free_heap)}`}>
                    {formatBytes(deviceInfo.free_heap)} 可用
                  </span>
                </div>
                <ProgressBar
                  now={getMemoryUsagePercent(deviceInfo.free_heap)}
                  variant={getMemoryStatusColor(deviceInfo.free_heap)}
                  className="progress-custom"
                  style={{ height: '8px' }}
                />
                <small className="text-muted">
                  使用率: {getMemoryUsagePercent(deviceInfo.free_heap)}%
                </small>
              </div>
            </div>

            <div className="data-item">
              <div className="data-label">运行时间</div>
              <div className="data-value">
                <div className="d-flex align-items-center">
                  <span className="me-2">⏱️</span>
                  {formatUptime(currentUptime)}
                  {uptimeTimerActive && (
                    <span className="ms-2">
                      <span 
                        className="badge bg-success badge-custom" 
                        style={{ fontSize: '0.75rem' }}
                        title="运行时间实时更新中"
                      >
                        实时
                      </span>
                    </span>
                  )}
                </div>
              </div>
            </div>
          </Card.Body>
        </Card>
      </div>

      {/* 系统说明卡片 */}
      <Card className="card-custom">
        <Card.Header className="card-header-custom">
          <h5 className="mb-0">ℹ️ 系统说明</h5>
        </Card.Header>
        <Card.Body className="card-body-custom">
          <div className="mb-4">
            <h6 className="text-gradient mb-3">🎛️ ESP32控制板系统</h6>
            <p className="text-secondary mb-3">
              基于ESP-IDF框架和FreeRTOS实时操作系统的智能电机控制系统，
              集成了多种通信协议和现代化的Web管理界面。
            </p>
          </div>

          <div className="row">
            <div className="col-md-6">
              <h6 className="mb-3">🚀 核心功能</h6>
              <ul className="list-unstyled">
                <li className="mb-2">
                  <span className="badge bg-primary me-2">📡</span>
                  SBUS遥控信号接收和解析
                </li>
                <li className="mb-2">
                  <span className="badge bg-success me-2">🚗</span>
                  CAN总线电机驱动控制
                </li>
                <li className="mb-2">
                  <span className="badge bg-info me-2">🌐</span>
                  Wi-Fi网络连接和Web界面
                </li>
                <li className="mb-2">
                  <span className="badge bg-warning me-2">🔄</span>
                  OTA固件无线更新
                </li>
                <li className="mb-2">
                  <span className="badge bg-secondary me-2">📊</span>
                  实时状态监控和数据展示
                </li>
              </ul>
            </div>

            <div className="col-md-6">
              <h6 className="mb-3">⚡ 技术特性</h6>
              <ul className="list-unstyled">
                <li className="mb-2">
                  <span className="text-primary">•</span>
                  <strong className="ms-2">FreeRTOS</strong> 多任务实时系统
                </li>
                <li className="mb-2">
                  <span className="text-success">•</span>
                  <strong className="ms-2">ESP-IDF</strong> 官方开发框架
                </li>
                <li className="mb-2">
                  <span className="text-info">•</span>
                  <strong className="ms-2">React</strong> 现代化Web界面
                </li>
                <li className="mb-2">
                  <span className="text-warning">•</span>
                  <strong className="ms-2">双分区</strong> OTA安全更新
                </li>
                <li className="mb-2">
                  <span className="text-secondary">•</span>
                  <strong className="ms-2">RESTful</strong> API接口设计
                </li>
              </ul>
            </div>
          </div>

          <div className="mt-4 p-3 bg-light rounded">
            <small className="text-muted">
              💡 <strong>使用提示:</strong>
              通过Web界面可以实时监控设备状态、上传新固件、配置网络参数等操作。
              建议定期检查设备状态并及时更新固件以获得最佳性能。
            </small>
          </div>
        </Card.Body>
      </Card>
    </div>
  )
}

export default DeviceInfo
