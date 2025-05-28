# 🛠️ ESP32控制板CAN工具

这是一个专为ESP32控制板项目设计的CAN设备检测、配置和测试工具集。支持与LKBLS481502电机驱动器的CAN通信测试，完全兼容ESP32项目的TWAI配置。

## 🎯 功能特性

- **🔍 自动检测**: 扫描并识别可用的CAN接口设备
- **⚙️ 智能配置**: 自动配置250kbps波特率，匹配ESP32 TWAI设置
- **🧪 通信测试**: 发送/接收CAN消息，验证通信链路
- **👁️ 实时监控**: 实时显示CAN总线上的所有消息
- **🚗 电机控制**: 模拟ESP32项目的LKBLS481502电机控制命令
- **🎮 差速测试**: 完整的差速控制逻辑测试

## 📋 支持的CAN接口

| 接口类型 | 描述 | 示例设备 |
|----------|------|----------|
| **SocketCAN** | Linux原生CAN支持 | can0, can1 |
| **PEAK CAN** | PEAK-System CAN设备 | PCAN-USB, PCAN-USB Pro |
| **Vector CAN** | Vector CAN接口 | VN1610, VN1640 |
| **Kvaser CAN** | Kvaser CAN设备 | Leaf Light, USBcan Pro |
| **USB2CAN** | USB转CAN适配器 | USB2CAN, CANtact |
| **串口CAN** | 串口CAN设备 | SLCAN, Lawicel |

## 🚀 快速开始

### 1. 环境准备

确保您的系统已安装Python 3.7或更高版本：

```bash
python --version
```

### 2. 安装依赖

```bash
# 进入工具目录
cd tools

# 安装Python依赖包
pip install -r requirements.txt

# 或使用国内镜像源（推荐）
pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple/
```

### 3. 运行工具

#### 🎯 Windows用户（推荐）
```cmd
# 双击运行批处理文件，或在命令行中执行
can_tool.bat
```

#### ⚡ 快速配置模式（推荐新手）
```bash
python quick_can_setup.py
```

#### 🔧 完整功能模式
```bash
# 交互式模式（推荐）
python can_tool.py

# 快速检测模式
python can_tool.py --detect

# 监控模式
python can_tool.py --monitor

# 电机测试模式
python can_tool.py --test-motor
```

## 🎮 使用指南

### 交互式模式操作流程

1. **检测CAN接口**
   - 选择选项 `1` 检测可用的CAN设备
   - 工具会自动扫描所有支持的接口类型

2. **连接CAN接口**
   - 选择选项 `2` 连接到检测到的CAN接口
   - 从列表中选择要使用的接口

3. **测试通信**
   - 选择选项 `3` 进行基本的CAN通信测试
   - 发送测试消息并尝试接收响应

4. **电机控制测试**
   - 选择选项 `4` 进行电机控制命令测试
   - 支持使能/禁用电机、设置速度、差速控制

5. **实时监控**
   - 选择选项 `5` 启动CAN总线实时监控
   - 显示所有CAN消息的详细信息

### 电机控制命令格式

工具完全兼容ESP32项目的CAN命令格式：

#### 使能电机命令
```
CAN ID: 0x06000001 (扩展帧)
数据: [0x23, 0x0D, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
```

#### 禁用电机命令
```
CAN ID: 0x06000001 (扩展帧)
数据: [0x23, 0x0C, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
```

#### 速度控制命令
```
CAN ID: 0x06000001 (扩展帧)
数据: [0x23, 0x00, 0x20, channel, speed_byte3, speed_byte2, speed_byte1, speed_byte0]
```

其中：
- `channel`: 1=左电机, 2=右电机
- `speed`: -100~+100 (应用层) → -10000~+10000 (驱动器层)

## 🔧 配置说明

### ESP32兼容配置

工具使用与ESP32项目完全相同的配置参数：

```python
esp32_config = {
    'bitrate': 250000,        # 250kbps，匹配TWAI_TIMING_CONFIG_250KBITS()
    'extended_id': True,      # 扩展帧，匹配ESP32项目
    'motor_base_id': 0x06000001,  # CAN ID，匹配ESP32项目
}
```

### 硬件连接

确保您的CAN设备正确连接：

```
CAN工具 ←→ CAN接口设备 ←→ CAN总线 ←→ ESP32控制板
```

CAN总线需要120Ω终端电阻（总线两端各一个）。

## 🐛 故障排除

### 常见问题

#### 1. 未检测到CAN接口
**可能原因**:
- CAN设备未正确连接
- 驱动程序未安装
- 设备权限不足

**解决方案**:
```bash
# Linux系统检查CAN设备
ip link show

# 检查USB设备
lsusb

# 检查串口设备
ls /dev/tty*
```

#### 2. 连接失败
**可能原因**:
- 波特率不匹配
- 设备被其他程序占用
- 硬件故障

**解决方案**:
- 确认设备未被其他程序使用
- 检查CAN总线终端电阻
- 验证硬件连接

#### 3. 发送失败
**可能原因**:
- CAN总线错误
- 无应答设备
- 总线负载过高

**解决方案**:
- 检查CAN总线状态
- 确认目标设备在线
- 降低发送频率

### 调试模式

启用详细日志输出：

```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 波特率 | 250 kbps | 匹配ESP32 TWAI配置 |
| 帧格式 | CAN 2.0B扩展帧 | 29位ID + 8字节数据 |
| 发送延迟 | < 10ms | 软件处理时间 |
| 监控精度 | 微秒级 | 时间戳精度 |

## 🔗 相关链接

- [ESP32控制板项目](../README.md)
- [CAN模块文档](../docs/modules/can-module.md)
- [硬件原理图](../docs/hardware/schematic.md)
- [python-can官方文档](https://python-can.readthedocs.io/)

## 📁 文件说明

| 文件名 | 描述 | 用途 |
|--------|------|------|
| `can_tool.py` | 主程序 | 完整功能的CAN工具 |
| `can_detector.py` | 检测模块 | CAN设备检测和配置类 |
| `quick_can_setup.py` | 快速配置 | 简化的CAN配置工具 |
| `can_tool.bat` | Windows脚本 | Windows系统快速启动 |
| `requirements.txt` | 依赖列表 | Python包依赖管理 |
| `README.md` | 说明文档 | 详细使用说明 |

## 🎯 使用场景

### 场景1: 初次使用CAN设备
```bash
# 1. 运行快速配置工具
python quick_can_setup.py

# 2. 选择"快速电机测试"验证连接
# 3. 观察电机是否响应命令
```

### 场景2: 调试CAN通信问题
```bash
# 1. 启动监控模式
python can_tool.py --monitor

# 2. 在另一个终端发送测试命令
python can_tool.py --test-motor

# 3. 观察CAN消息是否正确发送/接收
```

### 场景3: 验证ESP32兼容性
```bash
# 1. 使用完整工具连接CAN设备
python can_tool.py

# 2. 发送与ESP32相同格式的命令
# 3. 确认电机驱动器响应正确
```

## 📝 更新日志

### v1.0.0 (2024-01-XX)
- ✨ 初始版本发布
- 🔍 支持多种CAN接口自动检测
- ⚙️ ESP32项目兼容配置
- 🚗 LKBLS481502电机控制命令支持
- 👁️ 实时CAN总线监控
- 🎮 差速控制测试功能
- ⚡ 快速配置工具
- 🎯 Windows批处理脚本

## 📄 许可证

本工具遵循MIT许可证，与ESP32控制板项目保持一致。

---

💡 **提示**: 使用前请确保CAN设备驱动程序已正确安装，并且具有足够的系统权限！
