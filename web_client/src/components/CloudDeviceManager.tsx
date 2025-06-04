import React, { useState, useEffect } from 'react'
import { Card, Row, Col, Badge, Spinner, Alert, Button, ListGroup, Modal, Form } from 'react-bootstrap'
import { cloudDeviceAPI, deviceManagementAPI, CloudDevice } from '../services/api'

const CloudDeviceManager: React.FC = () => {
  const [devices, setDevices] = useState<CloudDevice[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState<string | null>(null)
  const [selectedDevice, setSelectedDevice] = useState<CloudDevice | null>(null)
  const [showCommandModal, setShowCommandModal] = useState(false)
  const [commandForm, setCommandForm] = useState({
    command: '',
    data: '{}'
  })

  // 获取云设备列表
  const fetchCloudDevices = async () => {
    try {
      setLoading(true)
      setError(null)
      const cloudDevices = await cloudDeviceAPI.getRegisteredDevices()
      setDevices(cloudDevices)
      
      if (cloudDevices.length === 0) {
        setError('暂无注册的ESP32设备')
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : '获取云设备列表失败')
    } finally {
      setLoading(false)
    }
  }

  // 选择设备
  const handleSelectDevice = (device: CloudDevice) => {
    setSelectedDevice(device)
    setSuccess(`已选择设备: ${device.device_name}`)
  }

  // 测试设备连接
  const handleTestDevice = async (device: CloudDevice) => {
    try {
      setError(null)
      await deviceManagementAPI.testConnection(device.local_ip)
      setSuccess(`设备 "${device.device_name}" 连接正常`)

      // 刷新设备列表以更新状态
      await fetchCloudDevices()
    } catch (err) {
      setError(`设备 "${device.device_name}" 连接失败: ${err instanceof Error ? err.message : '未知错误'}`)
    }
  }

  // 发送指令
  const handleSendCommand = async () => {
    if (!selectedDevice) {
      setError('请先选择一个设备')
      return
    }

    try {
      setError(null)
      let commandData = {}
      
      if (commandForm.data.trim()) {
        try {
          commandData = JSON.parse(commandForm.data)
        } catch (e) {
          setError('指令数据格式错误，请输入有效的JSON')
          return
        }
      }

      await cloudDeviceAPI.sendCommand(selectedDevice.device_id, commandForm.command, commandData)
      setSuccess(`指令已发送到设备: ${selectedDevice.device_name}`)
      setShowCommandModal(false)
      setCommandForm({ command: '', data: '{}' })
    } catch (err) {
      setError(err instanceof Error ? err.message : '发送指令失败')
    }
  }

  // 格式化时间
  const formatTime = (timeString: string | null): string => {
    if (!timeString) return '从未'
    try {
      return new Date(timeString).toLocaleString('zh-CN')
    } catch {
      return '无效时间'
    }
  }

  // 获取状态徽章
  const getStatusBadge = (status: string) => {
    const variant = status === 'online' ? 'success' : 'secondary'
    const text = status === 'online' ? '在线' : '离线'
    return <Badge bg={variant}>{text}</Badge>
  }

  useEffect(() => {
    fetchCloudDevices()
    
    // 每30秒刷新一次设备状态
    const interval = setInterval(fetchCloudDevices, 30000)
    return () => clearInterval(interval)
  }, [])

  // 清除消息
  useEffect(() => {
    if (success || error) {
      const timer = setTimeout(() => {
        setSuccess(null)
        setError(null)
      }, 5000)
      return () => clearTimeout(timer)
    }
  }, [success, error])

  return (
    <div className="card-enter">
      {/* 页面标题和操作栏 */}
      <div className="d-flex justify-content-between align-items-center mb-4">
        <div>
          <h2 className="mb-1">☁️ 云设备管理</h2>
          <p className="text-muted mb-0">管理注册到云服务器的ESP32设备</p>
        </div>
        <div className="d-flex gap-2">
          <Button
            variant="outline-primary"
            onClick={fetchCloudDevices}
            disabled={loading}
            className="btn-custom"
          >
            {loading ? (
              <>
                <Spinner size="sm" className="me-2" />
                刷新中...
              </>
            ) : (
              <>🔄 刷新列表</>
            )}
          </Button>
          {selectedDevice && (
            <Button
              variant="primary"
              onClick={() => setShowCommandModal(true)}
              className="btn-custom"
            >
              📤 发送指令
            </Button>
          )}
        </div>
      </div>

      {/* 消息提示 */}
      {error && (
        <Alert variant="danger" dismissible onClose={() => setError(null)}>
          {error}
        </Alert>
      )}
      {success && (
        <Alert variant="success" dismissible onClose={() => setSuccess(null)}>
          {success}
        </Alert>
      )}

      {/* 当前选中设备 */}
      {selectedDevice && (
        <Card className="card-custom mb-4">
          <Card.Header className="card-header-custom">
            <h5 className="mb-0">🎯 当前选中设备</h5>
          </Card.Header>
          <Card.Body className="card-body-custom">
            <Row>
              <Col md={6}>
                <div className="data-item mb-2">
                  <div className="data-label">设备名称</div>
                  <div className="data-value">{selectedDevice.device_name}</div>
                </div>
                <div className="data-item mb-2">
                  <div className="data-label">设备ID</div>
                  <div className="data-value">
                    <code>{selectedDevice.device_id}</code>
                  </div>
                </div>
              </Col>
              <Col md={6}>
                <div className="data-item mb-2">
                  <div className="data-label">IP地址</div>
                  <div className="data-value">
                    <code>{selectedDevice.local_ip}</code>
                  </div>
                </div>
                <div className="data-item mb-2">
                  <div className="data-label">状态</div>
                  <div className="data-value">{getStatusBadge(selectedDevice.status)}</div>
                </div>
              </Col>
            </Row>
          </Card.Body>
        </Card>
      )}

      {/* 设备列表 */}
      <Card className="card-custom">
        <Card.Header className="card-header-custom">
          <h5 className="mb-0">📱 注册设备列表 ({devices.length})</h5>
        </Card.Header>
        <Card.Body className="card-body-custom">
          {loading ? (
            <div className="text-center py-4">
              <Spinner animation="border" />
              <div className="mt-2">正在加载设备列表...</div>
            </div>
          ) : devices.length === 0 ? (
            <Alert variant="info">
              <Alert.Heading>📭 暂无注册设备</Alert.Heading>
              <p>目前没有ESP32设备注册到云服务器。</p>
              <p className="mb-0">
                <small>💡 提示：ESP32设备需要调用 <code>/register-device</code> 接口进行注册</small>
              </p>
            </Alert>
          ) : (
            <ListGroup>
              {devices.map((device) => (
                <ListGroup.Item
                  key={device.device_id}
                  className={`d-flex justify-content-between align-items-center ${
                    selectedDevice?.device_id === device.device_id ? 'list-group-item-primary' : ''
                  }`}
                >
                  <div className="flex-grow-1">
                    <div className="d-flex align-items-center mb-2">
                      <strong className="me-2">{device.device_name}</strong>
                      {getStatusBadge(device.status)}
                    </div>
                    <div className="row">
                      <div className="col-md-6">
                        <small className="text-muted d-block">
                          设备ID: <code>{device.device_id}</code>
                        </small>
                        <small className="text-muted d-block">
                          IP地址: <code>{device.local_ip}</code>
                        </small>
                      </div>
                      <div className="col-md-6">
                        <small className="text-muted d-block">
                          注册时间: {formatTime(device.registered_at)}
                        </small>
                        <small className="text-muted d-block">
                          最后活跃: {formatTime(device.last_seen || null)}
                        </small>
                      </div>
                    </div>
                  </div>
                  <div className="d-flex gap-2">
                    <Button
                      variant="outline-success"
                      size="sm"
                      onClick={() => handleTestDevice(device)}
                      className="btn-custom"
                    >
                      🔍 测试
                    </Button>
                    <Button
                      variant={selectedDevice?.device_id === device.device_id ? "success" : "outline-primary"}
                      size="sm"
                      onClick={() => handleSelectDevice(device)}
                      className="btn-custom"
                    >
                      {selectedDevice?.device_id === device.device_id ? "✅ 已选中" : "🎯 选择"}
                    </Button>
                  </div>
                </ListGroup.Item>
              ))}
            </ListGroup>
          )}
        </Card.Body>
      </Card>

      {/* 发送指令模态框 */}
      <Modal show={showCommandModal} onHide={() => setShowCommandModal(false)} size="lg">
        <Modal.Header closeButton>
          <Modal.Title>📤 发送指令到设备</Modal.Title>
        </Modal.Header>
        <Modal.Body>
          {selectedDevice && (
            <Alert variant="info">
              <strong>目标设备:</strong> {selectedDevice.device_name} ({selectedDevice.device_id})
            </Alert>
          )}
          
          <Form>
            <Form.Group className="mb-3">
              <Form.Label>指令名称</Form.Label>
              <Form.Control
                type="text"
                value={commandForm.command}
                onChange={(e) => setCommandForm(prev => ({ ...prev, command: e.target.value }))}
                placeholder="例如: led_control, motor_speed, restart"
              />
            </Form.Group>
            
            <Form.Group className="mb-3">
              <Form.Label>指令数据 (JSON格式)</Form.Label>
              <Form.Control
                as="textarea"
                rows={4}
                value={commandForm.data}
                onChange={(e) => setCommandForm(prev => ({ ...prev, data: e.target.value }))}
                placeholder='例如: {"pin": 2, "state": true}'
              />
              <Form.Text className="text-muted">
                请输入有效的JSON格式数据，如果不需要数据可以输入 {}
              </Form.Text>
            </Form.Group>
          </Form>
        </Modal.Body>
        <Modal.Footer>
          <Button variant="secondary" onClick={() => setShowCommandModal(false)}>
            取消
          </Button>
          <Button 
            variant="primary" 
            onClick={handleSendCommand}
            disabled={!commandForm.command.trim()}
          >
            📤 发送指令
          </Button>
        </Modal.Footer>
      </Modal>
    </div>
  )
}

export default CloudDeviceManager
