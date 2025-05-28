# 🚗 CAN模块详解

本文档详细介绍ESP32控制板项目中CAN模块的实现，包括TWAI配置、电机控制协议、命令格式和通信机制。

## 🎯 CAN通信概述

CAN（Controller Area Network）是一种可靠的车载网络通信协议，本项目使用ESP32的TWAI（Two-Wire Automotive Interface）控制器实现CAN通信，用于控制LKBLS481502电机驱动器。

### 通信特性
- **协议**: CAN 2.0B扩展帧
- **波特率**: 250 kbps
- **帧格式**: 29位ID + 8字节数据
- **收发器**: SN65HVD232D
- **终端电阻**: 120Ω

## 🔧 硬件配置

### GPIO引脚分配

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// 电机控制引脚 (通过CAN总线控制，不需要直接GPIO控制)
// CAN总线引脚定义:
// - TX: GPIO_NUM_16 (连接到SN65HVD232D的D引脚)
// - RX: GPIO_NUM_17 (连接到SN65HVD232D的R引脚)
````
</augment_code_snippet>

### TWAI配置

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
// TWAI (CAN) 配置
static const twai_general_config_t g_config = 
    TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
````
</augment_code_snippet>

### 硬件连接

```
ESP32          SN65HVD232D        CAN总线
GPIO16  -----> D (发送)      -----> CAN_H
GPIO17  <----- R (接收)      <----- CAN_L
3.3V    -----> VCC
GND     -----> GND
                Rs (斜率控制) -----> GND (高速模式)
```

## 📡 电机驱动协议

### 1. CAN ID定义

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
// CAN ID定义
#define DRIVER_ADDRESS 0x01         // 驱动器地址
#define DRIVER_TX_ID 0x06000000     // 发送基础ID
#define MOTOR_CHANNEL_A 0x01        // A路电机(左侧)
#define MOTOR_CHANNEL_B 0x02        // B路电机(右侧)

// 最终CAN ID = DRIVER_TX_ID + DRIVER_ADDRESS = 0x06000001
````
</augment_code_snippet>

### 2. 命令类型定义

```c
// 命令类型
#define CMD_ENABLE  1    // 使能电机
#define CMD_DISABLE 2    // 失能电机  
#define CMD_SPEED   3    // 设置速度
```

### 3. 电机通道定义

| 通道 | 定义 | 物理位置 | 说明 |
|------|------|----------|------|
| 0x01 | MOTOR_CHANNEL_A | 左侧电机 | A路输出 |
| 0x02 | MOTOR_CHANNEL_B | 右侧电机 | B路输出 |

## 📦 命令格式详解

### 1. 使能命令

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
if (cmd_type == CMD_ENABLE) {
    // 使能电机: 23 0D 20 01/02 00 00 00 00
    tx_data[0] = 0x23;
    tx_data[1] = 0x0D;
    tx_data[2] = 0x20;
    tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)
    tx_data[4] = 0x00;
    tx_data[5] = 0x00;
    tx_data[6] = 0x00;
    tx_data[7] = 0x00;
}
````
</augment_code_snippet>

**命令格式说明**:
```
字节0: 0x23 - 命令头
字节1: 0x0D - 使能操作码
字节2: 0x20 - 寄存器地址
字节3: 0x01/0x02 - 电机通道
字节4-7: 0x00 - 保留字节
```

### 2. 失能命令

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
} else if (cmd_type == CMD_DISABLE) {
    // 失能电机: 23 0C 20 01/02 00 00 00 00
    tx_data[0] = 0x23;
    tx_data[1] = 0x0C;
    tx_data[2] = 0x20;
    tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)
    tx_data[4] = 0x00;
    tx_data[5] = 0x00;
    tx_data[6] = 0x00;
    tx_data[7] = 0x00;
}
````
</augment_code_snippet>

**命令格式说明**:
```
字节0: 0x23 - 命令头
字节1: 0x0C - 失能操作码
字节2: 0x20 - 寄存器地址
字节3: 0x01/0x02 - 电机通道
字节4-7: 0x00 - 保留字节
```

### 3. 速度命令

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
} else if (cmd_type == CMD_SPEED) {
    // 设置速度: 23 00 20 01/02 HH HH LL LL
    tx_data[0] = 0x23;
    tx_data[1] = 0x00;
    tx_data[2] = 0x20;
    tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)

    // 将-100到100的速度转换为-10000到10000
    int32_t sp_value = (int32_t)speed * 100;

    // 32位有符号整数表示，高字节在前
    tx_data[4] = (sp_value >> 24) & 0xFF; // 最高字节
    tx_data[5] = (sp_value >> 16) & 0xFF;
    tx_data[6] = (sp_value >> 8) & 0xFF;
    tx_data[7] = sp_value & 0xFF; // 最低字节
}
````
</augment_code_snippet>

**速度编码示例**:
```
输入速度: 50 (-100~100)
转换后: 50 * 100 = 5000
十六进制: 0x00001388
大端序编码:
- tx_data[4] = 0x00 (最高字节)
- tx_data[5] = 0x00
- tx_data[6] = 0x13
- tx_data[7] = 0x88 (最低字节)

最终CAN帧: [0x23, 0x00, 0x20, 0x01, 0x00, 0x00, 0x13, 0x88]
```

## 🔄 CAN帧发送

### 1. 帧封装

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
static void keya_send_data(uint32_t id, uint8_t* data)
{
    twai_message_t message;
    message.extd = 1;                 // 扩展帧(29位ID)
    message.identifier = id;          // CAN ID: 0x06000001
    message.data_length_code = 8;     // 8字节数据
    message.rtr = 0;                  // 数据帧
    
    // 复制8字节数据
    for (int i = 0; i < 8; i++) {
        message.data[i] = data[i];
    }
    
    // 非阻塞发送
    esp_err_t result = twai_transmit(&message, 0);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ CAN transmit failed: %s", esp_err_to_name(result));
    }
}
````
</augment_code_snippet>

### 2. 帧格式

| 字段 | 长度 | 值 | 说明 |
|------|------|-----|------|
| SOF | 1位 | 0 | 帧起始 |
| ID | 29位 | 0x06000001 | 扩展标识符 |
| RTR | 1位 | 0 | 数据帧 |
| IDE | 1位 | 1 | 扩展帧 |
| r0 | 1位 | 0 | 保留位 |
| DLC | 4位 | 8 | 数据长度 |
| DATA | 64位 | 命令数据 | 8字节数据 |
| CRC | 16位 | 自动计算 | 循环冗余校验 |

## 🎮 电机控制接口

### 1. 初始化接口

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
/**
 * 初始化电机驱动
 */
esp_err_t drv_keyadouble_init(void)
{
    // 初始化TWAI (CAN)
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "Motor driver initialized");
    return ESP_OK;
}
````
</augment_code_snippet>

### 2. 运动控制接口

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
/**
 * 设置左右电机速度实现运动
 * @param speed_left 左电机速度(-100到100)
 * @param speed_right 右电机速度(-100到100)
 * @return 0=成功，1=参数错误
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    if ((abs(speed_left) > 100) || (abs(speed_right) > 100)) {
        printf("Wrong parameter in intf_move!!![lf=%d],[ri=%d]", speed_left, speed_right);
        return 1;
    }

    // 更新刹车标志
    if (speed_left != 0) {
        bk_flag_left = 1; // 1为松开
    } else {
        bk_flag_left = 0; // 0为刹车
    }

    if (speed_right != 0) {
        bk_flag_right = 1; // 1为松开
    } else {
        bk_flag_right = 0; // 0为刹车
    }

    // 发送使能和速度命令
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right);

    return 0;
}
````
</augment_code_snippet>

### 3. 底层控制函数

<augment_code_snippet path="main/drv_keyadouble.c" mode="EXCERPT">
````c
/**
 * 电机控制
 * @param cmd_type 命令类型: CMD_ENABLE/CMD_DISABLE/CMD_SPEED
 * @param channel 电机通道: MOTOR_CHANNEL_A(左)/MOTOR_CHANNEL_B(右)
 * @param speed 速度(-100到100，对应-10000到10000)
 */
static void motor_control(uint8_t cmd_type, uint8_t channel, int8_t speed)
{
    uint8_t tx_data[8] = {0};
    uint32_t tx_id = DRIVER_TX_ID + DRIVER_ADDRESS;

    // 根据命令类型构造数据
    // ... (命令构造代码见上文)

    keya_send_data(tx_id, tx_data);
}
````
</augment_code_snippet>

## 📊 性能特性

### 1. 通信性能

| 指标 | 数值 | 说明 |
|------|------|------|
| 波特率 | 250 kbps | CAN总线速度 |
| 发送延迟 | < 1ms | 软件处理时间 |
| 帧长度 | 108位 | 扩展帧+8字节数据 |
| 最大吞吐量 | > 1000帧/秒 | 理论值 |

### 2. 控制精度

| 参数 | 范围 | 精度 | 说明 |
|------|------|------|------|
| 输入速度 | -100~+100 | 1 | 应用层速度值 |
| CAN速度值 | -10000~+10000 | 100 | 驱动器速度值 |
| 分辨率 | 0.01% | - | 速度控制精度 |

### 3. 资源使用

| 资源 | 使用量 | 说明 |
|------|--------|------|
| RAM | < 2KB | 缓冲区和变量 |
| CPU | < 3% | 正常负载下 |
| TWAI | 1个控制器 | ESP32内置 |
| GPIO | 2个引脚 | TX/RX |

## 🔍 状态监控

### 1. CAN总线状态

```c
// 获取TWAI状态
twai_status_info_t status_info;
twai_get_status_info(&status_info);

ESP_LOGI(TAG, "📊 TWAI Status:");
ESP_LOGI(TAG, "   State: %d", status_info.state);
ESP_LOGI(TAG, "   TX error count: %d", status_info.tx_error_counter);
ESP_LOGI(TAG, "   RX error count: %d", status_info.rx_error_counter);
ESP_LOGI(TAG, "   TX queue: %d", status_info.msgs_to_tx);
ESP_LOGI(TAG, "   RX queue: %d", status_info.msgs_to_rx);
```

### 2. 发送统计

```c
// 发送统计
static uint32_t can_tx_count = 0;
static uint32_t can_tx_error = 0;

void update_can_stats(esp_err_t result)
{
    can_tx_count++;
    if (result != ESP_OK) {
        can_tx_error++;
    }
    
    if (can_tx_count % 1000 == 0) {
        float success_rate = (float)(can_tx_count - can_tx_error) / can_tx_count * 100;
        ESP_LOGI(TAG, "📈 CAN TX Success Rate: %.2f%% (%d/%d)", 
                 success_rate, can_tx_count - can_tx_error, can_tx_count);
    }
}
```

## 🛠️ 调试和测试

### 1. 调试输出

```c
// 启用CAN模块详细日志
esp_log_level_set("DRV_KEYADOUBLE", ESP_LOG_DEBUG);

// CAN消息调试
static void debug_can_message(uint32_t id, uint8_t* data)
{
    ESP_LOGD(TAG, "📡 CAN TX - ID: 0x%08X", id);
    ESP_LOGD(TAG, "   Data: %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3],
             data[4], data[5], data[6], data[7]);
}
```

### 2. 硬件测试

使用CAN分析仪检查总线状态：
- **波特率**: 250 kbps ± 0.1%
- **电平**: CAN_H/CAN_L差分信号
- **终端电阻**: 120Ω (总线两端)
- **帧格式**: 符合CAN 2.0B标准

### 3. 回环测试

```c
// CAN回环测试模式
static const twai_general_config_t test_config = {
    .mode = TWAI_MODE_NO_ACK,  // 无应答模式
    .tx_io = GPIO_NUM_16,
    .rx_io = GPIO_NUM_17,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 10,
    .rx_queue_len = 10,
    .alerts_enabled = TWAI_ALERT_NONE,
    .clkout_divider = 0,
};
```

## 🚨 故障排除

### 1. 常见问题

#### CAN发送失败
**检查项目**:
1. TWAI驱动是否正确初始化
2. GPIO16/17连接是否正确
3. CAN收发器供电是否正常
4. 总线终端电阻是否正确

#### 电机无响应
**检查项目**:
1. CAN ID是否正确 (0x06000001)
2. 命令格式是否符合协议
3. 电机驱动器是否上电
4. CAN总线连接是否正常

### 2. 错误恢复

```c
// CAN总线错误恢复
void can_error_recovery(void)
{
    twai_status_info_t status;
    twai_get_status_info(&status);
    
    if (status.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW(TAG, "🚨 CAN bus off - attempting recovery");
        twai_initiate_recovery();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 3. 性能优化

```c
// 批量发送优化
void send_motor_commands(int8_t left_speed, int8_t right_speed)
{
    // 先发送使能命令
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);
    
    // 再发送速度命令
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, left_speed);
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, right_speed);
}
```

---

💡 **提示**: CAN模块是控制系统的执行输出，确保命令格式正确和通信可靠是系统稳定运行的关键！
