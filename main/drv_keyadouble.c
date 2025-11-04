#include <stdio.h>
#include <string.h>
#include "drv_keyadouble.h"
#include "main.h"
#include <inttypes.h>

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

// 电机使能状态标志（避免重复发送使能命令）
static bool motor_enabled = false;

// TWAI (CAN) 配置 - 根据电路图SN65HVD232D CAN收发电路
// IO16连接到SN65HVD232D的D引脚(TX)，IO17连接到R引脚(RX)
// 使用标准模式，但发送时不等待ACK应答
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
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

    // 发送消息 - 不等待ACK，立即发送
    esp_err_t result = twai_transmit(&message, 0);  // 超时设为0，不等待
    if (result != ESP_OK) {
        // 只在严重错误时打印详细信息，超时错误降级为调试级别
        if (result == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "CAN send timeout (normal in no-ACK mode)");
        } else {
            ESP_LOGW(TAG, "CAN send error: %s", esp_err_to_name(result));
            // 只在严重错误时打印状态
            twai_status_info_t status_info;
            if (twai_get_status_info(&status_info) == ESP_OK) {
                ESP_LOGW(TAG, "CAN Status - State: %" PRIu32 ", TX Error: %" PRIu32 ", RX Error: %" PRIu32,
                         (unsigned long)status_info.state, (unsigned long)status_info.tx_error_counter, (unsigned long)status_info.rx_error_counter);
            }
        }
    }

    // 只在调试模式下打印详细的CAN数据
    ESP_LOGD(TAG, "CAN TX: %08" PRIX32 " [%02X %02X %02X %02X %02X %02X %02X %02X]",
             id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    // 只在速度命令时打印简化的速度信息 (0x23 0x00 0x20 channel speed_bytes)
    if (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20) {
        int32_t sp_value_tx = ((int32_t)data[4] << 24) |
                             ((int32_t)data[5] << 16) |
                             ((int32_t)data[6] << 8) |
                             ((int32_t)data[7]);
        int8_t actual_speed = (int8_t)(sp_value_tx / 100);
        uint8_t channel = data[3];
        ESP_LOGD(TAG, "Motor Ch%d speed: %d", channel, actual_speed);
    }

    // ⚡ 性能优化：移除延迟，避免阻塞控制循环
    // CAN发送采用非阻塞模式(超时=0)，无需额外延迟
    // 原有的10ms延迟会导致每次电机控制延迟40ms（4帧×10ms）
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
    // 初始化TWAI (CAN)
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    // 初始化电机使能状态
    motor_enabled = false;

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

    // ⚡ 性能优化：只在首次调用时发送使能命令，避免重复发送
    // 电机驱动器在使能后会保持状态，无需每次都发送使能命令
    // 这将减少50%的CAN帧发送量（从4帧减少到2帧）
    if (!motor_enabled) {
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0); // 使能A路(左侧)
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0); // 使能B路(右侧)
        motor_enabled = true;
        ESP_LOGI(TAG, "⚡ Motors enabled (one-time initialization)");
    }

    // 设置速度命令（每次都需要发送）
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left); // A路(左侧)速度
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right); // B路(右侧)速度

    // 🔒 安全机制：如果有速度命令，重置刹车定时器
    // 这样定时器知道系统正常工作，不会误触发紧急刹车
    if (speed_left != 0 && brake_timer_left != NULL) {
        xTimerReset(brake_timer_left, 0);  // 重置左刹车定时器
    }
    if (speed_right != 0 && brake_timer_right != NULL) {
        xTimerReset(brake_timer_right, 0);  // 重置右刹车定时器
    }

    return 0;
}
