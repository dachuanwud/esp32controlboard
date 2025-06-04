import React, { useState, useEffect, useRef } from 'react'
import { Card, Alert, ProgressBar, Button, Modal } from 'react-bootstrap'
import { otaAPI, OTAProgress } from '../services/api'

const OTAUpdate: React.FC = () => {
  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const [uploading, setUploading] = useState(false)
  const [uploadProgress, setUploadProgress] = useState(0)
  const [otaProgress, setOtaProgress] = useState<OTAProgress | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState(false)
  const [showConfirmModal, setShowConfirmModal] = useState(false)
  const fileInputRef = useRef<HTMLInputElement>(null)

  const fetchOTAProgress = async () => {
    try {
      const progress = await otaAPI.getProgress()
      setOtaProgress(progress)
    } catch (err) {
      console.error('Failed to fetch OTA progress:', err)
    }
  }

  useEffect(() => {
    fetchOTAProgress()
    // 每秒检查OTA进度
    const interval = setInterval(fetchOTAProgress, 1000)
    return () => clearInterval(interval)
  }, [])

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0]
    if (file) {
      // 检查文件类型
      if (!file.name.endsWith('.bin')) {
        setError('请选择.bin格式的固件文件')
        return
      }
      
      // 检查文件大小 (最大1MB)
      if (file.size > 1024 * 1024) {
        setError('固件文件大小不能超过1MB')
        return
      }

      setSelectedFile(file)
      setError(null)
      setSuccess(false)
    }
  }

  const handleDrop = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault()
    const file = event.dataTransfer.files[0]
    if (file) {
      // 模拟文件输入事件
      const fakeEvent = {
        target: { files: [file] }
      } as unknown as React.ChangeEvent<HTMLInputElement>
      handleFileSelect(fakeEvent)
    }
  }

  const handleDragOver = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault()
  }

  const startUpload = () => {
    if (!selectedFile) {
      setError('请先选择固件文件')
      return
    }
    setShowConfirmModal(true)
  }

  const confirmUpload = async () => {
    if (!selectedFile) return

    setShowConfirmModal(false)
    setUploading(true)
    setUploadProgress(0)
    setError(null)
    setSuccess(false)

    try {
      await otaAPI.uploadFirmware(selectedFile, (progress) => {
        setUploadProgress(progress)
      })
      
      setSuccess(true)
      setSelectedFile(null)
      if (fileInputRef.current) {
        fileInputRef.current.value = ''
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : '固件上传失败')
    } finally {
      setUploading(false)
    }
  }

  const handleRollback = async () => {
    try {
      await otaAPI.rollback()
      setSuccess(true)
    } catch (err) {
      setError(err instanceof Error ? err.message : '固件回滚失败')
    }
  }

  const formatFileSize = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes'
    const k = 1024
    const sizes = ['Bytes', 'KB', 'MB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  return (
    <div>
      <div className="d-flex justify-content-between align-items-center mb-4">
        <h2>🔄 OTA固件更新</h2>
        <Button variant="outline-warning" onClick={handleRollback} disabled={uploading}>
          ↩️ 回滚到上一版本
        </Button>
      </div>

      {error && (
        <Alert variant="danger" dismissible onClose={() => setError(null)}>
          <Alert.Heading>❌ 错误</Alert.Heading>
          <p>{error}</p>
        </Alert>
      )}

      {success && (
        <Alert variant="success" dismissible onClose={() => setSuccess(false)}>
          <Alert.Heading>✅ 成功</Alert.Heading>
          <p>操作完成！设备将在几秒钟后重启并应用新固件。</p>
        </Alert>
      )}

      <Card className="mb-4">
        <Card.Header>
          <h5 className="mb-0">📦 固件上传</h5>
        </Card.Header>
        <Card.Body>
          <div
            className={`upload-area ${selectedFile ? 'border-success' : ''}`}
            onDrop={handleDrop}
            onDragOver={handleDragOver}
            onClick={() => fileInputRef.current?.click()}
          >
            <input
              ref={fileInputRef}
              type="file"
              accept=".bin"
              onChange={handleFileSelect}
              style={{ display: 'none' }}
              disabled={uploading}
            />
            
            {selectedFile ? (
              <div>
                <h5 className="text-success">✅ 已选择文件</h5>
                <p><strong>文件名:</strong> {selectedFile.name}</p>
                <p><strong>大小:</strong> {formatFileSize(selectedFile.size)}</p>
                <p className="text-muted">点击重新选择文件</p>
              </div>
            ) : (
              <div>
                <h5>📁 选择固件文件</h5>
                <p>点击此处选择.bin格式的固件文件，或拖拽文件到此区域</p>
                <p className="text-muted">支持的格式: .bin | 最大大小: 1MB</p>
              </div>
            )}
          </div>

          {selectedFile && (
            <div className="mt-3 text-center">
              <Button
                variant="primary"
                size="lg"
                onClick={startUpload}
                disabled={uploading}
              >
                {uploading ? '上传中...' : '🚀 开始更新'}
              </Button>
            </div>
          )}

          {uploading && (
            <div className="mt-3">
              <h6>上传进度:</h6>
              <ProgressBar
                now={uploadProgress}
                label={`${uploadProgress}%`}
                animated
                striped
              />
            </div>
          )}
        </Card.Body>
      </Card>

      {otaProgress && (
        <Card>
          <Card.Header>
            <h5 className="mb-0">📊 OTA状态</h5>
          </Card.Header>
          <Card.Body>
            <div className="mb-3">
              <strong>状态:</strong> 
              <span className={`ms-2 ${otaProgress.in_progress ? 'text-warning' : otaProgress.success ? 'text-success' : 'text-muted'}`}>
                {otaProgress.status_message}
              </span>
            </div>

            {otaProgress.in_progress && (
              <div className="mb-3">
                <h6>更新进度:</h6>
                <ProgressBar
                  now={otaProgress.progress_percent}
                  label={`${otaProgress.progress_percent}%`}
                  animated
                  striped
                />
                <small className="text-muted">
                  {formatFileSize(otaProgress.written_size)} / {formatFileSize(otaProgress.total_size)}
                </small>
              </div>
            )}

            {otaProgress.error_message && (
              <Alert variant="danger" className="mt-3">
                <strong>错误详情:</strong> {otaProgress.error_message}
              </Alert>
            )}
          </Card.Body>
        </Card>
      )}

      {/* 确认对话框 */}
      <Modal show={showConfirmModal} onHide={() => setShowConfirmModal(false)}>
        <Modal.Header closeButton>
          <Modal.Title>⚠️ 确认固件更新</Modal.Title>
        </Modal.Header>
        <Modal.Body>
          <p>您即将上传并安装新的固件:</p>
          <ul>
            <li><strong>文件名:</strong> {selectedFile?.name}</li>
            <li><strong>大小:</strong> {selectedFile ? formatFileSize(selectedFile.size) : ''}</li>
          </ul>
          <Alert variant="warning">
            <strong>注意:</strong> 固件更新过程中请勿断电或关闭设备，否则可能导致设备损坏！
          </Alert>
        </Modal.Body>
        <Modal.Footer>
          <Button variant="secondary" onClick={() => setShowConfirmModal(false)}>
            取消
          </Button>
          <Button variant="danger" onClick={confirmUpload}>
            确认更新
          </Button>
        </Modal.Footer>
      </Modal>
    </div>
  )
}

export default OTAUpdate
