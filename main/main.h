#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// 定义GPIO引脚
#define LED_BLUE_PIN            GPIO_NUM_2   // 蓝色LED指示灯

// 电机控制引脚
#define LEFT_EN_PIN             GPIO_NUM_4   // 左电机使能
#define LEFT_DIR_PIN            GPIO_NUM_5   // 左电机方向
#define RIGHT_DIR_PIN           GPIO_NUM_18  // 右电机方向
#define RIGHT_EN_PIN            GPIO_NUM_19  // 右电机使能
#define LEFT_BK_PIN             GPIO_NUM_21  // 左电机刹车
#define RIGHT_BK_PIN            GPIO_NUM_22  // 右电机刹车

// PWM通道定义
#define PWM_CHANNEL_1           LEDC_CHANNEL_0
#define PWM_CHANNEL_2           LEDC_CHANNEL_1
#define PWM_CHANNEL_3           LEDC_CHANNEL_2
#define PWM_CHANNEL_4           LEDC_CHANNEL_3

// UART定义
#define UART_DEBUG              UART_NUM_0   // 调试串口
#define UART_485                UART_NUM_1   // 485通信
#define UART_SBUS               UART_NUM_2   // SBUS接收
#define UART_CMD                UART_NUM_1   // CMD_VEL接收 (与485共用)

// 缓冲区长度定义
#define LEN_SBUS                25
#define LEN_CHANEL              12
#define LEN_CMD                 7
#define LEN_485                 16

// 外部变量声明
extern uint8_t bk_flag_left;
extern uint8_t bk_flag_right;

#endif /* MAIN_H */
