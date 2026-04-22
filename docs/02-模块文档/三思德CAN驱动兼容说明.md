# 三思德 CAN 驱动兼容说明

本文档说明当前 ESP32 控制板工程如何兼容三思德双路无刷驱动器，依据的驱动手册为：

- [驱动器使用说明v1.063(1).pdf](/Users/houjl/Downloads/三思德/放线车/驱动器使用说明v1.063(1).pdf)

## 1. 编译选择

当前工程支持通过编译宏选择电机驱动协议，配置位于：

- [main/main.h](/Users/houjl/Downloads/esp32controlboard/main/main.h)

当前仓库默认配置为三思德驱动协议：

```c
#define MOTOR_DRIVER_PROTOCOL_KEYA_SDO   1
#define MOTOR_DRIVER_PROTOCOL_WEST_CAN   2
#define MOTOR_DRIVER_PROTOCOL            MOTOR_DRIVER_PROTOCOL_WEST_CAN
```

如果要切换为三思德驱动器，将 `MOTOR_DRIVER_PROTOCOL` 改为 `MOTOR_DRIVER_PROTOCOL_WEST_CAN`。

## 2. 协议差异

历史驱动协议：

- 扩展帧 `0x06000001`
- 每路电机分别发 1 帧
- 通过寄存器写命令实现使能和速度设置

三思德驱动协议：

- 扩展帧 `0x0DEEFF00`
- 单帧同时携带两路电机 32 位控制值
- 上电后需要先发 `10` 条零速命令解除上电保护
- 驱动器在 `500ms` 内收不到正确控制命令会停止输出

## 3. 当前兼容实现

兼容实现位于：

- [main/motor_driver.c](/Users/houjl/Downloads/esp32controlboard/main/motor_driver.c)
- [main/drv_sanside.c](/Users/houjl/Downloads/esp32controlboard/main/drv_sanside.c)

当前实现包含：

1. `motor_driver` 统一上层入口
2. `drv_sanside` 三思德控制帧封装
3. 初始化/恢复后的 `10` 条零速解锁序列
4. 周期性发送最新速度帧，满足 `500ms` 保活要求

## 4. 三思德控制帧

当前代码按手册中的控制帧发送：

- 帧 ID：`0x0DEEFF00`
- 数据长度：`8`
- Data0-3：电机 1 的 32 位有符号控制值，高位在前
- Data4-7：电机 2 的 32 位有符号控制值，高位在前

## 5. 速度值映射

上层控制逻辑输出范围仍然是 `-100 ~ 100`。

当前兼容层默认按三思德手册的开环模式范围做线性映射：

- `-100 -> -1100`
- `0 -> 0`
- `100 -> 1100`

说明：

- 这意味着当前实现默认假设驱动器工作在开环模式
- 如果你的驱动器已经被配置成速度闭环模式，那么这里的控制值应改成“目标转速”范围，而不是 `±1100`

对应代码位置：

- [main/drv_sanside.c](/Users/houjl/Downloads/esp32controlboard/main/drv_sanside.c)

## 6. 接线和总线参数

- CAN 波特率：`250 kbps`
- 帧格式：扩展帧
- ESP32 TX：`GPIO16`
- ESP32 RX：`GPIO17`

这部分沿用项目当前的 TWAI 配置。

## 7. 使用建议

建议第一次联调时：

1. 先确认驱动器 CAN 波特率为 `250K`
2. 先确认驱动器实际工作模式是开环还是速度闭环
3. 上电后观察是否能稳定收到 `0x0DEE(ID)01~04` 返回帧
4. 先架空轮子测试正反转方向
5. 如果转速明显不对，再调整速度映射而不是先改上层遥控逻辑
