import React, { useState, useEffect } from 'react'
import { Modal, Button, Form, Alert, Spinner, ListGroup, Badge, ProgressBar } from 'react-bootstrap'
import { Device, DeviceDiscoveryResult, deviceManagementAPI } from '../services/api'
import DeviceStorageService from '../services/deviceStorage'

interface DeviceManagerProps {
  show: boolean
  onHide: () => void
  onDeviceAdded: (device: Device) => void
  onDeviceUpdated: (device: Device) => void
  onDeviceDeleted: (deviceId: string) => void
  devices: Device[]
}

const DeviceManager: React.FC<DeviceManagerProps> = ({
  show,
  onHide,
  onDeviceAdded,
  onDeviceUpdated,
  onDeviceDeleted,
  devices
}) => {
  const [activeTab, setActiveTab] = useState<'manual' | 'discover' | 'manage'>('manual')
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState<string | null>(null)

  // 手动添加设备状态
  const [manualForm, setManualForm] = useState({
    name: '',
    ip: '',
    testing: false
  })

  // 设备发现状态
  const [discovery, setDiscovery] = useState({
    scanning: false,
    progress: 0,
    results: [] as DeviceDiscoveryResult[],
    ipRange: '192.168.1'
  })

  // 设备编辑状态
  const [editingDevice, setEditingDevice] = useState<Device | null>(null)
  const [editForm, setEditForm] = useState({
    name: '',
    ip: ''
  })

  useEffect(() => {
    if (show) {
      setError(null)
      setSuccess(null)
      setDiscovery(prev => ({ ...prev, results: [] }))
    }
  }, [show])

  // 手动添加设备
  const handleManualAdd = async () => {
    if (!manualForm.name.trim() || !manualForm.ip.trim()) {
      setError('请填写设备名称和IP地址')
      return
    }

    if (!DeviceStorageService.isValidIP(manualForm.ip)) {
      setError('请输入有效的IP地址')
      return
    }

    if (DeviceStorageService.deviceExists(manualForm.ip)) {
      setError('该IP地址的设备已存在')
      return
    }

    setManualForm(prev => ({ ...prev, testing: true }))
    setError(null)

    try {
      // 测试连接
      const deviceInfo = await deviceManagementAPI.testConnection(manualForm.ip)
      
      // 创建新设备
      const newDevice: Device = {
        id: DeviceStorageService.generateDeviceId(manualForm.ip, manualForm.name),
        name: manualForm.name.trim(),
        ip: manualForm.ip.trim(),
        status: 'online',
        lastSeen: Date.now(),
        deviceInfo
      }

      // 保存到存储
      DeviceStorageService.addDevice(newDevice)
      onDeviceAdded(newDevice)

      setSuccess(`设备 "${newDevice.name}" 添加成功！`)
      setManualForm({ name: '', ip: '', testing: false })
    } catch (err) {
      setError(err instanceof Error ? err.message : '无法连接到设备，请检查IP地址和网络连接')
    } finally {
      setManualForm(prev => ({ ...prev, testing: false }))
    }
  }

  // 快速发现设备
  const handleQuickDiscover = async () => {
    setDiscovery(prev => ({ ...prev, scanning: true, progress: 0, results: [] }))
    setError(null)

    try {
      const results = await deviceManagementAPI.quickDiscoverDevices()
      setDiscovery(prev => ({ ...prev, results, progress: 100 }))
      
      if (results.length === 0) {
        setError('未发现任何ESP32设备，请检查网络连接或尝试手动添加')
      } else {
        setSuccess(`发现 ${results.length} 个ESP32设备`)
      }
    } catch (err) {
      setError('设备发现失败，请检查网络连接')
    } finally {
      setDiscovery(prev => ({ ...prev, scanning: false }))
    }
  }

  // 全网段扫描
  const handleFullScan = async () => {
    setDiscovery(prev => ({ ...prev, scanning: true, progress: 0, results: [] }))
    setError(null)

    try {
      // 模拟进度更新
      const progressInterval = setInterval(() => {
        setDiscovery(prev => ({
          ...prev,
          progress: Math.min(prev.progress + 2, 95)
        }))
      }, 100)

      const results = await deviceManagementAPI.discoverDevices(discovery.ipRange)
      
      clearInterval(progressInterval)
      setDiscovery(prev => ({ ...prev, results, progress: 100 }))
      
      if (results.length === 0) {
        setError(`在 ${discovery.ipRange}.x 网段未发现任何ESP32设备`)
      } else {
        setSuccess(`在 ${discovery.ipRange}.x 网段发现 ${results.length} 个ESP32设备`)
      }
    } catch (err) {
      setError('网段扫描失败，请检查网络连接')
    } finally {
      setDiscovery(prev => ({ ...prev, scanning: false }))
    }
  }

  // 添加发现的设备
  const handleAddDiscoveredDevice = (result: DeviceDiscoveryResult) => {
    if (DeviceStorageService.deviceExists(result.ip)) {
      setError('该设备已存在')
      return
    }

    const deviceName = result.deviceInfo?.device_name || `ESP32-${result.ip.split('.').pop()}`
    const newDevice: Device = {
      id: DeviceStorageService.generateDeviceId(result.ip, deviceName),
      name: deviceName,
      ip: result.ip,
      status: 'online',
      lastSeen: Date.now(),
      deviceInfo: result.deviceInfo
    }

    DeviceStorageService.addDevice(newDevice)
    onDeviceAdded(newDevice)
    setSuccess(`设备 "${newDevice.name}" 添加成功！`)
  }

  // 开始编辑设备
  const handleEditDevice = (device: Device) => {
    setEditingDevice(device)
    setEditForm({
      name: device.name,
      ip: device.ip
    })
  }

  // 保存设备编辑
  const handleSaveEdit = async () => {
    if (!editingDevice || !editForm.name.trim() || !editForm.ip.trim()) {
      setError('请填写设备名称和IP地址')
      return
    }

    if (!DeviceStorageService.isValidIP(editForm.ip)) {
      setError('请输入有效的IP地址')
      return
    }

    // 检查IP是否被其他设备使用
    const existingDevice = DeviceStorageService.findDeviceByIp(editForm.ip)
    if (existingDevice && existingDevice.id !== editingDevice.id) {
      setError('该IP地址已被其他设备使用')
      return
    }

    setLoading(true)
    setError(null)

    try {
      // 如果IP地址改变了，测试新的连接
      if (editForm.ip !== editingDevice.ip) {
        await deviceManagementAPI.testConnection(editForm.ip)
      }

      const updatedDevice: Device = {
        ...editingDevice,
        name: editForm.name.trim(),
        ip: editForm.ip.trim(),
        lastSeen: Date.now()
      }

      DeviceStorageService.updateDevice(editingDevice.id, updatedDevice)
      onDeviceUpdated(updatedDevice)
      
      setEditingDevice(null)
      setSuccess(`设备 "${updatedDevice.name}" 更新成功！`)
    } catch (err) {
      setError(err instanceof Error ? err.message : '无法连接到设备，请检查IP地址')
    } finally {
      setLoading(false)
    }
  }

  // 删除设备
  const handleDeleteDevice = (device: Device) => {
    const confirmMessage = `确定要删除设备 "${device.name}" 吗？\n\n此操作将：\n• 从本地存储中移除设备记录\n• 清除设备的所有配置信息\n• 此操作不可撤销\n\n设备IP: ${device.ip}\n最后连接: ${device.lastSeen ? new Date(device.lastSeen).toLocaleString() : '从未连接'}`

    if (window.confirm(confirmMessage)) {
      try {
        DeviceStorageService.removeDevice(device.id)
        onDeviceDeleted(device.id)
        setSuccess(`设备 "${device.name}" 已成功删除`)
        setError(null)
      } catch (err) {
        setError(`删除设备失败: ${err instanceof Error ? err.message : '未知错误'}`)
      }
    }
  }

  // 测试设备连接
  const handleTestDevice = async (device: Device) => {
    setLoading(true)
    setError(null)

    try {
      await deviceManagementAPI.testConnection(device.ip)
      
      // 更新设备状态
      const updatedDevice = { ...device, status: 'online' as const, lastSeen: Date.now() }
      DeviceStorageService.updateDevice(device.id, updatedDevice)
      onDeviceUpdated(updatedDevice)
      
      setSuccess(`设备 "${device.name}" 连接正常`)
    } catch (err) {
      // 更新设备状态为离线
      const updatedDevice = { ...device, status: 'offline' as const }
      DeviceStorageService.updateDevice(device.id, updatedDevice)
      onDeviceUpdated(updatedDevice)
      
      setError(`设备 "${device.name}" 连接失败: ${err instanceof Error ? err.message : '未知错误'}`)
    } finally {
      setLoading(false)
    }
  }

  return (
    <Modal show={show} onHide={onHide} size="lg" centered>
      <Modal.Header closeButton>
        <Modal.Title>🔧 设备管理</Modal.Title>
      </Modal.Header>
      
      <Modal.Body>
        {/* 标签页导航 */}
        <div className="d-flex mb-4 border-bottom">
          <Button
            variant={activeTab === 'manual' ? 'primary' : 'outline-secondary'}
            className="me-2 mb-2"
            onClick={() => setActiveTab('manual')}
          >
            ➕ 手动添加
          </Button>
          <Button
            variant={activeTab === 'discover' ? 'primary' : 'outline-secondary'}
            className="me-2 mb-2"
            onClick={() => setActiveTab('discover')}
          >
            🔍 设备发现
          </Button>
          <Button
            variant={activeTab === 'manage' ? 'primary' : 'outline-secondary'}
            className="mb-2"
            onClick={() => setActiveTab('manage')}
          >
            ⚙️ 设备管理
          </Button>
        </div>

        {/* 错误和成功提示 */}
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

        {/* 手动添加设备 */}
        {activeTab === 'manual' && (
          <div>
            <h6 className="mb-3">📝 手动添加ESP32设备</h6>
            <Form>
              <Form.Group className="mb-3">
                <Form.Label>设备名称</Form.Label>
                <Form.Control
                  type="text"
                  placeholder="例如: ESP32控制板-001"
                  value={manualForm.name}
                  onChange={(e) => setManualForm(prev => ({ ...prev, name: e.target.value }))}
                  disabled={manualForm.testing}
                />
              </Form.Group>
              
              <Form.Group className="mb-3">
                <Form.Label>IP地址</Form.Label>
                <Form.Control
                  type="text"
                  placeholder="例如: 192.168.1.100"
                  value={manualForm.ip}
                  onChange={(e) => setManualForm(prev => ({ ...prev, ip: e.target.value }))}
                  disabled={manualForm.testing}
                />
                <Form.Text className="text-muted">
                  请输入ESP32设备的IP地址，系统将自动测试连接
                </Form.Text>
              </Form.Group>
              
              <Button
                variant="primary"
                onClick={handleManualAdd}
                disabled={manualForm.testing || !manualForm.name.trim() || !manualForm.ip.trim()}
                className="btn-custom"
              >
                {manualForm.testing ? (
                  <>
                    <Spinner size="sm" className="me-2" />
                    测试连接中...
                  </>
                ) : (
                  '➕ 添加设备'
                )}
              </Button>
            </Form>
          </div>
        )}

        {/* 设备发现 */}
        {activeTab === 'discover' && (
          <div>
            <h6 className="mb-3">🔍 自动发现ESP32设备</h6>
            
            <div className="mb-3">
              <Button
                variant="success"
                onClick={handleQuickDiscover}
                disabled={discovery.scanning}
                className="me-2 btn-custom"
              >
                {discovery.scanning ? (
                  <>
                    <Spinner size="sm" className="me-2" />
                    快速扫描中...
                  </>
                ) : (
                  '⚡ 快速发现'
                )}
              </Button>
              
              <Button
                variant="info"
                onClick={handleFullScan}
                disabled={discovery.scanning}
                className="btn-custom"
              >
                {discovery.scanning ? (
                  <>
                    <Spinner size="sm" className="me-2" />
                    全网段扫描中...
                  </>
                ) : (
                  '🌐 全网段扫描'
                )}
              </Button>
            </div>

            <Form.Group className="mb-3">
              <Form.Label>IP网段</Form.Label>
              <Form.Control
                type="text"
                placeholder="例如: 192.168.1"
                value={discovery.ipRange}
                onChange={(e) => setDiscovery(prev => ({ ...prev, ipRange: e.target.value }))}
                disabled={discovery.scanning}
              />
              <Form.Text className="text-muted">
                全网段扫描将扫描该网段下的所有IP地址 (1-254)
              </Form.Text>
            </Form.Group>

            {discovery.scanning && (
              <div className="mb-3">
                <ProgressBar
                  now={discovery.progress}
                  label={`${discovery.progress}%`}
                  animated
                  striped
                  className="progress-custom"
                />
              </div>
            )}

            {discovery.results.length > 0 && (
              <div>
                <h6 className="mb-3">发现的设备 ({discovery.results.length})</h6>
                <ListGroup>
                  {discovery.results.map((result, index) => (
                    <ListGroup.Item
                      key={index}
                      className="d-flex justify-content-between align-items-center"
                    >
                      <div>
                        <strong>{result.deviceInfo?.device_name || `ESP32-${result.ip.split('.').pop()}`}</strong>
                        <br />
                        <small className="text-muted">
                          IP: {result.ip} | 响应时间: {result.responseTime}ms
                          {result.deviceInfo && (
                            <> | 固件: {result.deviceInfo.firmware_version}</>
                          )}
                        </small>
                      </div>
                      <Button
                        variant="outline-primary"
                        size="sm"
                        onClick={() => handleAddDiscoveredDevice(result)}
                        disabled={DeviceStorageService.deviceExists(result.ip)}
                        className="btn-custom"
                      >
                        {DeviceStorageService.deviceExists(result.ip) ? '已添加' : '➕ 添加'}
                      </Button>
                    </ListGroup.Item>
                  ))}
                </ListGroup>
              </div>
            )}
          </div>
        )}

        {/* 设备管理 */}
        {activeTab === 'manage' && (
          <div>
            <h6 className="mb-3">⚙️ 已添加的设备 ({devices.length})</h6>
            
            {devices.length === 0 ? (
              <Alert variant="info">
                暂无设备，请先添加ESP32设备
              </Alert>
            ) : (
              <ListGroup>
                {devices.map((device) => (
                  <ListGroup.Item key={device.id}>
                    {editingDevice?.id === device.id ? (
                      // 编辑模式
                      <div>
                        <Form.Group className="mb-2">
                          <Form.Control
                            type="text"
                            value={editForm.name}
                            onChange={(e) => setEditForm(prev => ({ ...prev, name: e.target.value }))}
                            placeholder="设备名称"
                          />
                        </Form.Group>
                        <Form.Group className="mb-3">
                          <Form.Control
                            type="text"
                            value={editForm.ip}
                            onChange={(e) => setEditForm(prev => ({ ...prev, ip: e.target.value }))}
                            placeholder="IP地址"
                          />
                        </Form.Group>
                        <div>
                          <Button
                            variant="success"
                            size="sm"
                            onClick={handleSaveEdit}
                            disabled={loading}
                            className="me-2"
                          >
                            {loading ? <Spinner size="sm" /> : '💾 保存'}
                          </Button>
                          <Button
                            variant="secondary"
                            size="sm"
                            onClick={() => setEditingDevice(null)}
                          >
                            ❌ 取消
                          </Button>
                        </div>
                      </div>
                    ) : (
                      // 显示模式
                      <div className="d-flex justify-content-between align-items-center">
                        <div>
                          <div className="d-flex align-items-center mb-1">
                            <span className={`status-indicator ${device.status === 'online' ? 'status-connected' : 'status-disconnected'} me-2`}></span>
                            <strong>{device.name}</strong>
                            <Badge bg={device.status === 'online' ? 'success' : 'danger'} className="ms-2">
                              {device.status === 'online' ? '在线' : '离线'}
                            </Badge>
                          </div>
                          <small className="text-muted">
                            IP: {device.ip}
                            {device.lastSeen && (
                              <> | 最后连接: {new Date(device.lastSeen).toLocaleString()}</>
                            )}
                          </small>
                        </div>
                        <div>
                          <Button
                            variant="outline-info"
                            size="sm"
                            onClick={() => handleTestDevice(device)}
                            disabled={loading}
                            className="me-1"
                          >
                            🔍 测试
                          </Button>
                          <Button
                            variant="outline-primary"
                            size="sm"
                            onClick={() => handleEditDevice(device)}
                            className="me-1"
                          >
                            ✏️ 编辑
                          </Button>
                          <Button
                            variant="outline-danger"
                            size="sm"
                            onClick={() => handleDeleteDevice(device)}
                          >
                            🗑️ 删除
                          </Button>
                        </div>
                      </div>
                    )}
                  </ListGroup.Item>
                ))}
              </ListGroup>
            )}
          </div>
        )}
      </Modal.Body>
      
      <Modal.Footer>
        <Button variant="secondary" onClick={onHide}>
          关闭
        </Button>
      </Modal.Footer>
    </Modal>
  )
}

export default DeviceManager
