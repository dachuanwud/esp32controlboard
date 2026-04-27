#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"

// 项目版本信息
#include "version.h"

// ====================================================================
// 功能开关配置
// ====================================================================

// 核心功能模式开关 - 设置为1时禁用Web功能和Wi-Fi连接，确保核心功能稳定性
#define CORE_FUNCTION_MODE      1

// 调试功能开关
#define ENABLE_SBUS_DEBUG       0   // 启用SBUS实时调试打印
#define ENABLE_SBUS_RAW_DATA    0   // 启用SBUS原始数据打印
#define ENABLE_SBUS_FRAME_INFO  0   // 启用SBUS帧信息打印
#define ENABLE_CAN_DEBUG        0   // 启用CAN发送/状态调试打印
#define ENABLE_CMD_VEL          0   // 禁用CMD_VEL UART接收与任务

// 遥控器输入方案选择
// 1: 原有云卓/默认SBUS通道方案
// 2: T12D 遥控器方案
#define REMOTE_INPUT_PROFILE_YUNZHUO   1
#define REMOTE_INPUT_PROFILE_T12D      2
#define REMOTE_INPUT_PROFILE           REMOTE_INPUT_PROFILE_T12D

// T12D 轴向适配：
// 当前现场驱动方向：CH3 推高时需要反向输出，才对应小车前进；
// 左右轴保留原有反向适配，避免改变已正常的转向手感。
#define T12D_INVERT_STEERING_AXIS   1
#define T12D_INVERT_THROTTLE_AXIS   1

// 电机驱动协议选择
// 1: 历史双路驱动(SDO/寄存器写命令)
// 2: 三思德双路无刷驱动(扩展帧 0x0DEEFF00 双电机控制)
#define MOTOR_DRIVER_PROTOCOL_KEYA_SDO   1
#define MOTOR_DRIVER_PROTOCOL_WEST_CAN   2
#define MOTOR_DRIVER_PROTOCOL            MOTOR_DRIVER_PROTOCOL_WEST_CAN

// 三思德双路驱动电机安装方向适配。
// 当前现场现象：CH3 前进命令左右同速时车体左转，说明左侧电机实际方向与逻辑方向相反。
#define WEST_CAN_INVERT_LEFT_MOTOR       1
#define WEST_CAN_INVERT_RIGHT_MOTOR      0

// ====================================================================
// 放线设备 (RS485 / Modbus RTU) 功能开关
// ====================================================================
// 1: 启用 CH7 + CH10 控制放线设备（UART1 发送 Modbus 写单寄存器帧）
// 0: 完全禁用，drv_payout.c 退化为空实现，零代码影响
#define ENABLE_PAYOUT_DEVICE       1

// 放线设备 UART / GPIO 映射
// 使用 UART1，TX 复用通用 UART1 TX 引脚，只发不收，无需绑定 RX
// 如果后续需要读驱动板回复，把 PAYOUT_UART_RX_PIN 改成空闲 GPIO 即可
#define PAYOUT_UART                UART_NUM_1
#define PAYOUT_UART_TX_PIN         UART_TX_PIN
#define PAYOUT_UART_RX_PIN         UART_PIN_NO_CHANGE
#define PAYOUT_RS485_DE_PIN        GPIO_NUM_NC   // 自动流控 RS485 模块填 GPIO_NUM_NC

// 放线设备 Modbus / 速度换算参数
// 与 esp32controlboard_fangxianqi_esp32 参考工程完全一致
#define PAYOUT_BAUD_RATE           9600
#define PAYOUT_MODBUS_SLAVE        0x01
#define PAYOUT_MODBUS_REG          0x0042
#define PAYOUT_CHANNEL_MIN         1050
#define PAYOUT_CHANNEL_MAX         1950
#define PAYOUT_CHANNEL_MID         1500
#define PAYOUT_CHANNEL_DEADBAND    50      // 中位 1500±50 视为 0 速
#define PAYOUT_PWM_SCALE           20      // (ch-1500)/9 * 20 → -1000~+1000
#define PAYOUT_PWM_LIMIT           1000

// SBUS 通道分配（放线设备专用）
// CH7(ch_val[6]) = 正/反转切换（高档=正转，低档=反转，中位=停止）
// CH10(ch_val[9]) = 速度大小（最小=0，最大=满速）
// 不再占用 CH2 / CH8，保留给遥控小车原有语义。
#define PAYOUT_DIRECTION_CHANNEL_IDX  6U
#define PAYOUT_SPEED_CHANNEL_IDX      9U

// 发送限频：避免以 SBUS 14ms 节奏狂发 Modbus 帧压爆 RS485
#define PAYOUT_SEND_INTERVAL_MS    50

// CMD_VEL功能开关 - 设置为0禁用UART1 CMD_VEL接收
#define ENABLE_CMD_VEL          0   // 禁用CMD_VEL功能（节省UART1资源）

// 功能模块开关（当CORE_FUNCTION_MODE=1时，以下功能将被禁用）
#if CORE_FUNCTION_MODE
    #define ENABLE_HTTP_SERVER      0   // 禁用HTTP服务器
    #define ENABLE_CLOUD_CLIENT     0   // 禁用云客户端
    #define ENABLE_DATA_INTEGRATION 0   // 禁用数据集成
    #define ENABLE_WEB_FEATURES     0   // 禁用所有Web功能
    #define ENABLE_WIFI             0   // 禁用Wi-Fi连接
#else
    #define ENABLE_HTTP_SERVER      1   // 启用HTTP服务器
    #define ENABLE_CLOUD_CLIENT     1   // 启用云客户端
    #define ENABLE_DATA_INTEGRATION 1   // 启用数据集成
    #define ENABLE_WEB_FEATURES     1   // 启用所有Web功能
    #define ENABLE_WIFI             1   // 启用Wi-Fi连接
#endif

// OTA功能开关（需要临时禁用时设为0）
#define ENABLE_CLOUD_OTA       0   // 云端OTA
#define ENABLE_HTTP_OTA        0   // HTTP OTA

// 定义GPIO引脚
// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
// UART0 GPIO1 TXD,GPIO3 RXD, 但GPIO3是启动脚，慎用RX；优先用USB虚拟串口，无需外接引脚
// GPIO 0,2,15启动风险 GPIO0/GPIO2：Boot引脚，RX 慎用（开机电平影响烧录），TX可临时用但不推荐
// GPIO6~GPIO11（Flash）  UART1 GPIO10 TX,GPIO9 RX 与Flash引脚复用，使用会导致程序崩溃，必须重映射到其他引脚
// LED指示灯引脚 - 共阳极RGB LED
// LED1组
#define LED1_RED_PIN            GPIO_NUM_NC  // GPIO12是启动绑带脚，禁用红灯避免上电偶发启动失败
#define LED1_GREEN_PIN          GPIO_NUM_13  // LED1绿色引脚
#define LED1_BLUE_PIN           GPIO_NUM_13  // LED1蓝色引脚
#define LED2_RED_PIN            GPIO_NUM_NC  // GPIO12是启动绑带脚，禁用红灯避免上电偶发启动失败
#define LED2_GREEN_PIN          GPIO_NUM_13  // LED2绿色引脚
#define LED2_BLUE_PIN           GPIO_NUM_13  // LED2蓝色引脚
// 电机控制引脚 (通过CAN总线控制，不需要直接GPIO控制)
// CAN总线引脚定义:
// - TX: GPIO_NUM_16 (连接到SN65HVD232D的D引脚)
// - RX: GPIO_NUM_17 (连接到SN65HVD232D的R引脚)
#define CAN_TX_PIN              GPIO_NUM_16 // - TX: GPIO_NUM_16 (连接到SN65HVD232D的D引脚)
#define CAN_RX_PIN              GPIO_NUM_17 // - RX: GPIO_NUM_17 (连接到SN65HVD232D的R引脚)
#define MPC_PIN                 GPIO_NUM_21  // mpu引脚
#define I2C_SDA_PIN             GPIO_NUM_22  // sda
#define I2C_SCL_RED_PIN         GPIO_NUM_23  // scl
#define SBUS_RX_PIN             GPIO_NUM_25  // 遥控器sbus引脚
#define UART_TX_PIN             GPIO_NUM_26  // UART1 TX 引脚
#define UART_RX_PIN             GPIO_NUM_27  // UART1 RX 引脚 这里使用GPIO27作为CMD_VEL接收引脚，请根据实际硬件连接调整

//GPIO34~GPIO39仅输入
#define KEY2_PIN                GPIO_NUM_35  // 按键2

// PWM通道定义
#define PWM_CHANNEL_1           LEDC_CHANNEL_0
#define PWM_CHANNEL_2           LEDC_CHANNEL_1
#define PWM_CHANNEL_3           LEDC_CHANNEL_2
#define PWM_CHANNEL_4           LEDC_CHANNEL_3

// UART定义
#define UART_DEBUG              UART_NUM_0   // 调试串口 (通过CH340)
#define UART_CMD                UART_NUM_1   // CMD_VEL接收 (RX: GPIO_NUM_21)
#define UART_SBUS               UART_NUM_2   // SBUS接收 (RX: SBUS_RX_PIN)
// 电机控制modbus rs485串口信息
#define BAUD_RATE               9600                // 波特率
#define DATA_BITS               UART_DATA_8_BITS    // 8位数据位
#define PARITY                  UART_PARITY_EVEN    // 偶校验位
#define STOP_BITS               UART_STOP_BITS_1    // 1位停止位
// 编译期防护：SBUS RX引脚不能与LED引脚复用，否则会导致UART接收失效
// 注意：GPIO_NUM_xxx 是枚举常量，不能用于预处理 #if 比较，使用 _Static_assert
_Static_assert(SBUS_RX_PIN != LED1_RED_PIN, "SBUS_RX_PIN conflicts with LED1_RED_PIN");
_Static_assert(SBUS_RX_PIN != LED1_GREEN_PIN, "SBUS_RX_PIN conflicts with LED1_GREEN_PIN");
_Static_assert(SBUS_RX_PIN != LED1_BLUE_PIN, "SBUS_RX_PIN conflicts with LED1_BLUE_PIN");
_Static_assert(SBUS_RX_PIN != LED2_RED_PIN, "SBUS_RX_PIN conflicts with LED2_RED_PIN");
_Static_assert(SBUS_RX_PIN != LED2_GREEN_PIN, "SBUS_RX_PIN conflicts with LED2_GREEN_PIN");
_Static_assert(SBUS_RX_PIN != LED2_BLUE_PIN, "SBUS_RX_PIN conflicts with LED2_BLUE_PIN");

// 缓冲区长度定义
#define LEN_SBUS                25
#define LEN_CHANEL              12
#define LEN_CMD                 7
#define LEN_485                 16

// 将毫秒向上取整为 FreeRTOS tick，避免在 100Hz tick 下出现 pdMS_TO_TICKS(1/2)=0 的忙等。
#define RTOS_DELAY_TICKS(ms) \
    ((TickType_t)(((ms) + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS))

// 外部变量声明
extern uint8_t bk_flag_left;
extern uint8_t bk_flag_right;


// 全局状态变量声明（供HTTP服务器使用）
extern uint32_t g_last_sbus_update;
extern uint32_t g_last_motor_update;
extern uint16_t g_last_sbus_channels[16];
extern int8_t g_last_motor_left;
extern int8_t g_last_motor_right;

#endif /* MAIN_H */
