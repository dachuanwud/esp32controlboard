import React, { useState, useEffect } from 'react'
import { Card, Row, Col, Badge, Spinner, Alert, Button, ProgressBar } from 'react-bootstrap'
import { cloudDeviceAPI, DeviceStatus as DeviceStatusType, CloudDevice } from '../services/api'

const CloudDeviceStatus: React.FC = () => {
  const [devices, setDevices] = useState<CloudDevice[]>([])
  const [selectedDevice, setSelectedDevice] = useState<CloudDevice | null>(null)
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatusType | null>(null)
  const [loading, setLoading] = useState(true)

  const [error, setError] = useState<string | null>(null)
  const [autoRefresh, setAutoRefresh] = useState(true)

  // 获取云设备列表
  const fetchCloudDevices = async () => {
    try {
      setLoading(true)
      setError(null)
      const cloudDevices = await cloudDeviceAPI.getRegisteredDevices()
      setDevices(cloudDevices)
      
      // 如果没有选中设备，自动选择第一个在线设备
      if (!selectedDevice && cloudDevices.length > 0) {
        const onlineDevice = cloudDevices.find(d => d.status === 'online') || cloudDevices[0]
        setSelectedDevice(onlineDevice)
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : '获取云设备列表失败')
    } finally {
      setLoading(false)
    }
  }

  // 从设备数据中提取状态信息
  const extractDeviceStatus = (device: CloudDevice): DeviceStatusType | null => {
    if (!device || device.status !== 'online') {
      return null
    }

    // 将CloudDevice的状态数据转换为DeviceStatus格式
    return {
      sbus_connected: device.sbus_connected || false,
      can_connected: device.can_connected || false,
      wifi_connected: device.wifi_connected || false,
      wifi_ip: device.wifi_ip || device.local_ip,
      wifi_rssi: device.wifi_rssi || 0,
      sbus_channels: device.sbus_channels || [],
      motor_left_speed: device.motor_left_speed || 0,
      motor_right_speed: device.motor_right_speed || 0,
      last_sbus_time: device.last_sbus_time || 0,
      last_cmd_time: device.last_cmd_time || 0,
      free_heap: device.free_heap || 0,
      total_heap: device.total_heap || 0,
      uptime_seconds: device.uptime_seconds || 0,
      task_count: device.task_count || 0,
      can_tx_count: device.can_tx_count || 0,
      can_rx_count: device.can_rx_count || 0
    }
  }

  // 选择设备
  const handleSelectDevice = (device: CloudDevice) => {
    setSelectedDevice(device)
    const status = extractDeviceStatus(device)
    setDeviceStatus(status)
  }

  // 手动刷新状态
  const handleRefreshStatus = async () => {
    if (selectedDevice) {
      // 重新获取设备列表以获取最新状态
      await fetchCloudDevices()
      // 找到当前选中的设备并更新状态
      const updatedDevice = devices.find(d => d.device_id === selectedDevice.device_id)
      if (updatedDevice) {
        const status = extractDeviceStatus(updatedDevice)
        setDeviceStatus(status)
      }
    }
  }

  // 格式化字节数
  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes'
    const k = 1024
    const sizes = ['Bytes', 'KB', 'MB', 'GB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  // 格式化运行时间
  const formatUptime = (seconds: number): string => {
    const days = Math.floor(seconds / 86400)
    const hours = Math.floor((seconds % 86400) / 3600)
    const minutes = Math.floor((seconds % 3600) / 60)
    const secs = seconds % 60

    if (days > 0) {
      return `${days}天 ${hours}小时 ${minutes}分钟`
    } else if (hours > 0) {
      return `${hours}小时 ${minutes}分钟 ${secs}秒`
    } else if (minutes > 0) {
      return `${minutes}分钟 ${secs}秒`
    } else {
      return `${secs}秒`
    }
  }

  // 获取状态徽章
  const getStatusBadge = (connected: boolean, label: string) => {
    return (
      <div className="d-flex align-items-center mb-2">
        <Badge bg={connected ? 'success' : 'secondary'} className="me-2">
          {connected ? '✅' : '❌'}
        </Badge>
        <span>{label}</span>
      </div>
    )
  }

  // 获取信号强度颜色
  const getSignalColor = (rssi: number): string => {
    if (rssi > -50) return 'success'
    if (rssi > -70) return 'warning'
    return 'danger'
  }

  useEffect(() => {
    fetchCloudDevices()
  }, [])

  useEffect(() => {
    if (selectedDevice) {
      const status = extractDeviceStatus(selectedDevice)
      setDeviceStatus(status)
    }
  }, [selectedDevice])

  useEffect(() => {
    if (!autoRefresh || !selectedDevice) return

    const interval = setInterval(async () => {
      // 自动刷新时重新获取设备列表
      await fetchCloudDevices()
    }, 5000) // 每5秒刷新一次设备列表

    return () => clearInterval(interval)
  }, [autoRefresh, selectedDevice])

  // 当设备列表更新时，更新选中设备的状态
  useEffect(() => {
    if (selectedDevice && devices.length > 0) {
      const updatedDevice = devices.find(d => d.device_id === selectedDevice.device_id)
      if (updatedDevice) {
        setSelectedDevice(updatedDevice)
        const status = extractDeviceStatus(updatedDevice)
        setDeviceStatus(status)
      }
    }
  }, [devices])

  // 清除错误消息
  useEffect(() => {
    if (error) {
      const timer = setTimeout(() => setError(null), 5000)
      return () => clearTimeout(timer)
    }
  }, [error])

  if (loading) {
    return (
      <div className="text-center py-5">
        <Spinner animation="border" />
        <div className="mt-2">正在加载云设备...</div>
      </div>
    )
  }

  if (devices.length === 0) {
    return (
      <Alert variant="info" className="card-custom">
        <Alert.Heading>📭 暂无注册设备</Alert.Heading>
        <p>目前没有ESP32设备注册到云服务器。</p>
        <p className="mb-0">
          <small>💡 提示：ESP32设备需要调用 <code>/register-device</code> 接口进行注册</small>
        </p>
      </Alert>
    )
  }

  return (
    <div className="card-enter">
      {/* 页面标题和操作栏 */}
      <div className="d-flex justify-content-between align-items-center mb-4">
        <div>
          <h2 className="mb-1">📈 云设备实时状态</h2>
          <p className="text-muted mb-0">监控注册到云服务器的ESP32设备状态</p>
        </div>
        <div className="d-flex gap-2 align-items-center">
          <div className="form-check form-switch">
            <input
              className="form-check-input"
              type="checkbox"
              id="autoRefresh"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
            />
            <label className="form-check-label" htmlFor="autoRefresh">
              自动刷新
            </label>
          </div>
          <Button
            variant="outline-primary"
            onClick={handleRefreshStatus}
            disabled={loading || !selectedDevice}
            className="btn-custom"
          >
            {loading ? (
              <>
                <Spinner size="sm" className="me-2" />
                刷新中...
              </>
            ) : (
              <>🔄 手动刷新</>
            )}
          </Button>
        </div>
      </div>

      {/* 错误提示 */}
      {error && (
        <Alert variant="danger" dismissible onClose={() => setError(null)}>
          {error}
        </Alert>
      )}

      {/* 设备选择器 */}
      <Card className="card-custom mb-4">
        <Card.Header className="card-header-custom">
          <h5 className="mb-0">🎯 选择设备</h5>
        </Card.Header>
        <Card.Body className="card-body-custom">
          <Row>
            {devices.map((device) => (
              <Col md={6} lg={4} key={device.device_id} className="mb-3">
                <Card
                  className={`h-100 cursor-pointer ${
                    selectedDevice?.device_id === device.device_id ? 'border-primary' : ''
                  }`}
                  onClick={() => handleSelectDevice(device)}
                  style={{ cursor: 'pointer' }}
                >
                  <Card.Body>
                    <div className="d-flex justify-content-between align-items-start mb-2">
                      <strong>{device.device_name}</strong>
                      <Badge bg={device.status === 'online' ? 'success' : 'secondary'}>
                        {device.status === 'online' ? '在线' : '离线'}
                      </Badge>
                    </div>
                    <small className="text-muted d-block">
                      ID: <code>{device.device_id}</code>
                    </small>
                    <small className="text-muted d-block">
                      IP: <code>{device.local_ip}</code>
                    </small>
                    {selectedDevice?.device_id === device.device_id && (
                      <div className="mt-2">
                        <Badge bg="primary">当前选中</Badge>
                      </div>
                    )}
                  </Card.Body>
                </Card>
              </Col>
            ))}
          </Row>
        </Card.Body>
      </Card>

      {/* 设备状态显示 */}
      {selectedDevice && (
        <>
          {selectedDevice.status !== 'online' ? (
            <Alert variant="warning" className="card-custom">
              <Alert.Heading>⚠️ 设备离线</Alert.Heading>
              <p>设备 "{selectedDevice.device_name}" 当前处于离线状态，无法获取实时状态信息。</p>
            </Alert>
          ) : loading && !deviceStatus ? (
            <div className="text-center py-4">
              <Spinner animation="border" />
              <div className="mt-2">正在获取设备状态...</div>
            </div>
          ) : !deviceStatus ? (
            <Alert variant="warning" className="card-custom">
              <Alert.Heading>⚠️ 无法获取状态</Alert.Heading>
              <p>无法获取设备状态信息，请检查设备连接。</p>
            </Alert>
          ) : (
            <div className="data-grid">
              {/* 连接状态卡片 */}
              <Card className="card-custom">
                <Card.Header className="card-header-custom">
                  <h5 className="mb-0">🔗 连接状态</h5>
                </Card.Header>
                <Card.Body className="card-body-custom">
                  {getStatusBadge(deviceStatus.sbus_connected, 'SBUS遥控')}
                  {getStatusBadge(deviceStatus.can_connected, 'CAN总线')}
                  {getStatusBadge(deviceStatus.wifi_connected, 'Wi-Fi网络')}
                  
                  {deviceStatus.wifi_connected && (
                    <div className="mt-3 p-3 bg-light rounded">
                      <div className="row">
                        <div className="col-6">
                          <small className="text-muted d-block">IP地址</small>
                          <code className="text-primary">{deviceStatus.wifi_ip}</code>
                        </div>
                        <div className="col-6">
                          <small className="text-muted d-block">信号强度</small>
                          <div className="d-flex align-items-center">
                            <Badge bg={getSignalColor(deviceStatus.wifi_rssi)} className="me-2">
                              {deviceStatus.wifi_rssi} dBm
                            </Badge>
                          </div>
                        </div>
                      </div>
                    </div>
                  )}
                </Card.Body>
              </Card>

              {/* 系统资源卡片 */}
              <Card className="card-custom">
                <Card.Header className="card-header-custom">
                  <h5 className="mb-0">💾 系统资源</h5>
                </Card.Header>
                <Card.Body className="card-body-custom">
                  <div className="data-item mb-3">
                    <div className="data-label">可用内存</div>
                    <div className="data-value">
                      <div className="d-flex align-items-center">
                        <span className="me-2">{formatBytes(deviceStatus.free_heap)}</span>
                        <ProgressBar 
                          now={((deviceStatus.total_heap - deviceStatus.free_heap) / deviceStatus.total_heap) * 100}
                          style={{ width: '100px' }}
                          variant={deviceStatus.free_heap < 50000 ? 'danger' : 'success'}
                        />
                      </div>
                    </div>
                  </div>
                  
                  <div className="data-item mb-3">
                    <div className="data-label">运行时间</div>
                    <div className="data-value">{formatUptime(deviceStatus.uptime_seconds)}</div>
                  </div>
                  
                  <div className="data-item">
                    <div className="data-label">任务数量</div>
                    <div className="data-value">
                      <Badge bg="info">{deviceStatus.task_count} 个任务</Badge>
                    </div>
                  </div>
                </Card.Body>
              </Card>

              {/* SBUS数据卡片 */}
              {deviceStatus.sbus_connected && (
                <Card className="card-custom">
                  <Card.Header className="card-header-custom">
                    <h5 className="mb-0">🎮 SBUS通道数据</h5>
                  </Card.Header>
                  <Card.Body className="card-body-custom">
                    <Row>
                      {deviceStatus.sbus_channels.slice(0, 8).map((value, index) => (
                        <Col md={6} key={index} className="mb-2">
                          <div className="d-flex justify-content-between align-items-center">
                            <small>CH{index + 1}</small>
                            <div className="d-flex align-items-center">
                              <span className="me-2">{value}</span>
                              <ProgressBar 
                                now={(value / 2048) * 100}
                                style={{ width: '60px', height: '8px' }}
                                variant="primary"
                              />
                            </div>
                          </div>
                        </Col>
                      ))}
                    </Row>
                  </Card.Body>
                </Card>
              )}

              {/* CAN总线状态卡片 */}
              {deviceStatus.can_connected && (
                <Card className="card-custom">
                  <Card.Header className="card-header-custom">
                    <h5 className="mb-0">🚌 CAN总线状态</h5>
                  </Card.Header>
                  <Card.Body className="card-body-custom">
                    <div className="data-item mb-2">
                      <div className="data-label">发送计数</div>
                      <div className="data-value">
                        <Badge bg="success">{deviceStatus.can_tx_count}</Badge>
                      </div>
                    </div>
                    <div className="data-item">
                      <div className="data-label">接收计数</div>
                      <div className="data-value">
                        <Badge bg="info">{deviceStatus.can_rx_count}</Badge>
                      </div>
                    </div>
                  </Card.Body>
                </Card>
              )}
            </div>
          )}
        </>
      )}
    </div>
  )
}

export default CloudDeviceStatus
