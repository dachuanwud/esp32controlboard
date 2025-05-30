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

// 定义GPIO引脚
// LED指示灯引脚 - 共阳极RGB LED
// LED1组
#define LED1_RED_PIN            GPIO_NUM_12  // LED1红色引脚
#define LED1_GREEN_PIN          GPIO_NUM_13  // LED1绿色引脚
#define LED1_BLUE_PIN           GPIO_NUM_14  // LED1蓝色引脚

// LED2组
#define LED2_RED_PIN            GPIO_NUM_25  // LED2红色引脚
#define LED2_GREEN_PIN          GPIO_NUM_26  // LED2绿色引脚
#define LED2_BLUE_PIN           GPIO_NUM_27  // LED2蓝色引脚

// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
#define KEY2_PIN                GPIO_NUM_35  // 按键2

// 电机控制引脚 (通过CAN总线控制，不需要直接GPIO控制)
// CAN总线引脚定义:
// - TX: GPIO_NUM_16 (连接到SN65HVD232D的D引脚)
// - RX: GPIO_NUM_17 (连接到SN65HVD232D的R引脚)

// PWM通道定义
#define PWM_CHANNEL_1           LEDC_CHANNEL_0
#define PWM_CHANNEL_2           LEDC_CHANNEL_1
#define PWM_CHANNEL_3           LEDC_CHANNEL_2
#define PWM_CHANNEL_4           LEDC_CHANNEL_3

// UART定义
#define UART_DEBUG              UART_NUM_0   // 调试串口 (通过CH340)
#define UART_CMD                UART_NUM_1   // CMD_VEL接收 (RX: GPIO_NUM_21)
#define UART_SBUS               UART_NUM_2   // SBUS接收 (RX: GPIO_NUM_22)

// 缓冲区长度定义
#define LEN_SBUS                25
#define LEN_CHANEL              12
#define LEN_CMD                 7
#define LEN_485                 16

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
