# 🚀 ESP32控制板Web OTA系统部署指南

## 📋 部署概述

本指南将详细介绍如何部署ESP32控制板Web OTA系统，包括开发环境配置、固件编译、Web前端构建、设备烧录和系统测试等完整流程。

## 🛠️ 环境准备

### 硬件要求

- **ESP32开发板**: 支持Wi-Fi功能的ESP32芯片
- **USB数据线**: Type-C或Micro-USB (根据开发板接口)
- **电脑**: Windows 10/11, macOS, 或 Linux
- **网络环境**: 2.4GHz Wi-Fi网络

### 软件环境

#### ESP-IDF开发环境

1. **安装ESP-IDF**
   ```bash
   # 下载ESP-IDF v5.4.1
   git clone -b v5.4.1 --recursive https://github.com/espressif/esp-idf.git
   
   # 安装依赖
   cd esp-idf
   ./install.sh  # Linux/macOS
   install.bat   # Windows
   ```

2. **设置环境变量**
   ```bash
   # Linux/macOS
   . $HOME/esp/esp-idf/export.sh
   
   # Windows
   %userprofile%\esp\esp-idf\export.bat
   ```

#### Node.js开发环境

1. **安装Node.js**
   - 访问 [Node.js官网](https://nodejs.org/)
   - 下载并安装LTS版本 (推荐18.x或更高版本)

2. **验证安装**
   ```bash
   node --version  # 应显示v18.x.x或更高
   npm --version   # 应显示npm版本
   ```

## 📦 项目构建

### 方法一：一键构建 (推荐)

使用提供的批处理脚本进行完整构建：

```bash
# Windows
build_all_ota.bat

# 该脚本将自动完成：
# 1. ESP32固件编译
# 2. Web前端构建
# 3. 显示构建结果
```

### 方法二：分步构建

#### 1. ESP32固件构建

```bash
# 进入项目目录
cd esp32controlboard

# 配置项目 (可选)
idf.py menuconfig

# 编译固件
idf.py build

# 或使用提供的脚本
build_only.bat
```

**构建输出文件:**
- `build/esp32controlboard.bin` - 主应用程序
- `build/bootloader/bootloader.bin` - 引导程序
- `build/partition_table/partition-table.bin` - 分区表

#### 2. Web前端构建

```bash
# 进入Web客户端目录
cd web_client

# 安装依赖
npm install

# 构建生产版本
npm run build

# 或使用提供的脚本
cd ..
build_web.bat
```

**构建输出文件:**
- `web_client/dist/` - 生产版本文件
- `web_client/dist/index.html` - 主页面
- `web_client/dist/assets/` - 静态资源

## 🔧 设备配置

### 1. 硬件连接

1. **连接ESP32到电脑**
   - 使用USB数据线连接ESP32开发板
   - 确认设备管理器中出现COM端口

2. **检查端口号**
   ```bash
   # Windows
   # 查看设备管理器中的COM端口号
   
   # Linux/macOS
   ls /dev/tty*
   ```

### 2. 固件烧录

#### 首次烧录 (完整烧录)

```bash
# 使用提供的脚本 (Windows)
flash_com10.bat

# 或手动烧录
idf.py -p COM10 flash

# Linux/macOS示例
idf.py -p /dev/ttyUSB0 flash
```

#### 监控串口输出

```bash
# 烧录后监控设备启动
idf.py -p COM10 monitor

# 或组合烧录和监控
idf.py -p COM10 flash monitor
```

### 3. Wi-Fi网络配置

#### 方法一：硬编码配置 (推荐用于测试)

修改 `main/main.c` 文件中的Wi-Fi配置：

```c
// Wi-Fi配置 - 修改为您的网络信息
#define DEFAULT_WIFI_SSID     "您的WiFi名称"
#define DEFAULT_WIFI_PASSWORD "您的WiFi密码"
```

重新编译并烧录固件。

#### 方法二：Web界面配置

1. ESP32首次启动时会创建热点
2. 连接到ESP32热点
3. 通过Web界面配置Wi-Fi网络

## 🌐 Web前端部署

### 方法一：本地开发服务器

```bash
cd web_client

# 启动开发服务器
npm run dev

# 访问 http://localhost:3000
```

### 方法二：生产环境部署

#### 使用Nginx

1. **安装Nginx**
   ```bash
   # Ubuntu/Debian
   sudo apt install nginx
   
   # CentOS/RHEL
   sudo yum install nginx
   ```

2. **配置Nginx**
   ```nginx
   server {
       listen 80;
       server_name localhost;
       
       location / {
           root /path/to/web_client/dist;
           index index.html;
           try_files $uri $uri/ /index.html;
       }
       
       location /api/ {
           proxy_pass http://192.168.1.100/api/;
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
       }
   }
   ```

3. **启动服务**
   ```bash
   sudo systemctl start nginx
   sudo systemctl enable nginx
   ```

#### 使用Apache

1. **安装Apache**
   ```bash
   # Ubuntu/Debian
   sudo apt install apache2
   
   # CentOS/RHEL
   sudo yum install httpd
   ```

2. **配置虚拟主机**
   ```apache
   <VirtualHost *:80>
       DocumentRoot /path/to/web_client/dist
       
       ProxyPass /api/ http://192.168.1.100/api/
       ProxyPassReverse /api/ http://192.168.1.100/api/
   </VirtualHost>
   ```

## 🔍 系统测试

### 1. 基础功能测试

#### ESP32设备测试

1. **串口监控**
   ```bash
   idf.py -p COM10 monitor
   ```
   
   检查输出日志：
   - ✅ Wi-Fi连接成功
   - ✅ HTTP服务器启动
   - ✅ OTA管理器初始化

2. **网络连接测试**
   ```bash
   # 查找ESP32设备IP
   ping 192.168.1.100
   
   # 测试HTTP服务
   curl http://192.168.1.100/api/device/info
   ```

#### Web界面测试

1. **访问Web界面**
   - 浏览器打开: `http://[ESP32_IP]`
   - 检查页面是否正常加载

2. **功能测试清单**
   - [ ] 设备信息页面显示正常
   - [ ] 实时状态数据更新
   - [ ] SBUS通道数据显示 (如已连接)
   - [ ] Wi-Fi状态显示正确
   - [ ] OTA上传界面可用

### 2. OTA更新测试

#### 准备测试固件

1. **修改固件版本号**
   ```c
   // 在main.c中修改版本信息
   strcpy(info->firmware_version, "1.0.1-OTA-TEST");
   ```

2. **重新编译**
   ```bash
   idf.py build
   ```

#### 执行OTA测试

1. **通过Web界面上传固件**
   - 选择 `build/esp32controlboard.bin` 文件
   - 点击"开始更新"
   - 监控更新进度

2. **验证更新结果**
   - 设备自动重启
   - 检查新版本号
   - 验证功能正常

### 3. 压力测试

#### 网络稳定性测试

```bash
# 持续ping测试
ping -t 192.168.1.100

# API接口压力测试
for i in {1..100}; do
  curl http://192.168.1.100/api/device/status
  sleep 1
done
```

#### 内存泄漏测试

监控ESP32设备的内存使用情况：

```bash
# 持续监控串口输出中的内存信息
idf.py -p COM10 monitor | grep -i "heap\|memory"
```

## 🐛 故障排除

### 常见问题及解决方案

#### 1. 编译错误

**问题**: ESP-IDF编译失败
```
解决方案:
1. 检查ESP-IDF版本 (推荐v5.4.1)
2. 确认环境变量设置正确
3. 清理构建缓存: idf.py fullclean
4. 重新安装ESP-IDF依赖
```

**问题**: Web前端构建失败
```
解决方案:
1. 检查Node.js版本 (需要18+)
2. 清理node_modules: rm -rf node_modules package-lock.json
3. 重新安装依赖: npm install
4. 检查TypeScript错误
```

#### 2. 烧录问题

**问题**: 无法连接到ESP32
```
解决方案:
1. 检查USB数据线连接
2. 确认COM端口号正确
3. 检查驱动程序安装
4. 尝试按住BOOT按钮烧录
```

**问题**: 烧录失败
```
解决方案:
1. 降低烧录波特率: idf.py -p COM10 -b 115200 flash
2. 检查Flash大小配置
3. 尝试擦除Flash: idf.py -p COM10 erase_flash
```

#### 3. 网络连接问题

**问题**: Wi-Fi连接失败
```
解决方案:
1. 检查SSID和密码正确性
2. 确认网络为2.4GHz频段
3. 检查网络信号强度
4. 查看ESP32串口日志
```

**问题**: 无法访问Web界面
```
解决方案:
1. 确认ESP32获得IP地址
2. 检查防火墙设置
3. 尝试直接访问API: curl http://[IP]/api/device/info
4. 检查HTTP服务器状态
```

#### 4. OTA更新问题

**问题**: OTA更新失败
```
解决方案:
1. 检查固件文件格式 (.bin)
2. 确认文件大小 (≤1MB)
3. 检查网络稳定性
4. 查看OTA错误日志
```

**问题**: 更新后设备无法启动
```
解决方案:
1. 等待自动回滚 (30秒)
2. 手动回滚: 访问 /api/ota/rollback
3. 重新烧录原始固件
4. 检查分区表配置
```

## 📊 性能监控

### 系统资源监控

#### ESP32端监控

```c
// 在代码中添加监控日志
ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
ESP_LOGI(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());
```

#### 网络性能监控

```bash
# 监控网络延迟
ping -c 100 192.168.1.100

# 监控HTTP响应时间
curl -w "@curl-format.txt" -o /dev/null -s http://192.168.1.100/api/device/info
```

### 日志分析

#### ESP32日志级别配置

```bash
# 设置日志级别
idf.py menuconfig
# Component config -> Log output -> Default log verbosity
```

#### Web服务器访问日志

在HTTP服务器代码中添加访问日志：

```c
ESP_LOGI(TAG, "API Request: %s %s from %s", 
         httpd_req_get_method(req), 
         req->uri, 
         "client_ip");
```

## 🔄 维护和更新

### 定期维护任务

1. **固件更新**
   - 定期检查新版本
   - 测试OTA更新流程
   - 备份当前配置

2. **系统监控**
   - 检查内存使用情况
   - 监控网络连接稳定性
   - 分析错误日志

3. **安全更新**
   - 更新Wi-Fi密码
   - 检查访问控制
   - 审查API安全性

### 版本管理

#### 固件版本管理

```c
// 版本号格式: MAJOR.MINOR.PATCH-TYPE
#define FIRMWARE_VERSION "1.0.0-OTA"
```

#### Web前端版本管理

```json
{
  "version": "1.0.0",
  "build": "20240101-001"
}
```

---

*本部署指南基于ESP32控制板Web OTA系统v1.0.0，如遇问题请参考故障排除章节或联系技术支持。*
