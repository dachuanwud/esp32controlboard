import React, { useState, useEffect } from 'react'
import { Card, Form, Button, Alert, Badge, Spinner, ListGroup } from 'react-bootstrap'
import { wifiAPI, WiFiStatus } from '../services/api'

const WiFiSettings: React.FC = () => {
  const [wifiStatus, setWifiStatus] = useState<WiFiStatus | null>(null)
  const [networks, setNetworks] = useState<any[]>([])
  const [ssid, setSsid] = useState('')
  const [password, setPassword] = useState('')

  const [scanning, setScanning] = useState(false)
  const [connecting, setConnecting] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState<string | null>(null)

  const fetchWiFiStatus = async () => {
    try {
      const status = await wifiAPI.getStatus()
      setWifiStatus(status)
    } catch (err) {
      console.error('Failed to fetch Wi-Fi status:', err)
    }
  }

  const scanNetworks = async () => {
    try {
      setScanning(true)
      setError(null)
      const scannedNetworks = await wifiAPI.scan()
      setNetworks(scannedNetworks)
    } catch (err) {
      setError(err instanceof Error ? err.message : '扫描Wi-Fi网络失败')
    } finally {
      setScanning(false)
    }
  }

  const connectToWiFi = async (e: React.FormEvent) => {
    e.preventDefault()
    
    if (!ssid.trim()) {
      setError('请输入Wi-Fi网络名称')
      return
    }

    try {
      setConnecting(true)
      setError(null)
      setSuccess(null)
      
      await wifiAPI.connect(ssid.trim(), password)
      setSuccess(`成功连接到Wi-Fi网络: ${ssid}`)
      setPassword('')
      
      // 延迟获取状态，等待连接完成
      setTimeout(fetchWiFiStatus, 2000)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Wi-Fi连接失败')
    } finally {
      setConnecting(false)
    }
  }

  const selectNetwork = (networkSsid: string) => {
    setSsid(networkSsid)
    setPassword('')
  }

  const getStatusBadge = (state: string) => {
    const statusMap = {
      'connected': { variant: 'success', text: '已连接' },
      'connecting': { variant: 'warning', text: '连接中' },
      'disconnected': { variant: 'secondary', text: '未连接' },
      'failed': { variant: 'danger', text: '连接失败' }
    }
    
    const status = statusMap[state as keyof typeof statusMap] || { variant: 'secondary', text: '未知' }
    
    return (
      <Badge bg={status.variant}>
        <span className={`status-indicator ${state === 'connected' ? 'status-connected' : state === 'connecting' ? 'status-connecting' : 'status-disconnected'}`}></span>
        {status.text}
      </Badge>
    )
  }

  const getSignalStrength = (rssi: number): string => {
    if (rssi >= -50) return '优秀'
    if (rssi >= -60) return '良好'
    if (rssi >= -70) return '一般'
    return '较差'
  }

  const getSignalBars = (rssi: number): string => {
    if (rssi >= -50) return '📶'
    if (rssi >= -60) return '📶'
    if (rssi >= -70) return '📶'
    return '📶'
  }

  useEffect(() => {
    fetchWiFiStatus()
    // 每10秒刷新一次Wi-Fi状态
    const interval = setInterval(fetchWiFiStatus, 10000)
    return () => clearInterval(interval)
  }, [])

  return (
    <div>
      <div className="d-flex justify-content-between align-items-center mb-4">
        <h2>📶 Wi-Fi设置</h2>
        <div>
          <Button variant="outline-primary" onClick={fetchWiFiStatus} className="me-2">
            🔄 刷新状态
          </Button>
          <Button variant="outline-info" onClick={scanNetworks} disabled={scanning}>
            {scanning ? <Spinner size="sm" /> : '🔍'} 扫描网络
          </Button>
        </div>
      </div>

      {error && (
        <Alert variant="danger" dismissible onClose={() => setError(null)}>
          <Alert.Heading>❌ 错误</Alert.Heading>
          <p>{error}</p>
        </Alert>
      )}

      {success && (
        <Alert variant="success" dismissible onClose={() => setSuccess(null)}>
          <Alert.Heading>✅ 成功</Alert.Heading>
          <p>{success}</p>
        </Alert>
      )}

      <Card className="mb-4">
        <Card.Header>
          <h5 className="mb-0">📊 当前状态</h5>
        </Card.Header>
        <Card.Body>
          {wifiStatus ? (
            <div>
              <div className="mb-3">
                <strong>连接状态:</strong> {getStatusBadge(wifiStatus.state)}
              </div>
              
              {wifiStatus.state === 'connected' && (
                <>
                  <div className="mb-2">
                    <strong>IP地址:</strong> <code>{wifiStatus.ip_address}</code>
                  </div>
                  <div className="mb-2">
                    <strong>信号强度:</strong> 
                    <span className="ms-2">
                      {getSignalBars(wifiStatus.rssi)} {wifiStatus.rssi} dBm ({getSignalStrength(wifiStatus.rssi)})
                    </span>
                  </div>
                  <div className="mb-2">
                    <strong>连接时间:</strong> {new Date(wifiStatus.connect_time).toLocaleString()}
                  </div>
                </>
              )}
              
              {wifiStatus.retry_count > 0 && (
                <div className="mb-2">
                  <strong>重试次数:</strong> {wifiStatus.retry_count}
                </div>
              )}
            </div>
          ) : (
            <div className="text-center">
              <Spinner animation="border" size="sm" />
              <span className="ms-2">获取状态中...</span>
            </div>
          )}
        </Card.Body>
      </Card>

      <Card className="mb-4">
        <Card.Header>
          <h5 className="mb-0">🔗 连接到Wi-Fi</h5>
        </Card.Header>
        <Card.Body>
          <Form onSubmit={connectToWiFi}>
            <Form.Group className="mb-3">
              <Form.Label>网络名称 (SSID)</Form.Label>
              <Form.Control
                type="text"
                value={ssid}
                onChange={(e) => setSsid(e.target.value)}
                placeholder="输入Wi-Fi网络名称"
                disabled={connecting}
              />
            </Form.Group>
            
            <Form.Group className="mb-3">
              <Form.Label>密码</Form.Label>
              <Form.Control
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="输入Wi-Fi密码"
                disabled={connecting}
              />
            </Form.Group>
            
            <Button type="submit" variant="primary" disabled={connecting || !ssid.trim()}>
              {connecting ? (
                <>
                  <Spinner size="sm" className="me-2" />
                  连接中...
                </>
              ) : (
                '🔗 连接'
              )}
            </Button>
          </Form>
        </Card.Body>
      </Card>

      {networks.length > 0 && (
        <Card>
          <Card.Header>
            <h5 className="mb-0">📡 可用网络</h5>
          </Card.Header>
          <Card.Body>
            <ListGroup variant="flush">
              {networks.map((network, index) => (
                <ListGroup.Item
                  key={index}
                  action
                  onClick={() => selectNetwork(network.ssid)}
                  className="d-flex justify-content-between align-items-center"
                >
                  <div>
                    <strong>{network.ssid}</strong>
                    {network.authmode !== 'WIFI_AUTH_OPEN' && (
                      <Badge bg="secondary" className="ms-2">🔒</Badge>
                    )}
                  </div>
                  <div>
                    <span className="me-2">
                      {getSignalBars(network.rssi)} {network.rssi} dBm
                    </span>
                    <Badge bg="info">{network.authmode}</Badge>
                  </div>
                </ListGroup.Item>
              ))}
            </ListGroup>
            
            {networks.length === 0 && (
              <div className="text-center text-muted">
                <p>未发现可用的Wi-Fi网络</p>
                <Button variant="outline-secondary" onClick={scanNetworks}>
                  重新扫描
                </Button>
              </div>
            )}
          </Card.Body>
        </Card>
      )}
    </div>
  )
}

export default WiFiSettings
