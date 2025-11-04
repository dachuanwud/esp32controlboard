# SBUS 接收 → CAN 发送完整数据流文档

**文档版本**: v1.0
**最后更新**: 2025-11-02
**作者**: ESP32 Control Board Team

---

## 📋 目录

1. [系统概述](#系统概述)
2. [SBUS协议规范](#sbus协议规范)
3. [完整数据流路径](#完整数据流路径)
4. [各模块详细说明](#各模块详细说明)
5. [数据结构定义](#数据结构定义)
6. [时序图](#时序图)
7. [代码位置索引](#代码位置索引)
8. [调试指南](#调试指南)

---

## 系统概述

### 功能描述

本系统实现从**SBUS遥控接收机**接收16通道遥控信号，经过解析和处理后，通过**CAN总线**发送电机控制命令到**双路电机驱动器**，实现履带车的差速控制。

### 系统架构

```
┌─────────────────┐
│   SBUS接收机    │
│   (遥控器信号)  │
└────────┬────────┘
         │ SBUS信号 (GPIO22)
         │ 100000bps, 8E2, 反相
         ▼
┌─────────────────────────────────┐
│      ESP32 控制板               │
│  ┌──────────────────────────┐  │
│  │  UART2接收 (sbus.c)       │  │
│  │  → SBUS解析               │  │
│  │  → 通道映射               │  │
│  │  → 差速计算               │  │
│  │  → CAN发送                │  │
│  └──────────────────────────┘  │
└────────┬────────────────────────┘
         │ CAN总线 (GPIO16/17)
         │ 250Kbps, 扩展帧
         ▼
┌─────────────────┐
│  电机驱动器     │
│  (CAN接收)      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  左/右电机      │
└─────────────────┘
```

---

## SBUS协议规范

### SBUS帧格式

SBUS使用**25字节**固定长度帧，格式如下：

```
字节位置    内容              说明
─────────────────────────────────────────────
Byte[0]     0x0F             帧头标识
Byte[1-22]  数据字节         包含16个通道的11位数据
Byte[23]    标志字节         通道17-18, 失锁标志等
Byte[24]    0x00             帧尾标识
```

### 数据编码

- **波特率**: 100000 bps
- **数据位**: 8位
- **校验位**: 偶校验 (Even)
- **停止位**: 2位
- **信号**: 反相逻辑 (需要硬件反相器或软件反相)

### 通道数据格式

每个通道占用**11位**，范围 **0-2047**，映射到标准PWM范围 **1050-1950**：

```
原始值范围:  282 ~ 1722 (SBUS原始范围)
映射后范围:  1050 ~ 1950 (标准PWM范围)
中位值:      1500 (对应遥控器中位)
```

### 通道位分配

16个通道的数据被打包在22个字节中：

```
通道0:  Byte[1]的低3位 + Byte[2]的8位        = 11位
通道1:  Byte[2]的高3位 + Byte[3]的低5位      = 11位
通道2:  Byte[3]的高6位 + Byte[4]的2位 + Byte[5]的3位 = 11位
...
通道15: Byte[21]的高5位 + Byte[22]的低3位    = 11位
```

---

## 完整数据流路径

### 流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                      数据流完整路径                              │
└─────────────────────────────────────────────────────────────────┘

步骤1: SBUS信号接收
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[硬件层] SBUS接收机 → GPIO22 (UART2_RX)
         ↓
[驱动层] UART2中断 → 数据存入UART缓冲区
         ↓
[任务层] sbus_uart_task() 循环检测UART事件队列
         ↓
         [函数] sbus.c::sbus_uart_task()
         [位置] main/sbus.c:18-144
         [功能] 接收UART字节，组装完整SBUS帧
         [输出] 完整25字节SBUS帧存入 g_sbus_rx_buf[]

步骤2: SBUS帧解析
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[任务层] sbus_process_task() 循环调用
         ↓
         [函数] sbus.c::sbus_get_data()
         [位置] main/sbus.c:304-313
         [功能] 检查是否有完整帧 (g_sbus_pt & 0x80)
         ↓
         [函数] sbus.c::parse_sbus_msg()
         [位置] main/sbus.c:197-299
         [功能] 解析25字节SBUS帧，提取16个通道值
         [算法] 位操作提取11位通道数据
         [映射] 282-1722 → 1050-1950
         [输出] uint16_t channel[16]

步骤3: 队列传输
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[任务层] sbus_process_task() 继续处理
         ↓
         [数据结构] sbus_data_t { uint16_t channel[16]; }
         [队列] sbus_queue (FreeRTOS Queue, 容量20)
         [操作] xQueueSend(sbus_queue, &sbus_data, 0)
         [位置] main/main.c:298-305
         [功能] 将通道数据放入队列，供电机控制任务使用

步骤4: 通道值解析
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[任务层] motor_control_task() 循环接收队列数据
         ↓
         [操作] xQueueReceive(sbus_queue, &sbus_data, 0)
         [位置] main/main.c:405-416
         ↓
         [函数] channel_parse.c::parse_chan_val()
         [位置] main/channel_parse.c:109-214
         [功能] 解析通道值，计算电机速度
         [输入] uint16_t channel[16] (1050-1950范围)
         [处理]
           - 通道值映射到速度 (-100 ~ +100)
           - 检查模式开关 (通道6/7)
           - 计算差速转弯速度
         [输出] int8_t left_speed, int8_t right_speed

步骤5: 电机控制
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[函数] channel_parse.c::parse_chan_val() 继续
         ↓
         [函数] drv_keyadouble.c::intf_move_keyadouble()
         [位置] main/drv_keyadouble.c:166-201
         [功能] 设置左右电机速度
         [输入] int8_t speed_left (-100 ~ +100)
                int8_t speed_right (-100 ~ +100)
         [处理]
           - 更新刹车标志 (bk_flag_left/right)
           - 首次调用时发送使能命令
           - 调用 motor_control() 发送速度命令

步骤6: CAN数据封装
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[函数] drv_keyadouble.c::motor_control()
         [位置] main/drv_keyadouble.c:99-142
         [功能] 封装CAN消息
         [CAN ID] 0x06000001 (驱动器地址0x01)
         [数据格式] 8字节
           Byte[0]: 0x23 (命令头)
           Byte[1]: 0x00 (速度命令)
           Byte[2]: 0x20 (子命令)
           Byte[3]: 0x01/0x02 (通道A/B)
           Byte[4-7]: 速度值 (32位有符号整数，-10000~+10000)

步骤7: CAN发送
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[函数] drv_keyadouble.c::keya_send_data()
         [位置] main/drv_keyadouble.c:43-91
         [功能] 通过TWAI (CAN) 驱动发送数据
         [配置]
           - GPIO16: CAN_TX
           - GPIO17: CAN_RX
           - 波特率: 250Kbps
           - 扩展帧: 29位ID
         [操作] twai_transmit(&message, 0) (非阻塞)
         [输出] CAN总线信号 → 电机驱动器
```

---

## 各模块详细说明

### 1. SBUS接收模块 (sbus.c)

#### 初始化流程

```c
// 位置: main/sbus.c:149-190
esp_err_t sbus_init(void)
{
    // 1. 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = 100000,        // SBUS标准波特率
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN, // 偶校验
        .stop_bits = UART_STOP_BITS_2, // 2停止位
    };

    // 2. 安装UART驱动 (缓冲区1024字节)
    uart_driver_install(UART_SBUS, 1024, 0, 50, &sbus_uart_queue, 0);

    // 3. 配置GPIO引脚
    uart_set_pin(UART_SBUS, UART_PIN_NO_CHANGE, GPIO_NUM_22, ...);

    // 4. 启用信号反相 (SBUS使用反相逻辑)
    uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV);

    // 5. 创建UART接收任务
    xTaskCreate(sbus_uart_task, "sbus_uart_task", 4096, NULL, 12, NULL);
}
```

#### 接收任务逻辑

```c
// 位置: main/sbus.c:18-144
static void sbus_uart_task(void *pvParameters)
{
    while (1) {
        // 1. 等待UART事件
        if (xQueueReceive(sbus_uart_queue, &event, 10ms)) {
            if (event.type == UART_DATA) {
                // 2. 读取UART数据 (批量读取64字节)
                int len = uart_read_bytes(UART_SBUS, temp_buffer, 64, 10ms);

                // 3. 逐字节处理
                for (int i = 0; i < len; i++) {
                    data = temp_buffer[i];

                    // 4. 检查帧头 (Byte[0] = 0x0F)
                    if (g_sbus_pt == 1 && data != 0x0F) {
                        g_sbus_pt--; // 回退，重新等待
                    }

                    // 5. 存入缓冲区
                    g_sbus_rx_buf[g_sbus_pt++] = data;

                    // 6. 检查帧尾 (Byte[24] = 0x00)
                    if (g_sbus_pt == 25 && data == 0x00) {
                        g_sbus_pt |= 0x80; // 标记完整帧接收完成
                    }
                }
            }
        }

        // 短暂延时，避免过度占用CPU
        vTaskDelay(1ms);
    }
}
```

#### 帧解析逻辑

```c
// 位置: main/sbus.c:197-299
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel)
{
    // 1. 提取原始11位通道值 (0-2047范围)
    raw_channel[0] = (sbus_data[1] >> 0 | sbus_data[2] << 8) & 0x07FF;
    raw_channel[1] = (sbus_data[2] >> 3 | sbus_data[3] << 5) & 0x07FF;
    // ... 共16个通道

    // 2. 映射到标准PWM范围 (1050-1950)
    for (int i = 0; i < 16; i++) {
        channel[i] = (raw_channel[i] - 282) * 5 / 8 + 1050;
    }

    return 0;
}
```

### 2. 通道解析模块 (channel_parse.c)

#### 通道值映射

```c
// 位置: main/channel_parse.c:30-35
static int8_t chg_val(uint16_t val)
{
    // 输入: 1050~1950 (SBUS标准PWM范围)
    // 输出: -100~+100 (电机速度范围)
    // 算法: (val - 1500) / 9 * 2
    // 1500 → 0 (中位值)
    // 1050 → -100 (最小值)
    // 1950 → +100 (最大值)
    return (((int16_t)val - 1500) / 9 * 2) & 0xff;
}
```

#### 差速控制算法

```c
// 位置: main/channel_parse.c:109-214
uint8_t parse_chan_val(uint16_t* ch_val)
{
    // 1. 提取前后和左右分量
    int8_t sp_fb = chg_val(ch_val[2]); // 通道2: 前后
    int8_t sp_lr = chg_val(ch_val[0]);  // 通道0: 左右

    // 2. 检查模式开关
    bool single_hand = (ch_val[6] == 1950); // 通道6: 单手模式
    bool low_speed = (ch_val[7] == 1950);   // 通道7: 低速模式

    if (single_hand) {
        sp_lr = chg_val(ch_val[3]); // 使用通道3作为左右控制
    }

    if (low_speed) {
        sp_fb /= 2; // 速度减半
        sp_lr /= 2;
    }

    // 3. 计算差速转弯速度
    if (sp_fb == 0) {
        // 原地转向
        left_speed = sp_lr;
        right_speed = -sp_lr;
    } else if (sp_lr == 0) {
        // 直线前进/后退
        left_speed = sp_fb;
        right_speed = sp_fb;
    } else if (sp_lr > 0) {
        // 右转: 左轮保持，右轮减速
        left_speed = sp_fb;
        right_speed = cal_offset(sp_fb, sp_lr);
    } else {
        // 左转: 右轮保持，左轮减速
        left_speed = cal_offset(sp_fb, sp_lr);
        right_speed = sp_fb;
    }

    // 4. 调用电机控制
    intf_move(left_speed, right_speed);
}
```

### 3. CAN发送模块 (drv_keyadouble.c)

#### CAN初始化

```c
// 位置: main/drv_keyadouble.c:147-158
esp_err_t drv_keyadouble_init(void)
{
    // 1. CAN配置
    static const twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
    static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // 2. 安装并启动CAN驱动
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();

    // 3. 初始化电机使能标志
    motor_enabled = false;
}
```

#### 电机控制函数

```c
// 位置: main/drv_keyadouble.c:166-201
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // 1. 参数验证
    if (abs(speed_left) > 100 || abs(speed_right) > 100) {
        return 1; // 错误
    }

    // 2. 更新刹车标志
    bk_flag_left = (speed_left != 0) ? 1 : 0;
    bk_flag_right = (speed_right != 0) ? 1 : 0;

    // 3. 首次调用时发送使能命令
    if (!motor_enabled) {
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);
        motor_enabled = true;
    }

    // 4. 发送速度命令
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right);

    return 0;
}
```

#### CAN消息封装

```c
// 位置: main/drv_keyadouble.c:99-142
static void motor_control(uint8_t cmd_type, uint8_t channel, int8_t speed)
{
    uint8_t tx_data[8] = {0};
    uint32_t tx_id = 0x06000000 + 0x01; // 基础ID + 驱动器地址

    if (cmd_type == CMD_SPEED) {
        // 速度命令格式: 23 00 20 01/02 HH HH LL LL
        tx_data[0] = 0x23; // 命令头
        tx_data[1] = 0x00; // 速度命令
        tx_data[2] = 0x20; // 子命令
        tx_data[3] = channel; // 0x01=A路(左), 0x02=B路(右)

        // 速度值: -100 ~ +100 转换为 -10000 ~ +10000
        int32_t sp_value = (int32_t)speed * 100;

        // 32位有符号整数，大端序
        tx_data[4] = (sp_value >> 24) & 0xFF;
        tx_data[5] = (sp_value >> 16) & 0xFF;
        tx_data[6] = (sp_value >> 8) & 0xFF;
        tx_data[7] = sp_value & 0xFF;
    }

    // 发送CAN消息
    keya_send_data(tx_id, tx_data);
}
```

#### CAN发送函数

```c
// 位置: main/drv_keyadouble.c:43-91
static void keya_send_data(uint32_t id, uint8_t* data)
{
    twai_message_t message;
    message.extd = 1;              // 扩展帧 (29位ID)
    message.identifier = id;
    message.data_length_code = 8;   // 8字节数据
    message.rtr = 0;               // 数据帧

    // 复制数据
    memcpy(message.data, data, 8);

    // 非阻塞发送 (超时=0)
    esp_err_t result = twai_transmit(&message, 0);

    if (result != ESP_OK) {
        // 错误处理
        ESP_LOGW(TAG, "CAN send error: %s", esp_err_to_name(result));
    }
}
```

---

## 数据结构定义

### SBUS数据结构

```c
// SBUS原始帧 (25字节)
typedef struct {
    uint8_t header;           // Byte[0]: 0x0F
    uint8_t data[22];         // Byte[1-22]: 通道数据
    uint8_t flags;            // Byte[23]: 标志位
    uint8_t footer;           // Byte[24]: 0x00
} sbus_raw_frame_t;

// SBUS通道数据 (16通道)
typedef struct {
    uint16_t channel[16];     // 每个通道: 1050~1950
} sbus_data_t;
```

### CAN消息结构

```c
// CAN扩展帧消息
typedef struct {
    uint32_t identifier;      // 29位扩展ID
    uint8_t data_length_code; // 数据长度 (8字节)
    uint8_t data[8];          // 数据内容
} can_message_t;

// 电机速度命令格式
typedef struct {
    uint8_t cmd_header;       // 0x23
    uint8_t cmd_type;         // 0x00 (速度命令)
    uint8_t sub_cmd;          // 0x20
    uint8_t channel;          // 0x01/0x02 (A/B路)
    int32_t speed_value;      // -10000 ~ +10000
} motor_speed_cmd_t;
```

---

## 时序图

### 完整时序流程

```
时间轴 →
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SBUS接收机     UART2        sbus_uart_task    sbus_process_task
     │          │                │                    │
     │--SBUS---->│                │                    │
     │  信号     │                │                    │
     │          │--UART事件--->│                    │
     │          │  队列          │                    │
     │          │                │--读取字节-------->│
     │          │                │                    │
     │          │                │--组装完整帧-------->│
     │          │                │                    │
     │          │                │                    │--解析SBUS帧
     │          │                │                    │--提取16通道
     │          │                │                    │
     │          │                │                    │--放入队列
     │          │                │                    │
     │          │                │                    │
motor_control_task   channel_parse    drv_keyadouble      CAN总线
     │                    │                │                │
     │<--从队列读取--------│                │                │
     │                    │                │                │
     │--调用parse_chan_val->│                │                │
     │                    │--解析通道值---->│                │
     │                    │--计算速度------>│                │
     │                    │                │                │
     │                    │--调用intf_move->│                │
     │                    │                │--封装CAN消息-->│
     │                    │                │                │
     │                    │                │--发送CAN帧----->│
     │                    │                │                │
     │                    │                │                │--CAN信号
     │                    │                │                │  到驱动器
```

### 时序性能指标

```
操作步骤                  延迟时间    说明
───────────────────────────────────────────────────────────
SBUS信号接收               <1ms      UART硬件缓冲
UART字节处理               1ms       任务循环延迟
SBUS帧组装                 14-20ms   SBUS帧周期(50-70ms)
帧解析                      <1ms      位操作和映射
队列传输                    <1ms      内存拷贝
通道解析                    <1ms      算法计算
CAN封装                     <1ms      数据结构填充
CAN发送                     <1ms      非阻塞发送
───────────────────────────────────────────────────────────
总延迟                     3-5ms     端到端延迟
```

---

## 代码位置索引

### 关键函数位置

| 功能模块 | 函数名 | 文件位置 | 行号 |
|---------|--------|---------|------|
| **SBUS初始化** | `sbus_init()` | `main/sbus.c` | 149-190 |
| **SBUS接收任务** | `sbus_uart_task()` | `main/sbus.c` | 18-144 |
| **SBUS帧解析** | `parse_sbus_msg()` | `main/sbus.c` | 197-299 |
| **获取SBUS数据** | `sbus_get_data()` | `main/sbus.c` | 304-313 |
| **SBUS处理任务** | `sbus_process_task()` | `main/main.c` | 275-312 |
| **通道值解析** | `parse_chan_val()` | `main/channel_parse.c` | 109-214 |
| **通道值映射** | `chg_val()` | `main/channel_parse.c` | 30-35 |
| **差速计算** | `cal_offset()` | `main/channel_parse.c` | 83-95 |
| **电机控制** | `intf_move_keyadouble()` | `main/drv_keyadouble.c` | 166-201 |
| **CAN电机控制** | `motor_control()` | `main/drv_keyadouble.c` | 99-142 |
| **CAN发送** | `keya_send_data()` | `main/drv_keyadouble.c` | 43-91 |
| **CAN初始化** | `drv_keyadouble_init()` | `main/drv_keyadouble.c` | 147-158 |
| **电机控制任务** | `motor_control_task()` | `main/main.c` | 377-422 |

### 配置常量位置

| 配置项 | 定义位置 | 说明 |
|--------|---------|------|
| `UART_SBUS` | `main/main.h:80` | UART2编号 |
| `GPIO_NUM_22` | `main/sbus.c:175` | SBUS接收引脚 |
| `LEN_SBUS` | `main/main.h:83` | SBUS帧长度(25) |
| `LEN_CHANEL` | `main/main.h:84` | 通道数量(12) |
| `DRIVER_TX_ID` | `main/drv_keyadouble.c:11` | CAN发送ID |
| `GPIO_NUM_16/17` | `main/drv_keyadouble.c:34` | CAN TX/RX引脚 |

---

## 调试指南

### 1. SBUS接收调试

#### 启用原始数据打印
```c
// 在 main.h 中启用
#define ENABLE_SBUS_RAW_DATA    1
#define ENABLE_SBUS_DEBUG       1
#define ENABLE_SBUS_FRAME_INFO  1
```

#### 调试输出示例
```
[SBUS] 📥 接收到 25 字节原始数据
[SBUS]    [0] 0x0F (15)
[SBUS]    [1] 0x12 (18)
...
[SBUS]    [24] 0x00 (0)
[SBUS] ✅ 检测到SBUS帧头: 0x0F
[SBUS] ✅ 检测到SBUS帧尾: 0x00，完整帧接收完成
[SBUS] 🎮 SBUS帧#123 - 关键通道: CH0:1500 CH1:1500 CH2:1500 ...
```

### 2. 通道解析调试

#### 启用通道变化检测
```c
// 已在 channel_parse.c 中实现
// 当通道值变化超过阈值时打印日志
```

#### 调试输出示例
```
[CHAN_PARSE] 🚀 First run - initializing track vehicle control
[CHAN_PARSE] 🎯 Control values - FB:50 LR:0
[CHAN_PARSE] ⬆️ FORWARD STRAIGHT - Speed:50
```

### 3. CAN发送调试

#### 启用CAN调试日志
```c
// 默认已启用 ESP_LOGD 级别日志
// 可通过 menuconfig 调整日志级别
```

#### 调试输出示例
```
[DRV_KEYA] CAN TX: 06000001 [23 00 20 01 00 00 13 88]
[DRV_KEYA] Motor Ch1 speed: 50
[DRV_KEYA] CAN TX: 06000001 [23 00 20 02 00 00 13 88]
[DRV_KEYA] Motor Ch2 speed: 50
```

### 4. 性能监控

#### 添加性能计时
```c
// 在关键位置添加时间戳
uint32_t start_time = xTaskGetTickCount();
// ... 处理代码 ...
uint32_t end_time = xTaskGetTickCount();
ESP_LOGI(TAG, "处理耗时: %lu ms", end_time - start_time);
```

#### 监控队列状态
```c
// 检查队列使用情况
UBaseType_t queue_messages = uxQueueMessagesWaiting(sbus_queue);
ESP_LOGI(TAG, "SBUS队列消息数: %d", queue_messages);
```

### 5. 常见问题排查

#### 问题1: SBUS无数据接收
- ✅ 检查GPIO22连接
- ✅ 检查UART配置 (100000bps, 8E2)
- ✅ 检查信号反相设置
- ✅ 检查SBUS接收机供电

#### 问题2: CAN发送失败
- ✅ 检查GPIO16/17连接
- ✅ 检查CAN总线终端电阻
- ✅ 检查CAN波特率 (250Kbps)
- ✅ 检查驱动器地址配置

#### 问题3: 控制响应慢
- ✅ 检查任务优先级 (SBUS:12, 电机:10)
- ✅ 检查队列是否阻塞
- ✅ 检查日志输出频率
- ✅ 检查CAN发送延迟

---

## 附录

### A. SBUS通道位操作详解

```c
// 通道0: Byte[1]的低3位 + Byte[2]的8位 = 11位
raw_channel[0] = (sbus_data[1] >> 0 | sbus_data[2] << 8) & 0x07FF;

// 位操作分解:
// sbus_data[1] = 0bABCDEFGH (8位)
// sbus_data[2] = 0bIJKLMNOP (8位)
//
// sbus_data[1] >> 0 = 0b00000000ABCDEFGH (取低8位)
// sbus_data[2] << 8 = 0bIJKLMNOP00000000 (左移8位)
//
// 组合: 0bIJKLMNOPABCDEFGH (16位)
// 掩码: 0x07FF = 0b0000011111111111 (取低11位)
// 结果: 0b00000IJKLMNOPABCDEFGH (11位)
```

### B. CAN消息格式详解

```c
// 速度命令示例: 左电机速度 +50
// CAN ID: 0x06000001
// 数据: [23 00 20 01 00 00 13 88]

Byte[0] = 0x23      // 命令头
Byte[1] = 0x00      // 速度命令类型
Byte[2] = 0x20      // 子命令
Byte[3] = 0x01      // 通道A (左侧电机)
Byte[4] = 0x00      // 速度值高字节
Byte[5] = 0x00      //
Byte[6] = 0x13      //
Byte[7] = 0x88      // 速度值低字节 (0x00001388 = 5000)

// 速度值转换: 5000 / 100 = 50 (速度值)
```

### C. 差速转弯算法详解

```c
// 前进+右转示例
sp_fb = 50  // 前进速度
sp_lr = 20  // 右转分量

// 计算过程:
left_speed = sp_fb = 50              // 左轮保持前进速度
right_speed = cal_offset(50, 20)    // 右轮减速

// cal_offset() 计算:
// abs(50) > abs(20) → 继续
// v1 > 0 → return abs(50) - abs(20) = 30

// 结果:
left_speed = 50
right_speed = 30
// 左轮快，右轮慢 → 右转
```

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2025-11-02 | 初始版本，完整数据流文档 |

---

**文档结束**

如有问题或需要补充，请联系开发团队。
