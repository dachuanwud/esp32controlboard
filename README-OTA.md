# 🎛️ ESP32控制板Web OTA系统

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.4.1-blue)](https://github.com/espressif/esp-idf)
[![React](https://img.shields.io/badge/React-18.2.0-61dafb)](https://reactjs.org/)
[![TypeScript](https://img.shields.io/badge/TypeScript-5.2.2-blue)](https://www.typescriptlang.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## 📋 项目简介

ESP32控制板Web OTA系统是一个完整的固件无线更新解决方案，在保持原有SBUS通信、CAN总线控制等核心功能的基础上，新增了Web上位机功能。系统采用ESP-IDF框架和FreeRTOS实时操作系统，配合React+TypeScript前端界面，提供了专业的设备管理和OTA更新体验。

## ✨ 主要特性

### 🔧 ESP32端功能
- **🌐 Wi-Fi连接管理** - Station模式连接，自动重连机制
- **🖥️ HTTP服务器** - RESTful API接口，支持CORS跨域
- **🔄 OTA固件更新** - 双分区机制，自动回滚保护
- **📡 SBUS通信** - 遥控信号接收和解析 (保持原有功能)
- **🚗 CAN总线控制** - 电机驱动控制 (保持原有功能)
- **⚡ FreeRTOS架构** - 多任务并发，实时响应

### 🌐 Web前端功能
- **📱 设备信息展示** - 硬件配置、固件版本、系统资源
- **📊 实时状态监控** - SBUS数据、电机状态、连接状态
- **🔄 OTA固件更新** - 拖拽上传、进度显示、错误处理
- **📶 Wi-Fi网络配置** - 网络扫描、连接管理、状态监控
- **📱 响应式设计** - 支持桌面和移动设备访问

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32控制板OTA系统                        │
├─────────────────────────────────────────────────────────────┤
│  Web前端     │  React + TypeScript + Bootstrap              │
│             │  设备监控 │ OTA更新 │ Wi-Fi配置 │ 实时状态    │
├─────────────────────────────────────────────────────────────┤
│  HTTP API   │  RESTful API接口                              │
│             │  /api/device/* │ /api/ota/* │ /api/wifi/*    │
├─────────────────────────────────────────────────────────────┤
│  ESP32端    │  HTTP服务器 │ OTA管理器 │ Wi-Fi管理器        │
│             │  现有功能: SBUS │ CAN总线 │ 电机控制         │
├─────────────────────────────────────────────────────────────┤
│  FreeRTOS   │  任务调度 │ 队列通信 │ 内存管理             │
├─────────────────────────────────────────────────────────────┤
│  硬件层     │  ESP32芯片 │ Wi-Fi模块 │ Flash存储          │
└─────────────────────────────────────────────────────────────┘
```

## 📁 项目结构

```
esp32controlboard/
├── main/                          # ESP32主程序
│   ├── main.c                     # 主程序入口
│   ├── wifi_manager.c/.h          # Wi-Fi管理模块
│   ├── http_server.c/.h           # HTTP服务器模块
│   ├── ota_manager.c/.h           # OTA管理模块
│   ├── sbus.c/.h                  # SBUS通信模块 (原有)
│   ├── channel_parse.c/.h         # 通道解析模块 (原有)
│   └── drv_keyadouble.c/.h        # 电机控制模块 (原有)
├── web_client/                    # Web前端项目
│   ├── src/
│   │   ├── components/            # React组件
│   │   │   ├── DeviceInfo.tsx     # 设备信息组件
│   │   │   ├── DeviceStatus.tsx   # 设备状态组件
│   │   │   ├── OTAUpdate.tsx      # OTA更新组件
│   │   │   └── WiFiSettings.tsx   # Wi-Fi设置组件
│   │   ├── services/
│   │   │   └── api.ts             # API接口封装
│   │   ├── App.tsx                # 主应用组件
│   │   └── main.tsx               # 应用入口
│   ├── package.json               # 依赖配置
│   └── vite.config.ts             # 构建配置
├── docs/                          # 技术文档
│   ├── ota-system.md              # 系统技术文档
│   ├── api-specification.md       # API接口规范
│   └── deployment-guide.md        # 部署指南
├── build_only.bat                 # ESP32构建脚本
├── flash_com10.bat                # 固件烧录脚本
├── build_web.bat                  # Web前端构建脚本
├── build_all_ota.bat              # 完整构建脚本
└── README-OTA.md                  # 项目说明文档
```

## 🚀 快速开始

### 环境要求

- **ESP-IDF**: v5.4.1
- **Node.js**: 18.x 或更高版本
- **Python**: 3.8+ (ESP-IDF依赖)
- **硬件**: ESP32开发板，支持Wi-Fi功能

### 一键构建

```bash
# Windows环境
build_all_ota.bat

# 该脚本将自动完成：
# 1. ESP32固件编译
# 2. Web前端构建
# 3. 显示构建结果和使用说明
```

### 分步操作

#### 1. 构建ESP32固件

```bash
# 设置ESP-IDF环境
export.bat  # Windows
# 或 . export.sh  # Linux/macOS

# 编译固件
idf.py build
# 或使用脚本: build_only.bat
```

#### 2. 烧录固件

```bash
# 烧录到ESP32 (COM10端口)
idf.py -p COM10 flash monitor
# 或使用脚本: flash_com10.bat
```

#### 3. 构建Web前端

```bash
cd web_client
npm install
npm run build
# 或使用脚本: build_web.bat
```

#### 4. 配置Wi-Fi

修改 `main/main.c` 中的Wi-Fi配置：

```c
#define DEFAULT_WIFI_SSID     "您的WiFi名称"
#define DEFAULT_WIFI_PASSWORD "您的WiFi密码"
```

#### 5. 访问Web界面

1. ESP32连接Wi-Fi后，查看串口输出获取IP地址
2. 浏览器访问: `http://[ESP32_IP]`
3. 开始使用Web上位机功能

## 📡 API接口

### 主要接口

| 接口 | 方法 | 功能 |
|------|------|------|
| `/api/device/info` | GET | 获取设备信息 |
| `/api/device/status` | GET | 获取实时状态 |
| `/api/ota/upload` | POST | 上传固件文件 |
| `/api/ota/progress` | GET | 获取OTA进度 |
| `/api/wifi/status` | GET | 获取Wi-Fi状态 |
| `/api/wifi/connect` | POST | 连接Wi-Fi网络 |

### 使用示例

```bash
# 获取设备信息
curl http://192.168.1.100/api/device/info

# 上传固件
curl -X POST \
  -H "Content-Type: application/octet-stream" \
  --data-binary @firmware.bin \
  http://192.168.1.100/api/ota/upload
```

详细API文档请参考: [API接口规范](docs/api-specification.md)

## 🔄 OTA更新流程

### 通过Web界面更新

1. **准备固件文件**
   - 编译生成 `.bin` 格式固件
   - 文件大小限制: ≤1MB

2. **上传固件**
   - 访问Web界面OTA页面
   - 拖拽或选择固件文件
   - 点击"开始更新"

3. **监控进度**
   - 实时显示上传进度
   - 自动验证固件完整性
   - 设备自动重启应用新固件

4. **验证更新**
   - 检查新版本号
   - 验证功能正常
   - 如有问题可回滚到上一版本

### 安全机制

- **双分区保护**: 使用ESP32 OTA分区机制
- **自动回滚**: 更新失败时自动恢复
- **固件验证**: 校验文件完整性和格式
- **断电保护**: 更新过程中断电不会损坏设备

## 🛠️ 开发指南

### 添加新功能

1. **ESP32端**
   - 在相应模块中添加功能代码
   - 更新HTTP API接口
   - 添加FreeRTOS任务 (如需要)

2. **Web前端**
   - 创建新的React组件
   - 更新API服务接口
   - 添加路由和导航

### 调试方法

```bash
# ESP32端调试
idf.py monitor

# Web前端调试
cd web_client
npm run dev
```

### 代码规范

- **ESP32**: 遵循ESP-IDF编码规范
- **Web前端**: 使用TypeScript严格模式
- **API**: 遵循RESTful设计原则
- **文档**: 使用Markdown格式，包含emoji

## 📚 文档

- [🔧 系统技术文档](docs/ota-system.md) - 详细的技术架构和实现原理
- [📡 API接口规范](docs/api-specification.md) - 完整的API接口文档
- [🚀 部署指南](docs/deployment-guide.md) - 详细的部署和配置说明

## 🐛 故障排除

### 常见问题

1. **编译错误**
   - 检查ESP-IDF版本和环境配置
   - 清理构建缓存: `idf.py fullclean`

2. **Wi-Fi连接失败**
   - 确认网络为2.4GHz频段
   - 检查SSID和密码正确性

3. **OTA更新失败**
   - 检查固件文件格式和大小
   - 确认网络连接稳定

4. **Web界面无法访问**
   - 确认ESP32获得IP地址
   - 检查防火墙设置

详细故障排除请参考: [部署指南 - 故障排除](docs/deployment-guide.md#故障排除)

## 🤝 贡献

欢迎提交Issue和Pull Request来改进项目！

### 贡献指南

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request

## 📄 许可证

本项目采用MIT许可证 - 详见 [LICENSE](LICENSE) 文件

## 🙏 致谢

- [ESP-IDF](https://github.com/espressif/esp-idf) - ESP32开发框架
- [React](https://reactjs.org/) - Web前端框架
- [Bootstrap](https://getbootstrap.com/) - UI组件库
- [Vite](https://vitejs.dev/) - 前端构建工具

---

**📞 技术支持**: 如有问题请提交Issue或查看文档

**🔄 版本**: v1.0.0-OTA

**📅 更新时间**: 2024年1月
