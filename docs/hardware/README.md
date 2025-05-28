# ⚡ 硬件相关文档

本目录包含ESP32控制板项目的硬件设计文档，涵盖硬件原理图、引脚映射、元器件清单、PCB设计等硬件相关的详细说明。

## 📋 待添加文档列表

### 🔌 硬件设计
- **schematic.md** - 硬件原理图说明
  - 电路设计原理
  - 信号连接关系
  - 电源管理设计
  - 保护电路设计

### 📍 引脚定义
- **pin-mapping.md** - 引脚映射表
  - GPIO功能分配
  - 外设接口定义
  - 信号方向说明
  - 电气特性参数

### 📦 器件清单
- **component-list.md** - 元器件清单
  - 主要芯片规格
  - 被动器件参数
  - 连接器规格
  - 可选器件说明

## 🎯 硬件架构

### 系统框图
```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32控制板硬件架构                        │
├─────────────────────────────────────────────────────────────┤
│  外设接口  │  SBUS输入  │  CAN输出   │  UART调试  │  电源接口  │
├─────────────────────────────────────────────────────────────┤
│  信号调理  │  反相电路  │  CAN收发器 │  电平转换  │  稳压电路  │
├─────────────────────────────────────────────────────────────┤
│  主控芯片  │           ESP32-WROOM-32                        │
├─────────────────────────────────────────────────────────────┤
│  电源管理  │  5V输入    │  3.3V稳压  │  电源指示  │  保护电路  │
└─────────────────────────────────────────────────────────────┘
```

## 🔌 接口定义

### 主要接口
- **SBUS输入**: 3.3V TTL，反相逻辑
- **CAN输出**: 差分信号，120Ω终端
- **UART调试**: 3.3V TTL，115200bps
- **电源输入**: 5V DC，最大1A

### 连接器规格
- **SBUS接口**: 3Pin JST-XH 2.54mm
- **CAN接口**: 4Pin 端子排 5.08mm
- **调试接口**: USB Type-C
- **电源接口**: DC5.5×2.1mm 或 端子排

## 📍 ESP32引脚分配

### 当前使用的引脚
| GPIO | 功能 | 方向 | 说明 |
|------|------|------|------|
| GPIO0 | BOOT | INPUT | 启动模式选择 |
| GPIO2 | LED | OUTPUT | 状态指示LED |
| GPIO16 | CAN_TX | OUTPUT | CAN发送 |
| GPIO17 | CAN_RX | INPUT | CAN接收 |
| GPIO22 | SBUS_RX | INPUT | SBUS数据接收 |

### 保留引脚
| GPIO | 功能 | 方向 | 说明 |
|------|------|------|------|
| GPIO4 | SDA | I/O | I2C数据线 |
| GPIO5 | SCL | OUTPUT | I2C时钟线 |
| GPIO18 | SPI_CLK | OUTPUT | SPI时钟 |
| GPIO19 | SPI_MISO | INPUT | SPI主入从出 |
| GPIO21 | USER_KEY | INPUT | 用户按键 |
| GPIO23 | SPI_MOSI | OUTPUT | SPI主出从入 |

### 特殊功能引脚
| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO1 | TXD0 | UART0发送 (USB调试) |
| GPIO3 | RXD0 | UART0接收 (USB调试) |
| GPIO6-11 | FLASH | 连接外部Flash (不可用) |
| GPIO12 | MTDI | JTAG调试接口 |
| GPIO13 | MTCK | JTAG调试接口 |
| GPIO14 | MTMS | JTAG调试接口 |
| GPIO15 | MTDO | JTAG调试接口 |

## ⚡ 电源设计

### 电源规格
- **输入电压**: 5V ±5%
- **输出电压**: 3.3V ±3%
- **最大电流**: 800mA
- **效率**: >85%

### 电源架构
```
5V输入 → LDO稳压器 → 3.3V输出 → ESP32 + 外设
         ↓
      电源指示LED
         ↓
      过流保护电路
```

### 电源管理
- **低功耗模式**: 支持ESP32深度睡眠
- **电源监控**: 电压检测和报警
- **软启动**: 防止上电冲击
- **过压保护**: 输入过压保护

## 🔧 信号调理

### SBUS信号调理
- **输入**: 反相3.3V TTL
- **处理**: ESP32硬件反相功能
- **保护**: ESD保护二极管
- **滤波**: RC低通滤波器

### CAN信号调理
- **收发器**: TJA1050或兼容芯片
- **终端电阻**: 120Ω可选
- **保护**: TVS管保护
- **隔离**: 可选光电隔离

### 调试接口
- **USB转UART**: CH340G或CP2102
- **电平**: 3.3V TTL
- **速率**: 最高2Mbps
- **保护**: ESD保护

## 🛡️ 保护电路

### 输入保护
- **反接保护**: 肖特基二极管
- **过压保护**: TVS管
- **过流保护**: 自恢复保险丝
- **EMI滤波**: 共模电感 + 电容

### 输出保护
- **短路保护**: 限流电阻
- **ESD保护**: ESD保护二极管
- **过压保护**: 齐纳二极管
- **滤波**: 去耦电容

## 📏 PCB设计要求

### 布局原则
- **模拟数字分离**: 模拟和数字电路分区
- **电源平面**: 完整的电源和地平面
- **信号完整性**: 控制阻抗和串扰
- **热管理**: 散热焊盘和通孔

### 布线规则
- **最小线宽**: 0.1mm (4mil)
- **最小间距**: 0.1mm (4mil)
- **过孔大小**: 0.2mm (8mil)
- **阻抗控制**: 50Ω单端，100Ω差分

### 制造规格
- **板厚**: 1.6mm
- **层数**: 4层 (信号-地-电源-信号)
- **表面处理**: HASL或OSP
- **阻焊**: 绿色，白色丝印

## 🔍 测试点设计

### 关键测试点
- **电源**: 5V输入，3.3V输出
- **时钟**: ESP32主时钟
- **复位**: 复位信号
- **通信**: SBUS，CAN，UART信号

### 测试接口
- **JTAG**: 调试和烧录接口
- **SWD**: 简化调试接口
- **UART**: 串口调试接口
- **GPIO**: 通用测试接口

## 📊 电气特性

### ESP32规格
- **工作电压**: 3.0V ~ 3.6V
- **工作电流**: 80mA (正常)，150mA (峰值)
- **睡眠电流**: 5μA (深度睡眠)
- **工作温度**: -40°C ~ +85°C

### 接口规格
- **GPIO电流**: 最大40mA
- **输入电压**: 0V ~ 3.6V
- **输出电压**: 0V ~ 3.3V
- **输入阻抗**: >1MΩ

## 🛠️ 调试接口

### USB调试接口
- **连接器**: USB Type-C
- **芯片**: CH340G USB转UART
- **速率**: 115200 bps (默认)
- **功能**: 程序下载，日志输出

### JTAG调试接口
- **连接器**: 2.54mm排针
- **信号**: TCK, TMS, TDI, TDO
- **功能**: 在线调试，Flash烧录
- **工具**: ESP-Prog，J-Link

## 📚 参考资料

### 芯片手册
- [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- [ESP32-WROOM-32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf)

### 设计指南
- [ESP32 Hardware Design Guidelines](https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf)
- [ESP32 PCB Layout Guidelines](https://www.espressif.com/sites/default/files/documentation/esp32_pcb_layout_guidelines_en.pdf)

### 应用笔记
- [ESP32 Power Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/power_management.html)
- [ESP32 GPIO and RTC GPIO](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)

---

💡 **提示**: 硬件设计时请严格遵循ESP32的设计指南和电气规范！
