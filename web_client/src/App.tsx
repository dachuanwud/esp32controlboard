import React, { useState } from 'react'
import { Routes, Route, useLocation, Link } from 'react-router-dom'
import { Container, Navbar, Nav, Dropdown, Button } from 'react-bootstrap'
import { DeviceProvider, useDevice } from './contexts/DeviceContext'
import DeviceInfo from './components/DeviceInfo'
import DeviceStatus from './components/DeviceStatus'
import OTAUpdate from './components/OTAUpdate'
import WiFiSettings from './components/WiFiSettings'
import DeviceManager from './components/DeviceManager'
import CloudDeviceManager from './components/CloudDeviceManager'
import CloudDeviceStatus from './components/CloudDeviceStatus'

// 主应用内容组件
const AppContent: React.FC = () => {
  const location = useLocation()
  const [showDeviceManager, setShowDeviceManager] = useState(false)

  const {
    devices,
    selectedDevice,
    addDevice,
    updateDevice,
    deleteDevice,
    selectDevice,
    refreshDevices,
    loading
  } = useDevice()

  const getPageTitle = (pathname: string): string => {
    switch (pathname) {
      case '/': return '📊 设备信息'
      case '/status': return '📈 实时状态'
      case '/ota': return '🔄 OTA更新'
      case '/wifi': return '📶 Wi-Fi设置'
      case '/devices': return '☁️ 云设备管理'
      case '/cloud-status': return '📈 云设备状态'
      default: return '🎛️ ESP32控制板'
    }
  }

  const isActiveRoute = (path: string): boolean => {
    return location.pathname === path
  }

  return (
    <div className="App page-enter">
      {/* 现代化导航栏 */}
      <Navbar expand="lg" className="navbar-custom" fixed="top">
        <Container fluid className="px-4">
          <Navbar.Brand as={Link} to="/" className="navbar-brand-custom">
            🎛️ ESP32控制板 Web上位机
          </Navbar.Brand>

          {/* 设备选择器 */}
          <Dropdown className="me-auto ms-4">
            <Dropdown.Toggle
              variant="outline-light"
              size="sm"
              className="d-flex align-items-center"
              disabled={loading}
            >
              {selectedDevice ? (
                <>
                  <span className={`status-indicator ${selectedDevice.status === 'online' ? 'status-connected' : 'status-disconnected'}`}></span>
                  {selectedDevice.name}
                </>
              ) : (
                <>
                  <span className="status-indicator status-disconnected"></span>
                  未选择设备
                </>
              )}
            </Dropdown.Toggle>
            <Dropdown.Menu>
              <Dropdown.Header>选择设备</Dropdown.Header>
              {devices.length === 0 ? (
                <Dropdown.ItemText className="text-muted">
                  暂无设备，请先添加设备
                </Dropdown.ItemText>
              ) : (
                devices.map(device => (
                  <Dropdown.Item
                    key={device.id}
                    active={device.id === selectedDevice?.id}
                    onClick={() => selectDevice(device)}
                  >
                    <span className={`status-indicator ${device.status === 'online' ? 'status-connected' : 'status-disconnected'}`}></span>
                    {device.name}
                    <small className="text-muted d-block">{device.ip}</small>
                  </Dropdown.Item>
                ))
              )}
              <Dropdown.Divider />
              <Dropdown.Item onClick={() => setShowDeviceManager(true)}>
                🔧 设备管理
              </Dropdown.Item>
              {devices.length > 0 && (
                <Dropdown.Item onClick={refreshDevices} disabled={loading}>
                  🔄 刷新状态
                </Dropdown.Item>
              )}
            </Dropdown.Menu>
          </Dropdown>

          <Navbar.Toggle aria-controls="basic-navbar-nav" />
          <Navbar.Collapse id="basic-navbar-nav">
            <Nav className="ms-auto">
              <Nav.Link
                as={Link}
                to="/"
                className={`nav-link-custom ${isActiveRoute('/') ? 'active' : ''}`}
              >
                📊 设备信息
              </Nav.Link>
              <Nav.Link
                as={Link}
                to="/status"
                className={`nav-link-custom ${isActiveRoute('/status') ? 'active' : ''}`}
              >
                📈 实时状态
              </Nav.Link>
              <Nav.Link
                as={Link}
                to="/ota"
                className={`nav-link-custom ${isActiveRoute('/ota') ? 'active' : ''}`}
              >
                🔄 OTA更新
              </Nav.Link>
              <Nav.Link
                as={Link}
                to="/wifi"
                className={`nav-link-custom ${isActiveRoute('/wifi') ? 'active' : ''}`}
              >
                📶 Wi-Fi设置
              </Nav.Link>
              <Nav.Link
                as={Link}
                to="/devices"
                className={`nav-link-custom ${isActiveRoute('/devices') ? 'active' : ''}`}
              >
                ☁️ 云设备管理
              </Nav.Link>
              <Nav.Link
                as={Link}
                to="/cloud-status"
                className={`nav-link-custom ${isActiveRoute('/cloud-status') ? 'active' : ''}`}
              >
                📈 云设备状态
              </Nav.Link>
            </Nav>
          </Navbar.Collapse>
        </Container>
      </Navbar>

      {/* 主内容区域 */}
      <div className="main-container" style={{ marginTop: '80px' }}>
        {/* 页面标题 */}
        <div className="page-header">
          <h1 className="page-title">{getPageTitle(location.pathname)}</h1>
          <div className="d-flex align-items-center">
            {selectedDevice ? (
              <>
                <span className="text-muted me-3">当前设备:</span>
                <span className="fw-bold">{selectedDevice.name}</span>
                <span className={`status-indicator ms-2 ${selectedDevice.status === 'online' ? 'status-connected' : 'status-disconnected'}`}></span>
              </>
            ) : (
              <>
                <span className="text-warning me-2">⚠️</span>
                <span className="text-muted">请先添加并选择ESP32设备</span>
                <Button
                  variant="outline-primary"
                  size="sm"
                  className="ms-3"
                  onClick={() => setShowDeviceManager(true)}
                >
                  🔧 添加设备
                </Button>
              </>
            )}
          </div>
        </div>

        {/* 路由内容 */}
        <Routes>
          {/* 云设备管理路由 - 不需要本地设备选择 */}
          <Route path="/devices" element={<CloudDeviceManager />} />
          <Route path="/cloud-status" element={<CloudDeviceStatus />} />

          {/* 本地设备管理路由 - 需要设备选择 */}
          {selectedDevice ? (
            <>
              <Route path="/" element={<DeviceInfo />} />
              <Route path="/status" element={<DeviceStatus />} />
              <Route path="/ota" element={<OTAUpdate />} />
              <Route path="/wifi" element={<WiFiSettings />} />
            </>
          ) : (
            <Route path="/" element={
              <div className="text-center py-5">
                <div className="card-custom p-5">
                  <h3 className="text-muted mb-4">🎛️ 欢迎使用ESP32控制板Web上位机</h3>
                  <p className="text-secondary mb-4">
                    请先添加您的ESP32设备以开始使用。您可以通过以下方式添加设备：
                  </p>
                  <div className="row justify-content-center">
                    <div className="col-md-8">
                      <div className="d-grid gap-3">
                        <Button
                          variant="primary"
                          size="lg"
                          onClick={() => setShowDeviceManager(true)}
                          className="btn-custom"
                        >
                          🔧 打开设备管理
                        </Button>
                        <div className="text-muted">
                          <small>
                            💡 提示：确保您的ESP32设备已连接到网络，并且可以通过IP地址访问
                          </small>
                        </div>
                      </div>
                    </div>
                  </div>
                  <hr className="my-4" />
                  <div className="text-center">
                    <p className="text-muted mb-3">或者查看云服务器注册的设备：</p>
                    <div className="d-flex gap-2 justify-content-center">
                      <Link to="/devices" className="btn btn-outline-info btn-custom">
                        ☁️ 云设备管理
                      </Link>
                      <Link to="/cloud-status" className="btn btn-outline-success btn-custom">
                        📈 云设备状态
                      </Link>
                    </div>
                  </div>
                </div>
              </div>
            } />
          )}
        </Routes>

        {/* 设备管理对话框 */}
        <DeviceManager
          show={showDeviceManager}
          onHide={() => setShowDeviceManager(false)}
          onDeviceAdded={addDevice}
          onDeviceUpdated={updateDevice}
          onDeviceDeleted={deleteDevice}
          devices={devices}
        />
      </div>
    </div>
  )
}

// 主应用组件（包装DeviceProvider）
function App() {
  return (
    <DeviceProvider>
      <AppContent />
    </DeviceProvider>
  )
}

export default App
