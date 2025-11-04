# 刹车定时器功能说明

## 🔍 刹车定时器概述

刹车定时器是一个**安全保护机制**，用于在系统异常情况下自动触发紧急刹车，防止车辆失控。

---

## 🎯 设计目的

### 1. **通信中断保护**

当遥控器信号丢失或通信故障时，系统无法接收到新的速度命令。如果此时车辆仍在运动，可能造成危险。

### 2. **失联自动刹车**

刹车定时器监测电机控制状态，如果连续5秒未收到有效的速度命令（速度保持为0），说明：
- 可能通信中断
- 可能控制程序异常
- 可能设备故障

此时自动触发紧急刹车。

---

## 🔧 工作原理

### 工作流程

```
1. 系统启动
   └─> 创建左右两个刹车定时器（5秒超时）
   └─> 启动定时器

2. 电机控制循环
   ├─> 收到速度命令（speed_left/right != 0）
   │   └─> 设置 bk_flag = 1 (松开状态)
   │   └─> 发送速度命令到CAN总线
   │
   └─> 速度命令为0（speed_left/right == 0）
       └─> 设置 bk_flag = 0 (刹车状态)
       └─> 发送停止命令到CAN总线

3. 定时器检测
   ├─> 每5秒检查一次
   ├─> 如果 bk_flag == 0（刹车状态持续5秒）
   │   └─> 说明：长时间无速度命令
   │   └─> 执行：发送紧急刹车命令
   │
   └─> 如果 bk_flag == 1（有速度命令）
       └─> 说明：系统正常工作
       └─> 执行：无需操作（定时器自动重置）
```

### 代码实现

```c
// 刹车标志更新（在 drv_keyadouble.c 中）
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // 更新刹车标志
    if (speed_left != 0) {
        bk_flag_left = 1;  // 1 = 松开（有速度命令）
    } else {
        bk_flag_left = 0;  // 0 = 刹车（无速度命令）
    }

    if (speed_right != 0) {
        bk_flag_right = 1;  // 1 = 松开（有速度命令）
    } else {
        bk_flag_right = 0;  // 0 = 刹车（无速度命令）
    }

    // 发送速度命令到CAN总线
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right);
}

// 定时器回调（在 main.c 中）
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    if (bk_flag_left == 0) {
        // 刹车状态持续5秒，说明长时间无速度命令
        // 发送紧急刹车命令（通过CAN总线）
        ESP_LOGI(TAG, "⚠️ Left brake applied (emergency stop)");
        // TODO: 实际发送刹车命令到CAN总线
    }
}
```

---

## ⚠️ 当前实现的问题

### 问题1：定时器未重置

**当前行为：**
- 定时器启动后5秒触发一次
- 无论 `bk_flag` 状态如何，都会触发

**预期行为：**
- 当 `bk_flag == 1`（有速度命令）时，应该重置定时器
- 只有当 `bk_flag == 0` 持续5秒时才触发刹车

**建议修复：**
```c
// 在 intf_move_keyadouble() 中重置定时器
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // ... 更新刹车标志 ...

    // 如果有速度命令，重置定时器（防止误触发）
    if (speed_left != 0) {
        xTimerReset(brake_timer_left, 0);
    }
    if (speed_right != 0) {
        xTimerReset(brake_timer_right, 0);
    }

    // ... 发送速度命令 ...
}
```

### 问题2：刹车命令未实现

**当前行为：**
- 定时器回调函数只打印日志
- 没有实际发送刹车命令到CAN总线

**建议修复：**
```c
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    if (bk_flag_left == 0) {
        ESP_LOGW(TAG, "⚠️ 左电机长时间无速度命令，触发紧急刹车");

        // 发送紧急刹车命令到CAN总线
        motor_control(CMD_DISABLE, MOTOR_CHANNEL_A, 0);  // 失能电机
        motor_control(CMD_SPEED, MOTOR_CHANNEL_A, 0);     // 速度设为0

        // 可选：发送多次确保收到
        for (int i = 0; i < 3; i++) {
            motor_control(CMD_SPEED, MOTOR_CHANNEL_A, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
```

### 问题3：定时器配置不合理

**当前配置：**
```c
pdFALSE,  // 单次触发
```

**建议配置：**
```c
pdTRUE,   // 自动重载（周期触发）
```

这样定时器会每5秒检查一次，而不是只检查一次。

---

## 📊 改进建议

### 改进方案1：完整的安全机制

```c
/**
 * 电机控制函数（改进版）
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // ... 参数检查 ...

    // 更新刹车标志
    if (speed_left != 0) {
        bk_flag_left = 1;  // 有速度命令
        xTimerReset(brake_timer_left, 0);  // 重置定时器
    } else {
        bk_flag_left = 0;  // 无速度命令（刹车状态）
    }

    if (speed_right != 0) {
        bk_flag_right = 1;
        xTimerReset(brake_timer_right, 0);
    } else {
        bk_flag_right = 0;
    }

    // ... 发送速度命令 ...
}

/**
 * 刹车定时器回调（改进版）
 */
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    // 检查是否长时间无速度命令
    if (bk_flag_left == 0) {
        uint32_t last_update = g_last_motor_update;
        uint32_t current_time = xTaskGetTickCount();
        uint32_t time_diff = (current_time - last_update) * portTICK_PERIOD_MS;

        // 如果超过5秒未更新，触发紧急刹车
        if (time_diff > 5000) {
            ESP_LOGW(TAG, "⚠️ 左电机通信超时（%lu ms），触发紧急刹车",
                     (unsigned long)time_diff);

            // 发送紧急刹车命令
            motor_control(CMD_SPEED, MOTOR_CHANNEL_A, 0);

            // 可选：失能电机（更彻底的刹车）
            // motor_control(CMD_DISABLE, MOTOR_CHANNEL_A, 0);
        }
    }
}
```

### 改进方案2：使用全局更新时间戳

```c
// 在 main.c 中添加全局变量
uint32_t g_last_motor_update = 0;

// 在 intf_move_keyadouble() 中更新时间戳
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // ... 更新速度命令 ...

    // 更新时间戳（无论速度是否为0）
    g_last_motor_update = xTaskGetTickCount();

    return 0;
}

// 在定时器回调中检查时间戳
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    uint32_t current_time = xTaskGetTickCount();
    uint32_t time_diff = (current_time - g_last_motor_update) * portTICK_PERIOD_MS;

    // 如果超过5秒未更新，触发紧急刹车
    if (time_diff > 5000) {
        ESP_LOGW(TAG, "⚠️ 电机控制超时（%lu ms），触发紧急刹车",
                 (unsigned long)time_diff);
        motor_control(CMD_SPEED, MOTOR_CHANNEL_A, 0);
    }
}
```

---

## 🎯 刹车定时器的正确作用

### 1. **防止通信中断导致失控**

```
场景：遥控器信号丢失
├─> SBUS接收不到数据
├─> 速度命令停止更新
├─> 车辆可能继续按上次速度运动
└─> 刹车定时器检测到5秒无更新
    └─> 自动触发紧急刹车 ✅
```

### 2. **防止程序异常导致失控**

```
场景：控制程序崩溃或死锁
├─> 电机控制任务停止
├─> 速度命令停止更新
├─> 车辆可能继续运动
└─> 刹车定时器独立运行
    └─> 检测到异常后自动刹车 ✅
```

### 3. **防止设备故障导致失控**

```
场景：CAN总线故障
├─> 速度命令无法发送
├─> 电机可能保持上次状态
├─> 车辆可能继续运动
└─> 刹车定时器检测到无更新
    └─> 尝试发送紧急刹车命令 ✅
```

---

## 📝 总结

### 当前实现状态

- ✅ **已实现**：定时器创建和启动
- ⚠️ **部分实现**：刹车检测逻辑（有缺陷）
- ❌ **未实现**：实际刹车命令发送
- ❌ **未实现**：定时器重置机制

### 建议的改进优先级

**优先级A（必须修复）：**
1. ✅ 实现定时器重置机制（有速度命令时重置）
2. ✅ 实现实际的刹车命令发送

**优先级B（建议改进）：**
3. 📋 使用全局时间戳而非标志位
4. 📋 添加多次重试机制
5. 📋 添加刹车确认机制

**优先级C（可选优化）：**
6. 📅 可配置的超时时间
7. 📅 刹车强度分级（轻度/紧急）
8. 📅 日志记录刹车事件

---

## 🔧 快速修复代码

如果需要立即修复，可以这样修改：

```c
// 1. 修改定时器配置（自动重载）
brake_timer_left = xTimerCreateStatic(
    "brake_left",
    pdMS_TO_TICKS(5000),
    pdTRUE,  // 改为自动重载（周期触发）
    ...
);

// 2. 在电机控制函数中重置定时器
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    // ... 现有代码 ...

    // 重置定时器（防止误触发）
    if (speed_left != 0) {
        xTimerReset(brake_timer_left, 0);
    }
    if (speed_right != 0) {
        xTimerReset(brake_timer_right, 0);
    }

    return 0;
}

// 3. 在定时器回调中发送刹车命令
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    if (bk_flag_left == 0) {
        ESP_LOGW(TAG, "⚠️ 左电机长时间无速度命令，触发紧急刹车");
        motor_control(CMD_SPEED, MOTOR_CHANNEL_A, 0);
    }
}
```

---

**结论：刹车定时器是一个重要的安全机制，但当前实现还不完整，建议尽快修复以确保安全性。**
