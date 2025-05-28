# 📍 ESP32控制板引脚映射表

本文档详细列出ESP32控制板项目的完整引脚分配，包括功能定义、电气特性、连接说明和使用注意事项。

## 🎯 引脚分配概览

ESP32-WROOM-32模块共有38个GPIO引脚，本项目根据功能需求进行了合理分配，确保各模块正常工作且预留扩展空间。

## 📊 完整引脚映射表

### 主要功能引脚

| GPIO | 功能 | 方向 | 电平 | 连接对象 | 说明 |
|------|------|------|------|----------|------|
| GPIO0 | BOOT/KEY1 | INPUT | 3.3V | 按键1 | 启动模式选择/用户按键 |
| GPIO1 | TXD0 | OUTPUT | 3.3V | USB调试 | UART0发送 (调试输出) |
| GPIO2 | LED_STATUS | OUTPUT | 3.3V | 状态LED | 内置LED指示 |
| GPIO3 | RXD0 | INPUT | 3.3V | USB调试 | UART0接收 (调试输入) |

### LED指示灯引脚

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

| GPIO | 功能 | 方向 | 电平 | LED组 | 颜色 | 说明 |
|------|------|------|------|-------|------|------|
| GPIO12 | LED1_RED | OUTPUT | 3.3V | LED1 | 红色 | 共阳极，低电平点亮 |
| GPIO13 | LED1_GREEN | OUTPUT | 3.3V | LED1 | 绿色 | 共阳极，低电平点亮 |
| GPIO14 | LED1_BLUE | OUTPUT | 3.3V | LED1 | 蓝色 | 共阳极，低电平点亮 |
| GPIO25 | LED2_RED | OUTPUT | 3.3V | LED2 | 红色 | 共阳极，低电平点亮 |
| GPIO26 | LED2_GREEN | OUTPUT | 3.3V | LED2 | 绿色 | 共阳极，低电平点亮 |
| GPIO27 | LED2_BLUE | OUTPUT | 3.3V | LED2 | 蓝色 | 共阳极，低电平点亮 |

### 通信接口引脚

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
#define KEY2_PIN                GPIO_NUM_35  // 按键2

// UART定义
#define UART_DEBUG              UART_NUM_0   // 调试串口 (通过CH340)
#define UART_CMD                UART_NUM_1   // CMD_VEL接收 (RX: GPIO_NUM_21)
#define UART_SBUS               UART_NUM_2   // SBUS接收 (RX: GPIO_NUM_22)
````
</augment_code_snippet>

| GPIO | 功能 | 方向 | 协议 | 配置 | 连接对象 | 说明 |
|------|------|------|------|------|----------|------|
| GPIO16 | CAN_TX | OUTPUT | CAN | 250kbps | SN65HVD232D-D | CAN总线发送 |
| GPIO17 | CAN_RX | INPUT | CAN | 250kbps | SN65HVD232D-R | CAN总线接收 |
| GPIO21 | CMD_RX | INPUT | UART | 115200,8N1 | CMD_VEL输入 | 命令接收 |
| GPIO22 | SBUS_RX | INPUT | UART | 100000,8E2 | SBUS信号 | 遥控数据接收 |
| GPIO35 | KEY2 | INPUT | GPIO | 3.3V | 按键2 | 用户按键 (只读) |

## 🔌 接口详细配置

### 1. SBUS接收接口

<augment_code_snippet path="main/sbus.c" mode="EXCERPT">
````c
esp_err_t sbus_init(void)
{
    // SBUS协议配置：100000 bps, 8E2, 反相逻辑
    uart_config_t uart_config = {
        .baud_rate = 100000,            // SBUS标准波特率
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,     // SBUS使用偶校验
        .stop_bits = UART_STOP_BITS_2,  // SBUS使用2停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // 设置引脚：只配置RX引脚
    ESP_ERROR_CHECK(uart_set_pin(UART_SBUS, UART_PIN_NO_CHANGE, GPIO_NUM_22, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // SBUS使用反相逻辑，硬件无反相器时必须启用软件反相
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV));
}
````
</augment_code_snippet>

**SBUS接口特性**:
- **引脚**: GPIO22 (仅RX)
- **协议**: 100000 bps, 8E2
- **信号**: 反相逻辑 (硬件反相)
- **连接**: 直接连接SBUS信号线

### 2. CAN总线接口

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
// TWAI (CAN) 配置
static const twai_general_config_t g_config = 
    TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
````
</augment_code_snippet>

**CAN接口特性**:
- **TX引脚**: GPIO16 → SN65HVD232D-D
- **RX引脚**: GPIO17 ← SN65HVD232D-R  
- **波特率**: 250 kbps
- **收发器**: SN65HVD232D
- **终端电阻**: 120Ω

### 3. LED控制接口

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
/**
 * 初始化GPIO
 */
static void gpio_init(void)
{
    // 配置LED引脚 - 两组共阳极RGB LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_RED_PIN) | (1ULL << LED1_GREEN_PIN) | (1ULL << LED1_BLUE_PIN) |
                          (1ULL << LED2_RED_PIN) | (1ULL << LED2_GREEN_PIN) | (1ULL << LED2_BLUE_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 设置LED初始状态 - 共阳极LED，高电平(1)熄灭，低电平(0)点亮
    // LED1组初始状态 - 全部熄灭
    gpio_set_level(LED1_RED_PIN, 1);
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED1_BLUE_PIN, 1);

    // LED2组初始状态 - 全部熄灭
    gpio_set_level(LED2_RED_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    gpio_set_level(LED2_BLUE_PIN, 1);
}
````
</augment_code_snippet>

**LED控制特性**:
- **类型**: 共阳极RGB LED
- **控制逻辑**: 低电平点亮，高电平熄灭
- **电流**: 每个LED最大20mA
- **颜色混合**: 支持RGB颜色混合

### 4. 按键输入接口

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

**按键接口特性**:
- **KEY1 (GPIO0)**: 启动模式选择/用户按键
- **KEY2 (GPIO35)**: 用户按键 (只读引脚)
- **上拉电阻**: 内部上拉 (45kΩ)
- **中断**: 上升沿触发
- **防抖**: 软件防抖处理

## 🚫 受限和保留引脚

### 1. Flash连接引脚 (不可用)

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO6 | SPI_CLK | 连接外部Flash时钟 |
| GPIO7 | SPI_D0 | 连接外部Flash数据0 |
| GPIO8 | SPI_D1 | 连接外部Flash数据1 |
| GPIO9 | SPI_D2 | 连接外部Flash数据2 |
| GPIO10 | SPI_D3 | 连接外部Flash数据3 |
| GPIO11 | SPI_CMD | 连接外部Flash命令 |

### 2. 特殊功能引脚

| GPIO | 功能 | 限制 | 说明 |
|------|------|------|------|
| GPIO0 | BOOT | 启动时需要特定电平 | 低电平进入下载模式 |
| GPIO2 | BOOT | 启动时需要悬空或高电平 | 内置LED连接 |
| GPIO5 | VSPI_CS0 | 启动时需要高电平 | 可用作普通GPIO |
| GPIO12 | MTDI | 启动时影响Flash电压 | 需要低电平启动 |
| GPIO15 | MTDO | 启动时需要高电平 | JTAG调试接口 |

### 3. 只读引脚

| GPIO | 特性 | 说明 |
|------|------|------|
| GPIO34 | INPUT_ONLY | 只能作为输入，无内部上拉 |
| GPIO35 | INPUT_ONLY | 只能作为输入，无内部上拉 |
| GPIO36 | INPUT_ONLY | 只能作为输入，无内部上拉 |
| GPIO39 | INPUT_ONLY | 只能作为输入，无内部上拉 |

## 🔧 预留扩展引脚

### 1. 可用GPIO引脚

| GPIO | 当前状态 | 推荐用途 | 特性 |
|------|----------|----------|------|
| GPIO4 | 预留 | I2C_SDA | 支持内部上拉 |
| GPIO5 | 预留 | I2C_SCL | 支持内部上拉 |
| GPIO18 | 预留 | SPI_CLK | 高速SPI时钟 |
| GPIO19 | 预留 | SPI_MISO | SPI主入从出 |
| GPIO23 | 预留 | SPI_MOSI | SPI主出从入 |
| GPIO32 | 预留 | ADC1_CH4 | 模拟输入 |
| GPIO33 | 预留 | ADC1_CH5 | 模拟输入 |
| GPIO34 | 预留 | ADC1_CH6 | 只读，模拟输入 |
| GPIO36 | 预留 | ADC1_CH0 | 只读，模拟输入 |
| GPIO39 | 预留 | ADC1_CH3 | 只读，模拟输入 |

### 2. 扩展接口建议

#### I2C接口扩展
```c
// I2C配置建议
#define I2C_SDA_PIN    GPIO_NUM_4
#define I2C_SCL_PIN    GPIO_NUM_5
#define I2C_FREQ_HZ    100000  // 100kHz标准模式
```

#### SPI接口扩展
```c
// SPI配置建议
#define SPI_CLK_PIN    GPIO_NUM_18
#define SPI_MISO_PIN   GPIO_NUM_19
#define SPI_MOSI_PIN   GPIO_NUM_23
#define SPI_CS_PIN     GPIO_NUM_5   // 可选片选
```

#### ADC模拟输入
```c
// ADC配置建议
#define ADC_BATTERY    GPIO_NUM_32  // 电池电压检测
#define ADC_CURRENT    GPIO_NUM_33  // 电流检测
#define ADC_TEMP       GPIO_NUM_34  // 温度检测
```

## ⚡ 电气特性

### 1. GPIO电气参数

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|------|--------|--------|--------|------|
| 工作电压 | 3.0 | 3.3 | 3.6 | V |
| 输入高电平 | 2.0 | - | 3.6 | V |
| 输入低电平 | -0.3 | - | 0.8 | V |
| 输出高电平 | 2.4 | 3.3 | - | V |
| 输出低电平 | - | 0 | 0.4 | V |
| 输出电流 | - | - | 40 | mA |
| 输入阻抗 | 1 | - | - | MΩ |

### 2. 上拉/下拉电阻

| 类型 | 阻值范围 | 典型值 | 说明 |
|------|----------|--------|------|
| 内部上拉 | 30-100 | 45 | kΩ |
| 内部下拉 | 30-100 | 45 | kΩ |
| 外部上拉 | 1-10 | 4.7 | kΩ (推荐) |
| 外部下拉 | 1-10 | 4.7 | kΩ (推荐) |

### 3. 频率特性

| 信号类型 | 最大频率 | 说明 |
|----------|----------|------|
| 数字GPIO | 40 MHz | 普通数字信号 |
| SPI时钟 | 80 MHz | 高速SPI通信 |
| I2C时钟 | 1 MHz | 快速模式+ |
| UART | 5 Mbps | 高速串口 |
| PWM | 40 MHz | PWM输出 |

## 🛡️ 使用注意事项

### 1. 启动时引脚状态

- **GPIO0**: 启动时必须为高电平（正常运行）
- **GPIO2**: 启动时必须悬空或高电平
- **GPIO5**: 启动时必须为高电平
- **GPIO12**: 启动时必须为低电平（3.3V Flash）
- **GPIO15**: 启动时必须为高电平

### 2. 电流限制

- 单个GPIO最大输出电流：40mA
- 所有GPIO总输出电流：200mA
- LED驱动建议使用限流电阻
- 大功率负载需要外部驱动电路

### 3. 信号完整性

- 高频信号使用短连线
- 敏感信号远离开关电源
- 差分信号保持等长布线
- 添加适当的去耦电容

### 4. ESD保护

- 输入引脚添加ESD保护器件
- 长线连接使用TVS管保护
- PCB设计考虑ESD放电路径
- 外壳接地提供静电泄放

## 📋 引脚检查清单

### 设计验证清单

- [ ] 所有功能引脚已正确分配
- [ ] 启动引脚状态符合要求
- [ ] 电流需求在安全范围内
- [ ] 信号电平匹配外部器件
- [ ] 预留足够的扩展引脚
- [ ] 特殊功能引脚使用正确
- [ ] ESD保护措施到位
- [ ] PCB布线符合信号完整性要求

### 调试验证清单

- [ ] 上电后各引脚电平正确
- [ ] 通信接口工作正常
- [ ] LED指示功能正常
- [ ] 按键响应正确
- [ ] 扩展接口预留正确
- [ ] 无意外的引脚冲突
- [ ] 电气特性符合规范

---

💡 **提示**: 引脚分配是硬件设计的基础，修改引脚定义时需要同步更新软件配置和PCB设计！
