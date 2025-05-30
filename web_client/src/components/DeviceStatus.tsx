import React, { useState, useEffect } from 'react'
import { Card, Row, Col, Badge, Spinner, Alert, ProgressBar, Button, Form } from 'react-bootstrap'
import { deviceAPI, DeviceStatus as DeviceStatusType, deviceManagementAPI } from '../services/api'
import { useDeviceAPI } from '../contexts/DeviceContext'

const DeviceStatus: React.FC = () => {
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatusType | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [autoRefresh, setAutoRefresh] = useState(true)
  const [refreshing, setRefreshing] = useState(false)

  const { selectedDevice, apiInstance, isConnected } = useDeviceAPI()

  const fetchDeviceStatus = async (isManualRefresh = false) => {
    if (!selectedDevice || !apiInstance) {
      setError('未选择设备或设备未连接')
      setLoading(false)
      return
    }

    try {
      if (isManualRefresh) {
        setRefreshing(true)
      }
      setError(null)

      // 使用设备管理API获取状态
      const status = await deviceManagementAPI.getDeviceStatus(selectedDevice.ip)
      setDeviceStatus(status)
    } catch (err) {
      setError(err instanceof Error ? err.message : '获取设备状态失败')
    } finally {
      setLoading(false)
      setRefreshing(false)
    }
  }

  useEffect(() => {
    if (selectedDevice && isConnected) {
      fetchDeviceStatus()

      if (autoRefresh) {
        // 每2秒刷新一次状态
        const interval = setInterval(() => fetchDeviceStatus(), 2000)
        return () => clearInterval(interval)
      }
    } else {
      setDeviceStatus(null)
      setLoading(false)
      setError(selectedDevice ? '设备未连接' : '未选择设备')
    }
  }, [selectedDevice, isConnected, autoRefresh])

  const getStatusBadge = (connected: boolean, label: string) => {
    return (
      <div className="d-flex align-items-center justify-content-between p-3 border rounded mb-2"
           style={{ backgroundColor: connected ? 'rgba(5, 150, 105, 0.1)' : 'rgba(220, 38, 38, 0.1)' }}>
        <div className="d-flex align-items-center">
          <span className={`status-indicator ${connected ? 'status-connected' : 'status-disconnected'} me-2`}></span>
          <strong>{label}</strong>
        </div>
        <Badge bg={connected ? 'success' : 'danger'} className="badge-custom">
          {connected ? '已连接' : '未连接'}
        </Badge>
      </div>
    )
  }

  const getChannelValue = (value: number): string => {
    if (value >= 1050 && value <= 1950) {
      const percentage = Math.round(((value - 1050) / 900) * 100)
      return `${value} (${percentage}%)`
    }
    return value.toString()
  }

  const getMotorSpeedColor = (speed: number): string => {
    if (speed === 0) return 'secondary'
    if (Math.abs(speed) < 30) return 'info'
    if (Math.abs(speed) < 70) return 'warning'
    return 'danger'
  }

  const formatTimestamp = (timestamp: number): string => {
    if (timestamp === 0) {
      return '从未更新'
    }

    // ESP32返回的是系统滴答计数转换的毫秒数
    // 将其转换为相对时间显示
    const seconds = Math.floor(timestamp / 1000)
    const hours = Math.floor(seconds / 3600)
    const minutes = Math.floor((seconds % 3600) / 60)
    const remainingSeconds = seconds % 60

    if (hours > 0) {
      return `${hours}小时${minutes}分${remainingSeconds}秒前`
    } else if (minutes > 0) {
      return `${minutes}分${remainingSeconds}秒前`
    } else {
      return `${remainingSeconds}秒前`
    }
  }

  if (loading) {
    return (
      <div className="text-center py-5">
        <div className="loading-shimmer" style={{ width: '100%', height: '400px', borderRadius: '12px' }}>
          <div className="d-flex flex-column justify-content-center align-items-center h-100">
            <Spinner animation="border" variant="primary" style={{ width: '3rem', height: '3rem' }} />
            <p className="mt-3 text-muted">正在获取实时状态...</p>
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
        <Button variant="outline-danger" onClick={() => fetchDeviceStatus()} className="btn-custom">
          🔄 重新连接
        </Button>
      </Alert>
    )
  }

  if (!deviceStatus) {
    return (
      <Alert variant="warning" className="card-custom">
        <Alert.Heading>⚠️ 数据异常</Alert.Heading>
        <p>未能获取到设备状态，请检查设备连接</p>
      </Alert>
    )
  }

  return (
    <div className="card-enter">
      {/* 控制栏 */}
      <div className="d-flex justify-content-between align-items-center mb-4">
        <div className="d-flex align-items-center">
          <span className="status-indicator status-connected me-2"></span>
          <span className="text-muted">
            实时监控 •
            {autoRefresh ? ' 自动刷新中' : ' 已暂停'} •
            最后更新: {new Date().toLocaleTimeString()}
          </span>
        </div>
        <div className="d-flex align-items-center gap-3">
          <Form.Check
            type="switch"
            id="auto-refresh-switch"
            label="自动刷新"
            checked={autoRefresh}
            onChange={(e) => setAutoRefresh(e.target.checked)}
            className="mb-0"
          />
          <Button
            variant="outline-primary"
            onClick={() => fetchDeviceStatus(true)}
            disabled={refreshing}
            className="btn-custom"
          >
            {refreshing ? (
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

      {/* 状态监控网格 */}
      <div className="data-grid">
        {/* 连接状态卡片 */}
        <Card className="card-custom">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">🔗 连接状态</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            {getStatusBadge(deviceStatus.sbus_connected, 'SBUS遥控')}
            {getStatusBadge(deviceStatus.can_connected, 'CAN总线')}
            <div>
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
                      <span className="fw-bold">{deviceStatus.wifi_rssi} dBm</span>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </Card.Body>
        </Card>

        {/* 电机状态卡片 */}
        <Card className="card-custom">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">🚗 电机状态</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            <div className="motor-status">
              <div className="motor-info">
                <div className="motor-name">左电机</div>
                <div className="motor-speed text-primary">
                  {deviceStatus.motor_left_speed > 0 ? '+' : ''}{deviceStatus.motor_left_speed}%
                </div>
              </div>
              <div className="d-flex flex-column align-items-end">
                <Badge bg={getMotorSpeedColor(deviceStatus.motor_left_speed)} className="badge-custom mb-2">
                  {Math.abs(deviceStatus.motor_left_speed)}%
                </Badge>
                <ProgressBar
                  now={Math.abs(deviceStatus.motor_left_speed)}
                  variant={getMotorSpeedColor(deviceStatus.motor_left_speed)}
                  className="progress-custom"
                  style={{ width: '120px', height: '8px' }}
                />
              </div>
            </div>

            <div className="motor-status">
              <div className="motor-info">
                <div className="motor-name">右电机</div>
                <div className="motor-speed text-primary">
                  {deviceStatus.motor_right_speed > 0 ? '+' : ''}{deviceStatus.motor_right_speed}%
                </div>
              </div>
              <div className="d-flex flex-column align-items-end">
                <Badge bg={getMotorSpeedColor(deviceStatus.motor_right_speed)} className="badge-custom mb-2">
                  {Math.abs(deviceStatus.motor_right_speed)}%
                </Badge>
                <ProgressBar
                  now={Math.abs(deviceStatus.motor_right_speed)}
                  variant={getMotorSpeedColor(deviceStatus.motor_right_speed)}
                  className="progress-custom"
                  style={{ width: '120px', height: '8px' }}
                />
              </div>
            </div>
          </Card.Body>
        </Card>
      </div>

      {/* SBUS通道数据 */}
      {deviceStatus.sbus_connected && (
        <Card className="card-custom">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">📡 SBUS通道数据</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            <div className="channel-grid">
              {deviceStatus.sbus_channels.slice(0, 8).map((value, index) => (
                <div key={index} className="channel-item">
                  <div className="channel-number">通道 {index + 1}</div>
                  <div className="channel-value">{getChannelValue(value)}</div>
                  <ProgressBar
                    now={Math.max(0, Math.min(100, ((value - 1050) / 900) * 100))}
                    variant="info"
                    className="progress-custom mt-2"
                    style={{ height: '6px' }}
                  />
                </div>
              ))}
            </div>

            {deviceStatus.sbus_channels.length > 8 && (
              <details className="mt-4">
                <summary className="btn btn-outline-secondary btn-sm">
                  显示更多通道 (9-16)
                </summary>
                <div className="channel-grid mt-3">
                  {deviceStatus.sbus_channels.slice(8, 16).map((value, index) => (
                    <div key={index + 8} className="channel-item">
                      <div className="channel-number">通道 {index + 9}</div>
                      <div className="channel-value">{getChannelValue(value)}</div>
                      <ProgressBar
                        now={Math.max(0, Math.min(100, ((value - 1050) / 900) * 100))}
                        variant="info"
                        className="progress-custom mt-2"
                        style={{ height: '6px' }}
                      />
                    </div>
                  ))}
                </div>
              </details>
            )}
          </Card.Body>
        </Card>
      )}

      {/* 时间信息卡片 */}
      <Card className="card-custom">
        <Card.Header className="card-header-custom">
          <h5 className="mb-0">⏱️ 时间信息</h5>
        </Card.Header>
        <Card.Body className="card-body-custom">
          <div className="row">
            <div className="col-md-6">
              <div className="data-item">
                <div className="data-label">最后SBUS更新</div>
                <div className="data-value">
                  <span className="me-2">📡</span>
                  {formatTimestamp(deviceStatus.last_sbus_time)}
                </div>
              </div>
            </div>
            <div className="col-md-6">
              <div className="data-item">
                <div className="data-label">最后命令更新</div>
                <div className="data-value">
                  <span className="me-2">⚡</span>
                  {formatTimestamp(deviceStatus.last_cmd_time)}
                </div>
              </div>
            </div>
          </div>
        </Card.Body>
      </Card>
    </div>
  )
}

export default DeviceStatus
