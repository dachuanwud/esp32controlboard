import React, { useState, useEffect, useRef } from 'react'
import { Card, Button, Table, Modal, Form, Alert, ProgressBar, Badge, Spinner } from 'react-bootstrap'
import { otaAPI, cloudDeviceAPI, CloudDevice } from '../services/api'

interface Firmware {
  id: string
  filename: string
  original_name: string
  version: string
  description: string
  device_type: string
  file_size: number
  upload_time: string
  status: string
}

interface Deployment {
  id: string
  deployment_name: string
  firmware_version: string
  firmware_description: string
  status: string
  total_devices: number
  completed_devices: number
  failed_devices: number
  completion_percentage: number
  created_at: string
  started_at?: string
  completed_at?: string
  duration_seconds?: number
}

const OTAManager: React.FC = () => {
  const [firmwareList, setFirmwareList] = useState<Firmware[]>([])
  const [deployments, setDeployments] = useState<Deployment[]>([])
  const [devices, setDevices] = useState<CloudDevice[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState<string | null>(null)

  // 上传固件相关状态
  const [showUploadModal, setShowUploadModal] = useState(false)
  const [uploadFile, setUploadFile] = useState<File | null>(null)
  const [uploadProgress, setUploadProgress] = useState(0)
  const [uploading, setUploading] = useState(false)
  const fileInputRef = useRef<HTMLInputElement>(null)

  // 部署固件相关状态
  const [showDeployModal, setShowDeployModal] = useState(false)
  const [selectedFirmware, setSelectedFirmware] = useState<Firmware | null>(null)
  const [selectedDevices, setSelectedDevices] = useState<string[]>([])
  const [deploymentName, setDeploymentName] = useState('')

  useEffect(() => {
    loadData()
    const interval = setInterval(loadData, 10000) // 每10秒刷新一次
    return () => clearInterval(interval)
  }, [])

  const loadData = async () => {
    try {
      const [firmwareResult, devicesResult] = await Promise.all([
        otaAPI.getFirmwareList(),
        cloudDeviceAPI.getOnlineDevices()
      ])
      
      setFirmwareList(firmwareResult.firmware || [])
      setDevices(devicesResult)
      
      // 加载部署历史
      const deploymentsResult = await otaAPI.getDeploymentHistory()
      setDeployments(deploymentsResult.deployments || [])
    } catch (err) {
      console.error('加载数据失败:', err)
    }
  }

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0]
    if (file) {
      if (file.name.endsWith('.bin')) {
        setUploadFile(file)
        setError(null)
      } else {
        setError('请选择.bin格式的固件文件')
        setUploadFile(null)
      }
    }
  }

  const handleUploadFirmware = async (formData: { version: string; description: string; deviceType: string }) => {
    if (!uploadFile) {
      setError('请选择固件文件')
      return
    }

    setUploading(true)
    setUploadProgress(0)
    setError(null)

    try {
      await otaAPI.uploadFirmware(uploadFile, formData, (progress) => {
        setUploadProgress(progress)
      })

      setSuccess('固件上传成功')
      setShowUploadModal(false)
      setUploadFile(null)
      if (fileInputRef.current) {
        fileInputRef.current.value = ''
      }
      await loadData()
    } catch (err) {
      setError(err instanceof Error ? err.message : '固件上传失败')
    } finally {
      setUploading(false)
    }
  }

  const handleDeployFirmware = async () => {
    if (!selectedFirmware || selectedDevices.length === 0) {
      setError('请选择固件和目标设备')
      return
    }

    setLoading(true)
    setError(null)

    try {
      await otaAPI.deployFirmware({
        firmwareId: selectedFirmware.id,
        deviceIds: selectedDevices,
        deploymentName: deploymentName || `部署_${selectedFirmware.version}_${new Date().toLocaleString()}`
      })

      setSuccess('固件部署已启动')
      setShowDeployModal(false)
      setSelectedFirmware(null)
      setSelectedDevices([])
      setDeploymentName('')
      await loadData()
    } catch (err) {
      setError(err instanceof Error ? err.message : '固件部署失败')
    } finally {
      setLoading(false)
    }
  }

  const handleDeleteFirmware = async (firmwareId: string) => {
    if (!confirm('确定要删除这个固件吗？')) {
      return
    }

    try {
      await otaAPI.deleteFirmware(firmwareId)
      setSuccess('固件删除成功')
      await loadData()
    } catch (err) {
      setError(err instanceof Error ? err.message : '固件删除失败')
    }
  }

  const formatFileSize = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes'
    const k = 1024
    const sizes = ['Bytes', 'KB', 'MB', 'GB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  const getStatusBadge = (status: string) => {
    const statusMap = {
      'pending': { variant: 'secondary', text: '等待中' },
      'in_progress': { variant: 'warning', text: '进行中' },
      'completed': { variant: 'success', text: '已完成' },
      'failed': { variant: 'danger', text: '失败' },
      'partial': { variant: 'warning', text: '部分成功' },
      'available': { variant: 'success', text: '可用' }
    }
    const config = statusMap[status as keyof typeof statusMap] || { variant: 'secondary', text: status }
    return <Badge bg={config.variant}>{config.text}</Badge>
  }

  return (
    <div className="ota-manager">
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

      {/* 固件管理 */}
      <Card className="mb-4">
        <Card.Header className="d-flex justify-content-between align-items-center">
          <h5 className="mb-0">📦 固件管理</h5>
          <Button variant="primary" onClick={() => setShowUploadModal(true)}>
            上传固件
          </Button>
        </Card.Header>
        <Card.Body>
          <Table responsive striped hover>
            <thead>
              <tr>
                <th>版本</th>
                <th>文件名</th>
                <th>设备类型</th>
                <th>大小</th>
                <th>上传时间</th>
                <th>状态</th>
                <th>操作</th>
              </tr>
            </thead>
            <tbody>
              {firmwareList.map((firmware) => (
                <tr key={firmware.id}>
                  <td><strong>{firmware.version}</strong></td>
                  <td>{firmware.original_name}</td>
                  <td>{firmware.device_type}</td>
                  <td>{formatFileSize(firmware.file_size)}</td>
                  <td>{new Date(firmware.upload_time).toLocaleString()}</td>
                  <td>{getStatusBadge(firmware.status)}</td>
                  <td>
                    <Button
                      variant="success"
                      size="sm"
                      className="me-2"
                      onClick={() => {
                        setSelectedFirmware(firmware)
                        setShowDeployModal(true)
                      }}
                    >
                      部署
                    </Button>
                    <Button
                      variant="danger"
                      size="sm"
                      onClick={() => handleDeleteFirmware(firmware.id)}
                    >
                      删除
                    </Button>
                  </td>
                </tr>
              ))}
            </tbody>
          </Table>
          {firmwareList.length === 0 && (
            <div className="text-center text-muted py-4">
              暂无固件文件
            </div>
          )}
        </Card.Body>
      </Card>

      {/* 部署历史 */}
      <Card>
        <Card.Header>
          <h5 className="mb-0">🚀 部署历史</h5>
        </Card.Header>
        <Card.Body>
          <Table responsive striped hover>
            <thead>
              <tr>
                <th>部署名称</th>
                <th>固件版本</th>
                <th>状态</th>
                <th>进度</th>
                <th>设备数量</th>
                <th>创建时间</th>
                <th>耗时</th>
              </tr>
            </thead>
            <tbody>
              {deployments.map((deployment) => (
                <tr key={deployment.id}>
                  <td>{deployment.deployment_name}</td>
                  <td>{deployment.firmware_version}</td>
                  <td>{getStatusBadge(deployment.status)}</td>
                  <td>
                    <ProgressBar
                      now={deployment.completion_percentage}
                      label={`${deployment.completion_percentage}%`}
                      style={{ minWidth: '100px' }}
                    />
                  </td>
                  <td>
                    {deployment.completed_devices}/{deployment.total_devices}
                    {deployment.failed_devices > 0 && (
                      <span className="text-danger"> ({deployment.failed_devices}失败)</span>
                    )}
                  </td>
                  <td>{new Date(deployment.created_at).toLocaleString()}</td>
                  <td>
                    {deployment.duration_seconds 
                      ? `${Math.round(deployment.duration_seconds)}秒`
                      : deployment.status === 'in_progress' ? '进行中...' : '-'
                    }
                  </td>
                </tr>
              ))}
            </tbody>
          </Table>
          {deployments.length === 0 && (
            <div className="text-center text-muted py-4">
              暂无部署记录
            </div>
          )}
        </Card.Body>
      </Card>

      {/* 上传固件模态框 */}
      <Modal show={showUploadModal} onHide={() => setShowUploadModal(false)} size="lg">
        <Modal.Header closeButton>
          <Modal.Title>上传固件</Modal.Title>
        </Modal.Header>
        <Modal.Body>
          <UploadFirmwareForm
            file={uploadFile}
            onFileSelect={handleFileSelect}
            onSubmit={handleUploadFirmware}
            uploading={uploading}
            uploadProgress={uploadProgress}
            fileInputRef={fileInputRef}
          />
        </Modal.Body>
      </Modal>

      {/* 部署固件模态框 */}
      <Modal show={showDeployModal} onHide={() => setShowDeployModal(false)} size="lg">
        <Modal.Header closeButton>
          <Modal.Title>部署固件</Modal.Title>
        </Modal.Header>
        <Modal.Body>
          <DeployFirmwareForm
            firmware={selectedFirmware}
            devices={devices}
            selectedDevices={selectedDevices}
            onDeviceSelectionChange={setSelectedDevices}
            deploymentName={deploymentName}
            onDeploymentNameChange={setDeploymentName}
            onSubmit={handleDeployFirmware}
            loading={loading}
          />
        </Modal.Body>
      </Modal>
    </div>
  )
}

// 上传固件表单组件
interface UploadFirmwareFormProps {
  file: File | null
  onFileSelect: (event: React.ChangeEvent<HTMLInputElement>) => void
  onSubmit: (formData: { version: string; description: string; deviceType: string }) => void
  uploading: boolean
  uploadProgress: number
  fileInputRef: React.RefObject<HTMLInputElement>
}

const UploadFirmwareForm: React.FC<UploadFirmwareFormProps> = ({
  file, onFileSelect, onSubmit, uploading, uploadProgress, fileInputRef
}) => {
  const [version, setVersion] = useState('')
  const [description, setDescription] = useState('')
  const [deviceType, setDeviceType] = useState('ESP32')

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    if (!file || !version) return
    onSubmit({ version, description, deviceType })
  }

  return (
    <Form onSubmit={handleSubmit}>
      <Form.Group className="mb-3">
        <Form.Label>固件文件</Form.Label>
        <Form.Control
          ref={fileInputRef}
          type="file"
          accept=".bin"
          onChange={onFileSelect}
          disabled={uploading}
        />
        {file && (
          <Form.Text className="text-success">
            已选择: {file.name} ({(file.size / 1024 / 1024).toFixed(2)} MB)
          </Form.Text>
        )}
      </Form.Group>

      <Form.Group className="mb-3">
        <Form.Label>版本号 *</Form.Label>
        <Form.Control
          type="text"
          value={version}
          onChange={(e) => setVersion(e.target.value)}
          placeholder="例如: 1.0.0"
          required
          disabled={uploading}
        />
      </Form.Group>

      <Form.Group className="mb-3">
        <Form.Label>设备类型</Form.Label>
        <Form.Select
          value={deviceType}
          onChange={(e) => setDeviceType(e.target.value)}
          disabled={uploading}
        >
          <option value="ESP32">ESP32</option>
          <option value="ESP32-S2">ESP32-S2</option>
          <option value="ESP32-S3">ESP32-S3</option>
          <option value="ESP32-C3">ESP32-C3</option>
        </Form.Select>
      </Form.Group>

      <Form.Group className="mb-3">
        <Form.Label>描述</Form.Label>
        <Form.Control
          as="textarea"
          rows={3}
          value={description}
          onChange={(e) => setDescription(e.target.value)}
          placeholder="固件更新说明..."
          disabled={uploading}
        />
      </Form.Group>

      {uploading && (
        <div className="mb-3">
          <ProgressBar
            now={uploadProgress}
            label={`${uploadProgress}%`}
            animated
            striped
          />
        </div>
      )}

      <div className="d-flex justify-content-end">
        <Button
          type="submit"
          variant="primary"
          disabled={!file || !version || uploading}
        >
          {uploading ? (
            <>
              <Spinner size="sm" className="me-2" />
              上传中...
            </>
          ) : (
            '上传固件'
          )}
        </Button>
      </div>
    </Form>
  )
}

// 部署固件表单组件
interface DeployFirmwareFormProps {
  firmware: Firmware | null
  devices: CloudDevice[]
  selectedDevices: string[]
  onDeviceSelectionChange: (deviceIds: string[]) => void
  deploymentName: string
  onDeploymentNameChange: (name: string) => void
  onSubmit: () => void
  loading: boolean
}

const DeployFirmwareForm: React.FC<DeployFirmwareFormProps> = ({
  firmware, devices, selectedDevices, onDeviceSelectionChange,
  deploymentName, onDeploymentNameChange, onSubmit, loading
}) => {
  const handleDeviceToggle = (deviceId: string) => {
    if (selectedDevices.includes(deviceId)) {
      onDeviceSelectionChange(selectedDevices.filter(id => id !== deviceId))
    } else {
      onDeviceSelectionChange([...selectedDevices, deviceId])
    }
  }

  const handleSelectAll = () => {
    if (selectedDevices.length === devices.length) {
      onDeviceSelectionChange([])
    } else {
      onDeviceSelectionChange(devices.map(d => d.device_id))
    }
  }

  return (
    <div>
      {firmware && (
        <Alert variant="info">
          <strong>固件信息:</strong> {firmware.version} - {firmware.original_name}
          <br />
          <strong>描述:</strong> {firmware.description || '无描述'}
        </Alert>
      )}

      <Form.Group className="mb-3">
        <Form.Label>部署名称</Form.Label>
        <Form.Control
          type="text"
          value={deploymentName}
          onChange={(e) => onDeploymentNameChange(e.target.value)}
          placeholder={`部署_${firmware?.version}_${new Date().toLocaleString()}`}
        />
      </Form.Group>

      <Form.Group className="mb-3">
        <div className="d-flex justify-content-between align-items-center mb-2">
          <Form.Label>选择目标设备 ({selectedDevices.length}/{devices.length})</Form.Label>
          <Button variant="outline-secondary" size="sm" onClick={handleSelectAll}>
            {selectedDevices.length === devices.length ? '取消全选' : '全选'}
          </Button>
        </div>
        
        <div style={{ maxHeight: '300px', overflowY: 'auto', border: '1px solid #dee2e6', borderRadius: '0.375rem', padding: '0.5rem' }}>
          {devices.map((device) => (
            <Form.Check
              key={device.device_id}
              type="checkbox"
              id={`device-${device.device_id}`}
              label={
                <div>
                  <strong>{device.device_name}</strong> ({device.device_id})
                  <br />
                  <small className="text-muted">
                    IP: {device.local_ip} | 固件: {device.firmware_version || '未知'}
                  </small>
                </div>
              }
              checked={selectedDevices.includes(device.device_id)}
              onChange={() => handleDeviceToggle(device.device_id)}
              className="mb-2"
            />
          ))}
        </div>
        
        {devices.length === 0 && (
          <div className="text-center text-muted py-3">
            没有在线设备
          </div>
        )}
      </Form.Group>

      <div className="d-flex justify-content-end">
        <Button
          variant="success"
          onClick={onSubmit}
          disabled={!firmware || selectedDevices.length === 0 || loading}
        >
          {loading ? (
            <>
              <Spinner size="sm" className="me-2" />
              部署中...
            </>
          ) : (
            `开始部署 (${selectedDevices.length}个设备)`
          )}
        </Button>
      </div>
    </div>
  )
}

export default OTAManager
