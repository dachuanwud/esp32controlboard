# 🔌 ESP32控制板硬件原理图说明

本文档详细说明ESP32控制板的硬件设计原理，包括电路设计思路、信号连接关系、电源管理和保护电路设计。

## 🎯 硬件设计概述

ESP32控制板采用模块化设计，以ESP32-WROOM-32为核心，集成SBUS接收、CAN通信、LED指示、按键输入等功能，实现遥控履带车的完整控制方案。

## 🧠 核心处理器模块

### ESP32-WROOM-32模块

```
                    ESP32-WROOM-32
                   ┌─────────────────┐
    3.3V ──────────┤VCC          GND├────────── GND
    EN ────────────┤EN           IO0├────────── BOOT按键
                   ┤              IO1├────────── UART0_TX (USB)
    内置LED ───────┤IO2          IO3├────────── UART0_RX (USB)
    I2C_SDA ───────┤IO4          IO5├────────── I2C_SCL
                   ┤IO6-11    Flash├────────── (内部连接)
    LED1_R ────────┤IO12        IO13├────────── LED1_G
    LED1_B ────────┤IO14        IO15├────────── (预留)
    CAN_TX ────────┤IO16        IO17├────────── CAN_RX
    SPI_CLK ───────┤IO18        IO19├────────── SPI_MISO
                   ┤IO20        IO21├────────── CMD_RX
    SBUS_RX ───────┤IO22        IO23├────────── SPI_MOSI
                   ┤IO24        IO25├────────── LED2_R
    LED2_G ────────┤IO26        IO27├────────── LED2_B
                   ┤IO28-31  IO32-39├────────── (ADC/预留)
                   └─────────────────┘
```

**核心特性**:
- **处理器**: Xtensa 32位双核LX6
- **主频**: 240MHz
- **Flash**: 4MB
- **RAM**: 520KB
- **WiFi/蓝牙**: 集成无线通信
- **GPIO**: 34个可用引脚

## 📡 SBUS接收电路

### 信号接收设计

```
SBUS信号输入 ──┬── 保护电阻(100Ω) ──┬── GPIO22 (ESP32)
              │                    │
              └── ESD保护二极管 ─────┴── GND
                  (PESD1CAN)
```

**设计要点**:
- **信号电平**: 3.3V TTL，反相逻辑
- **保护电阻**: 100Ω限流保护
- **ESD保护**: PESD1CAN或类似器件
- **硬件反相**: ESP32内置UART反相功能

<augment_code_snippet path="main/sbus.c" mode="EXCERPT">
````c
// SBUS使用反相逻辑，硬件无反相器时必须启用软件反相
ESP_ERROR_CHECK(uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV));
ESP_LOGI(TAG, "🔄 Signal inversion: ENABLED (no hardware inverter)");
````
</augment_code_snippet>

### 连接器设计

```
SBUS连接器 (JST-XH 3Pin)
┌─────┬─────┬─────┐
│ VCC │ GND │SBUS │
│ 5V  │ GND │Data │
└─────┴─────┴─────┘
  │     │     │
  │     │     └── GPIO22 (经保护电路)
  │     └──────── GND
  └────────────── 5V (可选供电)
```

## 🚗 CAN总线通信电路

### CAN收发器电路

```
ESP32                SN65HVD232D              CAN总线
GPIO16 ──────────────┤D (Driver)         CAN_H├──── CAN_H
GPIO17 ──────────────┤R (Receiver)       CAN_L├──── CAN_L
3.3V ────────────────┤VCC                     │
GND ─────────────────┤GND                     │
GND ─────────────────┤Rs (Slope Control)      │
                     └────────────────────────┘
                              │
                              └── 120Ω终端电阻 (可选)
```

**SN65HVD232D特性**:
- **供电电压**: 3.3V
- **波特率**: 最高1Mbps (项目使用250kbps)
- **斜率控制**: Rs接地为高速模式
- **保护**: 内置过压和短路保护

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
// TWAI (CAN) 配置
static const twai_general_config_t g_config = 
    TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
````
</augment_code_snippet>

### CAN总线保护电路

```
CAN_H ──┬── TVS管 ──┬── 共模电感 ──┬── 120Ω ──┬── 到设备
        │          │              │         │
CAN_L ──┼── TVS管 ──┼── 共模电感 ──┼─────────┘
        │          │              │
        └── 0.1μF ──┴── GND        └── 0.1μF ── GND
```

**保护措施**:
- **TVS管**: 瞬态电压抑制
- **共模电感**: EMI滤波
- **终端电阻**: 120Ω阻抗匹配
- **去耦电容**: 高频滤波

## 💡 LED指示电路

### RGB LED驱动电路

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// LED指示灯引脚 - 共阳极RGB LED
// LED1组
#define LED1_RED_PIN            GPIO_NUM_12  // LED1红色引脚
#define LED1_GREEN_PIN          GPIO_NUM_13  // LED1绿色引脚
#define LED1_BLUE_PIN           GPIO_NUM_14  // LED1蓝色引脚

// LED2组
#define LED2_RED_PIN            GPIO_NUM_25  // LED2红色引脚
#define LED2_GREEN_PIN          GPIO_NUM_26  // LED2绿色引脚
#define LED2_BLUE_PIN           GPIO_NUM_27  // LED2蓝色引脚
````
</augment_code_snippet>

```
LED1组 (共阳极)
3.3V ──┬── LED_R ──┬── 限流电阻(330Ω) ──── GPIO12
       │          │
       ├── LED_G ──┼── 限流电阻(330Ω) ──── GPIO13
       │          │
       └── LED_B ──┴── 限流电阻(330Ω) ──── GPIO14

LED2组 (共阳极)
3.3V ──┬── LED_R ──┬── 限流电阻(330Ω) ──── GPIO25
       │          │
       ├── LED_G ──┼── 限流电阻(330Ω) ──── GPIO26
       │          │
       └── LED_B ──┴── 限流电阻(330Ω) ──── GPIO27
```

**设计参数**:
- **LED类型**: 共阳极RGB LED
- **工作电流**: 10mA (每个颜色)
- **限流电阻**: 330Ω
- **控制逻辑**: 低电平点亮

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 设置LED初始状态 - 共阳极LED，高电平(1)熄灭，低电平(0)点亮
// LED1组初始状态 - 全部熄灭
gpio_set_level(LED1_RED_PIN, 1);
gpio_set_level(LED1_GREEN_PIN, 1);
gpio_set_level(LED1_BLUE_PIN, 1);
````
</augment_code_snippet>

## 🔘 按键输入电路

### 按键电路设计

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
#define KEY2_PIN                GPIO_NUM_35  // 按键2
````
</augment_code_snippet>

```
按键1 (BOOT按键)
3.3V ──┬── 内部上拉(45kΩ) ──┬── GPIO0
       │                    │
       └── 按键开关 ─────────┴── GND
                            │
                            └── 去抖电容(100nF)

按键2 (用户按键)
3.3V ──┬── 内部上拉(45kΩ) ──┬── GPIO35
       │                    │
       └── 按键开关 ─────────┴── GND
                            │
                            └── 去抖电容(100nF)
```

**设计特点**:
- **上拉电阻**: ESP32内部45kΩ上拉
- **去抖电容**: 100nF硬件去抖
- **ESD保护**: 输入保护二极管
- **双功能**: GPIO0兼具BOOT功能

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 配置按键引脚
io_conf.intr_type = GPIO_INTR_POSEDGE;  // 上升沿触发中断
io_conf.mode = GPIO_MODE_INPUT;
io_conf.pin_bit_mask = (1ULL << KEY1_PIN) | (1ULL << KEY2_PIN);
io_conf.pull_down_en = 0;
io_conf.pull_up_en = 1;  // 启用内部上拉电阻
gpio_config(&io_conf);
````
</augment_code_snippet>

## ⚡ 电源管理电路

### 主电源设计

```
5V输入 ──┬── 防反接二极管 ──┬── 自恢复保险丝 ──┬── LDO稳压器 ──┬── 3.3V输出
         │  (肖特基)      │   (1A)          │  (AMS1117)   │
         │                │                 │              ├── ESP32
         └── 电源指示LED ──┴── 滤波电容 ──────┴── 去耦电容 ──┤
                              (1000μF)        (100μF)     ├── 外设
                                                          └── LED等
```

**电源规格**:
- **输入电压**: 5V ±5% (4.75V-5.25V)
- **输出电压**: 3.3V ±3% (3.2V-3.4V)
- **最大电流**: 800mA
- **纹波**: < 50mV
- **效率**: > 85%

### 电源保护电路

```
输入保护:
5V ──┬── TVS管(6.8V) ──┬── 自恢复保险丝 ──┬── 滤波
     │                │                  │
     └── 防反接二极管 ──┴── EMI滤波电感 ───┴── 稳压器

输出保护:
3.3V ──┬── 去耦电容 ──┬── 输出
       │             │
       └── 软启动 ────┴── 过流保护
```

## 🔌 接口连接器设计

### 主要连接器规格

| 接口 | 连接器类型 | 引脚数 | 间距 | 用途 |
|------|------------|--------|------|------|
| SBUS | JST-XH | 3Pin | 2.54mm | SBUS信号输入 |
| CAN | 端子排 | 4Pin | 5.08mm | CAN总线输出 |
| 电源 | DC插座 | 2Pin | - | 5V电源输入 |
| 调试 | USB-C | - | - | 程序下载/调试 |

### USB调试接口

```
USB-C接口 ──── CH340G ──── ESP32
             (USB转UART)
                │
                ├── VCC (5V)
                ├── GND
                ├── TXD ──── GPIO1 (RX)
                └── RXD ──── GPIO3 (TX)
```

**CH340G特性**:
- **接口**: USB 2.0 Full Speed
- **波特率**: 最高2Mbps
- **供电**: 5V USB供电
- **驱动**: 免驱动或通用驱动

## 🛡️ EMC和安全设计

### EMI抑制措施

```
电源EMI滤波:
AC输入 ──┬── 共模电感 ──┬── 差模电容 ──┬── 输出
         │             │             │
         └── 共模电容 ──┴── 共模电容 ──┘
```

**EMI设计要点**:
- **共模电感**: 抑制共模干扰
- **差模电容**: 滤除差模噪声
- **屏蔽**: 关键信号屏蔽处理
- **接地**: 完整的地平面设计

### ESD保护设计

```
输入保护:
信号输入 ──┬── 保护电阻 ──┬── GPIO
          │             │
          └── ESD二极管 ──┴── GND
              (PESD系列)
```

**ESD保护等级**:
- **人体模型**: ±8kV (IEC 61000-4-2)
- **机器模型**: ±500V
- **器件选择**: PESD1CAN, PESD5V0S1BA等

## 📏 PCB设计要求

### 布局原则

```
PCB分区布局:
┌─────────────────────────────────┐
│  电源区域    │    数字区域      │
│  (LDO等)     │   (ESP32等)     │
├─────────────────────────────────┤
│  模拟区域    │    接口区域      │
│  (ADC等)     │  (连接器等)     │
└─────────────────────────────────┘
```

### 布线规则

| 信号类型 | 线宽 | 间距 | 阻抗 | 说明 |
|----------|------|------|------|------|
| 电源 | 0.5mm | 0.2mm | - | 大电流走线 |
| 数字信号 | 0.1mm | 0.1mm | 50Ω | 普通数字信号 |
| 差分信号 | 0.1mm | 0.1mm | 100Ω | CAN差分对 |
| 高频信号 | 0.1mm | 0.15mm | 50Ω | 时钟等信号 |

### 层叠设计

```
4层PCB层叠:
Layer 1: 信号层 (元件面)
Layer 2: 地层 (GND)
Layer 3: 电源层 (3.3V/5V)
Layer 4: 信号层 (焊接面)
```

## 🔧 调试和测试接口

### 测试点设计

| 测试点 | 位置 | 用途 | 规格 |
|--------|------|------|------|
| TP_5V | 电源输入 | 5V电压测试 | Φ1.0mm |
| TP_3V3 | 电源输出 | 3.3V电压测试 | Φ1.0mm |
| TP_CAN_H | CAN接口 | CAN_H信号测试 | Φ0.8mm |
| TP_CAN_L | CAN接口 | CAN_L信号测试 | Φ0.8mm |
| TP_SBUS | SBUS接口 | SBUS信号测试 | Φ0.8mm |

### JTAG调试接口

```
JTAG接口 (2.54mm排针)
┌─────┬─────┬─────┬─────┐
│ VCC │ GND │ TCK │ TDO │
├─────┼─────┼─────┼─────┤
│ TMS │ TDI │ RST │ NC  │
└─────┴─────┴─────┴─────┘
```

**JTAG信号连接**:
- **TCK**: GPIO13 (MTCK)
- **TDO**: GPIO15 (MTDO)  
- **TDI**: GPIO12 (MTDI)
- **TMS**: GPIO14 (MTMS)

## 📋 设计验证清单

### 电路设计检查

- [ ] 电源电压和电流满足需求
- [ ] 信号电平匹配正确
- [ ] 保护电路设计完整
- [ ] EMC设计措施到位
- [ ] 连接器选型正确
- [ ] 测试点布置合理

### PCB设计检查

- [ ] 布局分区合理
- [ ] 布线规则符合要求
- [ ] 电源和地平面完整
- [ ] 差分信号等长匹配
- [ ] 去耦电容就近放置
- [ ] 机械尺寸符合要求

---

💡 **提示**: 硬件原理图是系统设计的基础，任何修改都需要充分验证和测试！
