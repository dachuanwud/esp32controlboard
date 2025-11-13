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

// CAN接收任务句柄
static TaskHandle_t can_rx_task_handle = NULL;

// CAN总线恢复计数器
static uint32_t can_recovery_count = 0;

// CAN总线恢复时间戳（用于限制恢复频率）
static uint32_t last_recovery_time = 0;
#define CAN_RECOVERY_MIN_INTERVAL_MS 1000  // 最小恢复间隔1秒，避免频繁恢复影响SBUS接收

// TWAI (CAN) 配置 - 根据电路图SN65HVD232D CAN收发电路
// IO16连接到SN65HVD232D的D引脚(TX)，IO17连接到R引脚(RX)
// 使用标准模式，但发送时不等待ACK应答
// 注意：配置结构体在初始化函数中创建，避免静态初始化问题
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/**
 * CAN总线恢复函数
 * 当错误计数器过高或处于BUS-OFF状态时，停止并重启CAN驱动
 * @return ESP_OK=恢复成功，其他=恢复失败
 */
static esp_err_t can_bus_recovery(void)
{
    twai_status_info_t status_info;
    esp_err_t ret;
    
    // 获取当前CAN状态
    ret = twai_get_status_info(&status_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "无法获取CAN状态信息: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 检查是否需要恢复
    bool need_recovery = false;
    const char* reason = NULL;
    
    if (status_info.state == TWAI_STATE_BUS_OFF) {
        need_recovery = true;
        reason = "BUS-OFF状态";
    } else if (status_info.tx_error_counter > 127 || status_info.rx_error_counter > 127) {
        need_recovery = true;
        reason = "错误计数器过高";
    }
    
    if (!need_recovery) {
        return ESP_OK;  // 不需要恢复
    }
    
    // 🔧 优化：限制恢复频率，避免频繁恢复影响SBUS接收
    uint32_t current_time = xTaskGetTickCount();
    if (last_recovery_time != 0 && 
        (current_time - last_recovery_time) < pdMS_TO_TICKS(CAN_RECOVERY_MIN_INTERVAL_MS)) {
        // 距离上次恢复时间太短，跳过本次恢复
        ESP_LOGD(TAG, "CAN恢复间隔太短，跳过本次恢复 (距离上次: %" PRIu32 "ms)",
                 (current_time - last_recovery_time) * portTICK_PERIOD_MS);
        return ESP_OK;
    }
    
    // 记录恢复前的状态
    ESP_LOGW(TAG, "CAN总线需要恢复: %s | 状态: %" PRIu32 ", TX错误: %" PRIu32 ", RX错误: %" PRIu32,
             reason,
             (unsigned long)status_info.state,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter);
    
    // 更新恢复时间戳
    last_recovery_time = current_time;
    
    // 停止CAN驱动
    ret = twai_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止CAN驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 🔧 优化：减少等待时间，避免长时间阻塞影响SBUS接收
    // 从100ms减少到50ms，减少对系统的影响
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 重启CAN驱动
    ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重启CAN驱动失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 更新恢复计数
    can_recovery_count++;
    
    // 🔧 优化：减少验证延迟，从50ms减少到20ms
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = twai_get_status_info(&status_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CAN总线恢复成功 (恢复次数: %" PRIu32 ") | 状态: %" PRIu32 ", TX错误: %" PRIu32 ", RX错误: %" PRIu32,
                 can_recovery_count,
                 (unsigned long)status_info.state,
                 (unsigned long)status_info.tx_error_counter,
                 (unsigned long)status_info.rx_error_counter);
    }
    
    return ESP_OK;
}

/**
 * CAN接收任务 - 批量清空接收队列
 * 避免电机反馈帧填满接收队列，影响CAN发送功能
 * 优先级设为5（低优先级），确保发送优先
 * 
 * 优化策略：
 * - 批量处理：每次循环最多处理10条消息，减少驱动内部锁竞争
 * - 自适应延迟：队列有消息时快速循环（1ms），队列为空时较长延迟（10ms）
 */
static void can_rx_task(void *pvParameters)
{
    twai_message_t message;
    uint32_t rx_count = 0;
    uint32_t batch_count = 0;
    
    ESP_LOGI(TAG, "CAN接收任务已启动");
    
    while (1) {
        batch_count = 0;
        
        // 批量清空接收队列，每次最多处理10条消息
        // 避免单次循环时间过长，减少驱动内部锁的持有时间
        while (batch_count < 10) {
            esp_err_t ret = twai_receive(&message, 0);
            if (ret == ESP_OK) {
                rx_count++;
                batch_count++;
                
                // 打印CAN接收消息的详细信息
                ESP_LOGI(TAG, "📥 CAN RX #%lu: ID=0x%08" PRIX32 " (%s), DLC=%d, RTR=%d, Data=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                         (unsigned long)rx_count,
                         message.identifier,
                         message.extd ? "EXT" : "STD",
                         message.data_length_code,
                         message.rtr,
                         message.data[0], message.data[1], message.data[2], message.data[3],
                         message.data[4], message.data[5], message.data[6], message.data[7]);
                
                // 只清空队列，不处理数据（根据用户需求）
                // 电机反馈帧被丢弃，避免队列满
            } else if (ret == ESP_ERR_TIMEOUT) {
                // 队列为空，跳出内层循环
                break;
            } else {
                ESP_LOGD(TAG, "CAN接收错误: %s", esp_err_to_name(ret));
                break;
            }
        }
        
        // 自适应延迟策略：
        // - 如果处理了消息，快速循环（1ms），尽快清空队列，避免阻塞发送
        // - 如果队列为空，较长延迟（10ms），减少CPU占用
        if (batch_count > 0) {
            vTaskDelay(pdMS_TO_TICKS(1));  // 快速循环，尽快清空队列
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));  // 正常延迟，减少CPU占用
        }
    }
}

/**
 * 发送CAN数据
 * @param id CAN扩展ID
 * @param data 8字节数据
 */
static void keya_send_data(uint32_t id, uint8_t* data)
{
    twai_message_t message;
    twai_status_info_t status_info;
    esp_err_t ret;
    
    // 发送前检查CAN总线状态
    ret = twai_get_status_info(&status_info);
    if (ret == ESP_OK) {
        // 检查BUS-OFF状态或错误计数器过高
        if (status_info.state == TWAI_STATE_BUS_OFF || 
            status_info.tx_error_counter > 127 || 
            status_info.rx_error_counter > 127) {
            // 尝试恢复CAN总线
            ESP_LOGW(TAG, "CAN总线处于错误状态，尝试恢复...");
            can_bus_recovery();
            // 恢复后再次检查状态
            ret = twai_get_status_info(&status_info);
            if (ret == ESP_OK && status_info.state == TWAI_STATE_BUS_OFF) {
                ESP_LOGE(TAG, "CAN总线恢复失败，仍处于BUS-OFF状态，无法发送");
                return;  // 无法发送，直接返回
            }
        }
    }
    
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
        // 🔧 优化：如果是队列满错误，根据消息类型决定处理策略
        // ESP_ERR_TIMEOUT通常表示发送队列满
        if (result == ESP_ERR_TIMEOUT) {
            // 判断是否是速度命令（0x23 0x00 0x20）
            // 速度命令需要重试，确保最新速度能发送
            // 使能命令可以跳过，因为不是那么紧急
            bool is_speed_cmd = (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20);
            
            if (is_speed_cmd) {
                // 速度命令：等待一小段时间（1ms），让队列中的旧消息发送出去
                // 然后重试一次，确保最新速度命令能发送
                vTaskDelay(pdMS_TO_TICKS(1));
                result = twai_transmit(&message, 0);
                if (result == ESP_OK) {
                    // 重试成功，静默返回
                    return;
                }
                // 重试仍然失败，记录日志
                ESP_LOGD(TAG, "CAN发送队列满，速度命令重试失败");
            } else {
                // 使能命令：直接跳过，避免阻塞
                ESP_LOGD(TAG, "CAN发送队列满，跳过使能命令");
            }
            return;  // 直接返回，不阻塞
        }
        
        // 其他错误才记录详细信息
        ESP_LOGW(TAG, "CAN发送失败: %s", esp_err_to_name(result));
        
        // 获取并打印CAN状态信息
        ret = twai_get_status_info(&status_info);
        if (ret == ESP_OK) {
            ESP_LOGW(TAG, "CAN状态 - 状态: %" PRIu32 ", TX错误: %" PRIu32 ", RX错误: %" PRIu32,
                     (unsigned long)status_info.state,
                     (unsigned long)status_info.tx_error_counter,
                     (unsigned long)status_info.rx_error_counter);
            
            // 如果错误计数器过高或处于BUS-OFF，尝试恢复
            if (status_info.state == TWAI_STATE_BUS_OFF || 
                status_info.tx_error_counter > 127 || 
                status_info.rx_error_counter > 127) {
                ESP_LOGW(TAG, "检测到CAN总线错误，尝试恢复...");
                can_bus_recovery();
            }
        } else {
            ESP_LOGE(TAG, "无法获取CAN状态信息: %s", esp_err_to_name(ret));
        }
        
        // 打印失败的帧信息，便于调试
        ESP_LOGW(TAG, "CAN发送失败帧: %08" PRIX32 " [%02X %02X %02X %02X %02X %02X %02X %02X]",
                 id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
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
    // 在函数内部创建配置结构体，避免静态变量修改问题
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
    
    // 🔧 优化：增加CAN队列大小，避免高频发送时队列满影响SBUS接收
    // SBUS更新频率71Hz，每次发送4条CAN消息，每秒约284条消息
    // 默认队列大小5太小，容易导致队列满和阻塞
    g_config.tx_queue_len = 20;     // 发送队列增加到20，避免高频发送时队列满
    g_config.rx_queue_len = 20;     // 接收队列增加到20，避免队列满
    // 注意：不设置 intr_flags，使用默认值（因为 CONFIG_TWAI_ISR_IN_IRAM 未启用）
    
    // 初始化TWAI (CAN)
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    // 等待CAN总线稳定（给硬件一些时间初始化）
    vTaskDelay(pdMS_TO_TICKS(100));

    // 创建CAN接收任务，定期清空接收队列
    // 优先级设为5（低优先级），确保电机控制任务（优先级10）的发送操作优先执行
    BaseType_t xReturned = xTaskCreate(
        can_rx_task,
        "can_rx_task",
        2048,           // 栈大小2048字节
        NULL,
        5,              // 优先级5（低优先级）
        &can_rx_task_handle
    );

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN RX task");
        return ESP_FAIL;
    }

    // 初始化恢复计数器
    can_recovery_count = 0;
    
    ESP_LOGI(TAG, "Motor driver initialized");
    ESP_LOGI(TAG, "CAN接收任务已创建 (优先级: 5, TX队列: 20, RX队列: 20)");
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

    // 设置速度命令（每次都需要发送）
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0); // 使能A路(左侧)
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0); // 使能B路(右侧)
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left); // A路(左侧)速度
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right); // B路(右侧)速度

    return 0;
}
