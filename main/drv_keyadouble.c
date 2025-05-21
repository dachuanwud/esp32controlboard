#include <stdio.h>
#include <string.h>
#include "drv_keyadouble.h"
#include "main.h"

static const char *TAG = "DRV_KEYA";

// 电机驱动CAN ID定义
#define DRIVER_ADDRESS 0x01 // 驱动器地址(默认为1)
#define DRIVER_TX_ID 0x06000000 // 发送基础ID (控制->驱动器)
#define DRIVER_RX_ID 0x05800000 // 接收基础ID (驱动器->控制)
#define DRIVER_HEARTBEAT_ID 0x07000000 // 心跳包ID (驱动器->控制)

// 电机通道定义
#define MOTOR_CHANNEL_A 0x01 // A路电机(左侧)
#define MOTOR_CHANNEL_B 0x02 // B路电机(右侧)

// 命令类型定义
#define CMD_ENABLE 0x01 // 使能电机
#define CMD_DISABLE 0x02 // 失能电机
#define CMD_SPEED 0x03 // 设置速度

// 外部变量
uint8_t bk_flag_left = 0;
uint8_t bk_flag_right = 0;

// TWAI (CAN) 配置
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/**
 * 发送CAN数据
 * @param id CAN扩展ID
 * @param data 8字节数据
 */
static void keya_send_data(uint32_t id, uint8_t* data)
{
    twai_message_t message;
    message.extd = 1;                 // 扩展帧(29位ID)
    message.identifier = id;
    message.data_length_code = 8;     // 帧长度8字节
    message.rtr = 0;                  // 数据帧

    // 复制数据
    for (int i = 0; i < 8; i++) {
        message.data[i] = data[i];
    }

    // 发送消息
    if (twai_transmit(&message, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send CAN message");
    }

    // 打印发送的CAN数据(调试用)
    char buffer[100];
    sprintf(buffer, "CAN:%08lX:", id);
    printf("%s", buffer);
    for (int i = 0; i < 8; i++) {
        sprintf(buffer, "%02X ", data[i]);
        printf("%s", buffer);
    }
    printf("\\r\\n");

    // 打印速度命令 (0x23 0x00 0x20 channel speed_bytes)
    if (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20) {
        int32_t sp_value_tx = ((int32_t)data[4] << 24) |
                             ((int32_t)data[5] << 16) |
                             ((int32_t)data[6] << 8) |
                             ((int32_t)data[7]);
        int8_t actual_speed = (int8_t)(sp_value_tx / 100);
        char speed_buffer[50];
        uint8_t channel = data[3];
        sprintf(speed_buffer, "Speed CMD Ch%d: %d\\r\\n", channel, actual_speed);
        printf("%s", speed_buffer);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}

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

    keya_send_data(tx_id, tx_data);
}

/**
 * 初始化电机驱动
 */
esp_err_t drv_keyadouble_init(void)
{
    // 初始化GPIO
    gpio_config_t io_conf = {};

    // 配置刹车引脚
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LEFT_BK_PIN) | (1ULL << RIGHT_BK_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 配置电机控制引脚
    io_conf.pin_bit_mask = (1ULL << LEFT_EN_PIN) | (1ULL << LEFT_DIR_PIN) |
                          (1ULL << RIGHT_DIR_PIN) | (1ULL << RIGHT_EN_PIN);
    gpio_config(&io_conf);

    // 设置初始状态
    gpio_set_level(LEFT_BK_PIN, 0);
    gpio_set_level(RIGHT_BK_PIN, 0);
    gpio_set_level(LEFT_EN_PIN, 1);
    gpio_set_level(LEFT_DIR_PIN, 1);
    gpio_set_level(RIGHT_DIR_PIN, 1);
    gpio_set_level(RIGHT_EN_PIN, 1);

    // 初始化TWAI (CAN)
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "Motor driver initialized");
    return ESP_OK;
}

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

    // 左侧刹车控制 (A路)
    if (speed_left != 0) {
        bk_flag_left = 1;
        gpio_set_level(LEFT_BK_PIN, 1); // 高电平解除刹车，允许转动
    } else {
        bk_flag_left = 0; // 0为刹车，1为松开
    }

    // 右侧刹车控制 (B路)
    if (speed_right != 0) {
        bk_flag_right = 1;
        gpio_set_level(RIGHT_BK_PIN, 1);
    } else {
        bk_flag_right = 0; // 0为刹车，1为松开
    }

    // 使能电机两路电机
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0); // 使能A路(左侧)
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0); // 使能B路(右侧)

    // 设置速度命令
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left); // A路(左侧)速度
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right); // B路(右侧)速度

    return 0;
}
