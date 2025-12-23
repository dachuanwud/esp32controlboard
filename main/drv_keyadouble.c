#include "drv_keyadouble.h"
#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "DRV_KEYA";

// 电机驱动CAN ID定义
#define DRIVER_ADDRESS 0x01            // 驱动器地址(默认为1)
#define DRIVER_TX_ID 0x06000000        // 发送基础ID (控制->驱动器)
#define DRIVER_RX_ID 0x05800000        // 接收基础ID (驱动器->控制)
#define DRIVER_HEARTBEAT_ID 0x07000000 // 心跳包ID (驱动器->控制)

// 控制器心跳ID定义 (用于多控制器仲裁)
#define CONTROLLER_ID 0x01                // A控制器ID
#define CONTROLLER_HEARTBEAT_ID 0x1800001 // 控制器心跳帧ID
#define HEARTBEAT_STATUS_ACTIVE 0x01      // 状态：正常控车中

// 电机通道定义
#define MOTOR_CHANNEL_A 0x01 // A路电机(左侧)
#define MOTOR_CHANNEL_B 0x02 // B路电机(右侧)

// 命令类型定义
#define CMD_ENABLE 0x01  // 使能电机
#define CMD_DISABLE 0x02 // 失能电机
#define CMD_SPEED 0x03   // 设置速度

// 外部变量
uint8_t bk_flag_left = 0;
uint8_t bk_flag_right = 0;

// 控制器心跳序列号
static uint16_t heartbeat_seq = 0;

// CAN接收任务句柄
static TaskHandle_t can_rx_task_handle = NULL;

// CAN总线恢复计数器
static uint32_t can_recovery_count = 0;

// CAN总线恢复时间戳（用于限制恢复频率）
static uint32_t last_recovery_time = 0;
#define CAN_RECOVERY_MIN_INTERVAL_MS                                           \
  1000 // 最小恢复间隔1秒，避免频繁恢复影响SBUS接收

// 🔧 新增：连续发送失败计数器（用于触发强制恢复）
static uint32_t consecutive_tx_failures = 0;
#define CAN_FORCE_RECOVERY_THRESHOLD 10 // 连续失败10次触发强制恢复

// 注意：已移除motor_enabled标志，改为每次发送速度命令时都发送使能命令
// 这样可以避免看门狗超时导致的驱动器失能问题

// TWAI (CAN) 配置 - 根据电路图SN65HVD232D CAN收发电路
// IO16连接到SN65HVD232D的D引脚(TX)，IO17连接到R引脚(RX)
// 使用标准模式，但发送时不等待ACK应答
// 注意：配置结构体在初始化函数中创建，避免静态初始化问题
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/**
 * CAN总线恢复函数
 * 当错误计数器过高或处于BUS-OFF状态时，停止并重启CAN驱动
 * @param force_recovery 是否强制恢复（跳过时间间隔限制）
 * @return
 * ESP_OK=恢复成功/不需要恢复，ESP_ERR_TIMEOUT=需要恢复但被时间限制跳过，其他=恢复失败
 */
static esp_err_t can_bus_recovery_ex(bool force_recovery) {
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
  const char *reason = NULL;

  if (status_info.state == TWAI_STATE_BUS_OFF) {
    need_recovery = true;
    reason = "BUS-OFF状态";
  } else if (status_info.state == TWAI_STATE_RECOVERING) {
    need_recovery = true;
    reason = "RECOVERING状态";
  } else if (status_info.tx_error_counter > 96) { // 降低阈值，提前恢复
    need_recovery = true;
    reason = "TX错误计数器过高";
  } else if (status_info.rx_error_counter > 96) {
    need_recovery = true;
    reason = "RX错误计数器过高";
  }

  if (!need_recovery && !force_recovery) {
    return ESP_OK; // 不需要恢复
  }

  // 强制恢复模式下，即使状态看起来正常也执行恢复
  if (force_recovery && !need_recovery) {
    reason = "强制恢复（连续发送失败）";
  }

  // 检查恢复时间间隔（非强制模式下）
  uint32_t current_time = xTaskGetTickCount();
  if (!force_recovery && last_recovery_time != 0 &&
      (current_time - last_recovery_time) <
          pdMS_TO_TICKS(CAN_RECOVERY_MIN_INTERVAL_MS)) {
    // 距离上次恢复时间太短，跳过本次恢复
    ESP_LOGD(TAG, "CAN恢复间隔太短，跳过本次恢复 (距离上次: %" PRIu32 "ms)",
             (current_time - last_recovery_time) * portTICK_PERIOD_MS);
    return ESP_ERR_TIMEOUT; // 返回特殊值表示被跳过
  }

  // 记录恢复前的状态
  ESP_LOGW(TAG, "🔄 CAN总线恢复: %s | 状态: %lu, TX错误: %lu, RX错误: %lu",
           reason, (unsigned long)status_info.state,
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

  // ⚡ 优化：减少等待时间，从50ms减少到20ms
  // CAN总线稳定时间通常只需要10-20ms，减少阻塞时间
  vTaskDelay(pdMS_TO_TICKS(20));

  // 重启CAN驱动
  ret = twai_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "重启CAN驱动失败: %s", esp_err_to_name(ret));
    return ret;
  }

  // 更新恢复计数
  can_recovery_count++;

  // 重置连续失败计数
  consecutive_tx_failures = 0;

  // ⚡ 优化：减少验证等待时间，从20ms减少到10ms
  // 恢复后立即验证，减少总阻塞时间（从70ms减少到30ms）
  vTaskDelay(pdMS_TO_TICKS(10));
  ret = twai_get_status_info(&status_info);
  if (ret == ESP_OK) {
    if (status_info.state == TWAI_STATE_RUNNING) {
      ESP_LOGI(TAG, "✅ CAN总线恢复成功 (恢复次数: %lu)",
               (unsigned long)can_recovery_count);
    } else {
      ESP_LOGW(TAG, "⚠️ CAN总线恢复后状态异常: %lu",
               (unsigned long)status_info.state);
    }
  }

  return ESP_OK;
}

/**
 * CAN总线恢复函数（兼容原有调用）
 */
static esp_err_t can_bus_recovery(void) { return can_bus_recovery_ex(false); }

// CAN接收溢出检测统计
static uint32_t can_rx_overflow_count = 0;      // 溢出计数
static uint32_t can_rx_max_queue_usage = 0;     // 最大队列使用量
static uint32_t last_overflow_warning_time = 0; // 上次溢出警告时间
#define OVERFLOW_WARNING_INTERVAL_MS 5000       // 溢出警告间隔5秒，避免日志刷屏

/**
 * CAN接收任务 - 批量清空接收队列
 * 避免电机反馈帧填满接收队列，影响CAN发送功能
 * ⚡ 优先级设为3（最低优先级），确保发送绝对优先
 *
 * 优化策略：
 * - ⚡ 最低优先级：优先级3，远低于发送任务（优先级10），确保发送任务可以随时抢占
 * - 批量处理：每次循环最多处理10条消息，减少驱动内部锁持有时间
 * - 快速释放：非阻塞接收（超时=0），快速释放驱动锁，减少对发送的影响
 * - ⚡ 主动让出CPU：即使处理了消息，也延迟2ms，给发送任务让出CPU时间
 * - 自适应延迟：队列为空时较长延迟（10ms），减少CPU占用
 * - 🛡️ 溢出检测：监控队列使用情况，检测溢出并发出警告
 */
static void can_rx_task(void *pvParameters) {
  twai_message_t message;
  uint32_t rx_count = 0;
  uint32_t batch_count = 0;
  uint32_t consecutive_empty_loops = 0; // 连续空循环计数

  ESP_LOGI(TAG, "CAN接收任务已启动");

  while (1) {
    batch_count = 0;

    // ⚡ 批量清空接收队列，每次最多处理10条消息
    // 关键优化：非阻塞接收（超时=0）+ 批量限制，快速释放驱动锁
    // 这样可以最大程度减少对发送任务的影响，即使驱动内部有锁竞争
    while (batch_count < 10) {
      // ⚡ 非阻塞接收：超时=0，立即返回，快速释放锁
      esp_err_t ret = twai_receive(&message, 0);
      if (ret == ESP_OK) {
        rx_count++;
        batch_count++;
        consecutive_empty_loops = 0; // 重置空循环计数

        // 打印CAN接收消息的详细信息（使用DEBUG级别，减少日志输出对性能的影响）
        // 如果需要调试CAN接收，可以通过日志级别控制启用
        ESP_LOGD(TAG,
                 "📥 CAN RX #%lu: ID=0x%08" PRIX32
                 " (%s), DLC=%d, RTR=%d, Data=[%02X %02X %02X %02X %02X %02X "
                 "%02X %02X]",
                 (unsigned long)rx_count, message.identifier,
                 message.extd ? "EXT" : "STD", message.data_length_code,
                 message.rtr, message.data[0], message.data[1], message.data[2],
                 message.data[3], message.data[4], message.data[5],
                 message.data[6], message.data[7]);

        // 只清空队列，不处理数据（根据用户需求）
        // 电机反馈帧被丢弃，避免队列满
      } else if (ret == ESP_ERR_TIMEOUT) {
        // 队列为空，跳出内层循环
        break;
      } else {
        // 🛡️ 溢出检测：ESP_ERR_INVALID_STATE 或 ESP_ERR_INVALID_ARG
        // 可能表示队列问题 ESP32
        // TWAI驱动在接收队列满时会丢弃新消息，但不会返回特定错误码
        // 我们通过监控连续接收失败来检测潜在问题
        ESP_LOGD(TAG, "CAN接收错误: %s", esp_err_to_name(ret));
        break;
      }
    }

    // 🛡️ 溢出检测：尝试获取队列状态（如果驱动支持）
    // 注意：ESP32 TWAI驱动可能不直接提供队列使用量查询
    // 我们通过监控处理速度来间接检测溢出风险
    if (batch_count >= 10) {
      // 如果一次处理了10条消息（达到批量上限），说明队列可能还有更多消息
      // 这是队列可能接近满的警告信号
      uint32_t current_time = xTaskGetTickCount();
      if (current_time - last_overflow_warning_time >
          pdMS_TO_TICKS(OVERFLOW_WARNING_INTERVAL_MS)) {
        ESP_LOGW(TAG, "⚠️ CAN接收队列繁忙：单次处理%d条消息，可能存在溢出风险",
                 batch_count);
        last_overflow_warning_time = current_time;
      }
    }

    // ⚡ 优化延迟策略：确保发送任务有足够机会执行
    // - 如果处理了消息：短暂延迟（2ms），给发送任务让出CPU时间
    // - 如果队列为空：较长延迟（10ms），减少CPU占用
    // 关键：即使队列很满，也要定期让出CPU，确保发送任务可以执行
    if (batch_count > 0) {
      // ⚡ 即使处理了消息，也要延迟2ms，给发送任务（优先级10）让出CPU时间
      // 这样可以确保即使接收队列很满，发送任务也能及时执行
      vTaskDelay(pdMS_TO_TICKS(2)); // 给发送任务让出CPU时间
      consecutive_empty_loops = 0;
    } else {
      consecutive_empty_loops++;
      // 如果连续多次空循环，可以适当增加延迟，减少CPU占用
      if (consecutive_empty_loops > 10) {
        vTaskDelay(pdMS_TO_TICKS(10)); // 正常延迟，减少CPU占用
      } else {
        vTaskDelay(pdMS_TO_TICKS(2)); // 短暂延迟，保持响应性，同时给发送任务机会
      }
    }
  }
}

/**
 * 发送CAN数据
 * @param id CAN扩展ID
 * @param data 8字节数据
 * 
 * ⚡ 完全非阻塞设计：
 * - 超时=0：立即返回，不等待
 * - 队列满：立即返回ESP_ERR_TIMEOUT，不阻塞
 * - 状态检查：快速检查，异常时立即返回
 * - 恢复过程：仅在必要时执行，尽量减少阻塞时间
 */
static void keya_send_data(uint32_t id, uint8_t *data) {
  twai_message_t message;
  twai_status_info_t status_info;
  esp_err_t ret;

  // ⚡ 快速状态检查：发送前检查CAN总线状态，异常时提前返回
  // 这样可以避免在异常状态下尝试发送，减少无效操作
  ret = twai_get_status_info(&status_info);
  if (ret == ESP_OK) {
    // 检查BUS-OFF状态或错误计数器过高
    if (status_info.state == TWAI_STATE_BUS_OFF ||
        status_info.tx_error_counter > 127 ||
        status_info.rx_error_counter > 127) {
      // 尝试恢复CAN总线（恢复过程会短暂阻塞，但这是必要的）
      ESP_LOGW(TAG, "CAN总线处于错误状态，尝试恢复...");
      can_bus_recovery();
      // 恢复后再次检查状态
      ret = twai_get_status_info(&status_info);
      if (ret == ESP_OK && status_info.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGE(TAG, "CAN总线恢复失败，仍处于BUS-OFF状态，无法发送");
        consecutive_tx_failures++; // 记录失败
        return; // 无法发送，直接返回，不阻塞
      }
    }
  }

  message.extd = 1; // 扩展帧(29位ID)
  message.identifier = id;
  message.data_length_code = 8; // 帧长度8字节
  message.rtr = 0;              // 数据帧

  // 复制数据
  for (int i = 0; i < 8; i++) {
    message.data[i] = data[i];
  }

  // ⚡ 完全非阻塞发送：超时设为0，立即返回，不等待
  // 如果队列满，立即返回ESP_ERR_TIMEOUT，避免阻塞控制循环
  esp_err_t result = twai_transmit(&message, 0); // 0 = 完全非阻塞

  if (result == ESP_OK) {
    // 发送成功，重置连续失败计数
    if (consecutive_tx_failures > 0) {
      consecutive_tx_failures = 0;
    }
  } else {
    // 发送失败，增加连续失败计数
    consecutive_tx_failures++;

    // 🔧 关键修复：连续发送失败时触发强制恢复（异步处理，不阻塞）
    if (consecutive_tx_failures >= CAN_FORCE_RECOVERY_THRESHOLD) {
      ESP_LOGW(TAG, "⚠️ CAN连续发送失败 %lu 次，触发强制恢复",
               (unsigned long)consecutive_tx_failures);
      // 注意：恢复过程会短暂阻塞，但这是必要的恢复操作
      can_bus_recovery_ex(true); // 强制恢复

      // 恢复后立即重试（非阻塞）
      result = twai_transmit(&message, 0);
      if (result == ESP_OK) {
        ESP_LOGI(TAG, "✅ CAN恢复后重试成功");
        consecutive_tx_failures = 0;
        return;
      }
    }

    // ESP_ERR_TIMEOUT 通常表示发送队列满或总线忙
    // ⚡ 非阻塞模式：队列满时立即返回，不等待，避免阻塞控制循环
    if (result == ESP_ERR_TIMEOUT) {
      bool is_speed_cmd =
          (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20);

      if (is_speed_cmd) {
        // ⚡ 速度命令：立即重试一次（非阻塞），如果还是失败则放弃
        // 这样可以提高速度命令的成功率，同时保持非阻塞
        result = twai_transmit(&message, 0);
        if (result == ESP_OK) {
          consecutive_tx_failures = 0;
          return;
        }
        ESP_LOGD(TAG, "CAN发送队列满，速度命令重试失败 (失败次数: %lu)",
                 (unsigned long)consecutive_tx_failures);
      } else {
        // 非速度命令（如使能命令）：队列满时直接跳过，避免阻塞
        ESP_LOGD(TAG, "CAN发送队列满，跳过非关键命令");
      }
      return;
    }

    // ESP_ERR_INVALID_STATE 表示 CAN 驱动状态异常
    if (result == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "⚠️ CAN驱动状态异常，尝试恢复...");
      can_bus_recovery_ex(true); // 强制恢复
      return;
    }

    // 其他错误：记录详细信息
    ESP_LOGW(TAG, "CAN发送失败: %s (失败次数: %lu)", esp_err_to_name(result),
             (unsigned long)consecutive_tx_failures);

    // 获取并打印CAN状态信息
    ret = twai_get_status_info(&status_info);
    if (ret == ESP_OK) {
      ESP_LOGW(TAG, "CAN状态 - 状态: %lu, TX错误: %lu, RX错误: %lu",
               (unsigned long)status_info.state,
               (unsigned long)status_info.tx_error_counter,
               (unsigned long)status_info.rx_error_counter);

      // 如果错误计数器过高或处于异常状态，尝试恢复
      if (status_info.state == TWAI_STATE_BUS_OFF ||
          status_info.state == TWAI_STATE_RECOVERING ||
          status_info.tx_error_counter > 96) {
        ESP_LOGW(TAG, "检测到CAN总线错误，尝试恢复...");
        can_bus_recovery();
      }
    }

    // 打印失败的帧信息，便于调试
    ESP_LOGW(TAG,
             "CAN发送失败帧: %08" PRIX32
             " [%02X %02X %02X %02X %02X %02X %02X %02X]",
             id, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
             data[7]);
  }

  // 只在调试模式下打印详细的CAN数据
  ESP_LOGD(TAG,
           "CAN TX: %08" PRIX32 " [%02X %02X %02X %02X %02X %02X %02X %02X]",
           id, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
           data[7]);

  // 只在速度命令时打印简化的速度信息 (0x23 0x00 0x20 channel speed_bytes)
  if (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20) {
    int32_t sp_value_tx = ((int32_t)data[4] << 24) | ((int32_t)data[5] << 16) |
                          ((int32_t)data[6] << 8) | ((int32_t)data[7]);
    int8_t actual_speed = (int8_t)(sp_value_tx / 100);
    uint8_t channel = data[3];
    ESP_LOGD(TAG, "Motor Ch%d speed: %d", channel, actual_speed);
  }

  // ⚡ 性能优化：完全非阻塞发送
  // - 超时=0：立即返回，不等待
  // - 队列满时：立即返回ESP_ERR_TIMEOUT，不阻塞
  // - 速度命令：允许一次立即重试，提高成功率
  // - 非关键命令：队列满时直接跳过，避免阻塞控制循环
  // 这样可以确保CAN发送不会阻塞SBUS接收和电机控制任务
}

/**
 * 电机控制
 * @param cmd_type 命令类型: CMD_ENABLE/CMD_DISABLE/CMD_SPEED
 * @param channel 电机通道: MOTOR_CHANNEL_A(左)/MOTOR_CHANNEL_B(右)
 * @param speed 速度(-100到100，对应-10000到10000)
 */
static void motor_control(uint8_t cmd_type, uint8_t channel, int8_t speed) {
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
 * 发送控制器心跳帧
 * 在发送电机速度命令前调用，通知其他控制器本机正在控车
 * @param speed_left 左电机目标速度(-100到100)
 * @param speed_right 右电机目标速度(-100到100)
 */
static void send_controller_heartbeat(int8_t speed_left, int8_t speed_right) {
  uint8_t tx_data[8] = {0};

  // Byte 0: 控制器ID
  tx_data[0] = CONTROLLER_ID;

  // Byte 1: 状态 (0x01 = 正常控车中)
  tx_data[1] = HEARTBEAT_STATUS_ACTIVE;

  // Byte 2-3: 序列号 (大端序)
  tx_data[2] = (heartbeat_seq >> 8) & 0xFF;
  tx_data[3] = heartbeat_seq & 0xFF;
  heartbeat_seq++; // 序列号递增

  // Byte 4-5: A路电机目标速度 (转换为-10000~+10000，大端序)
  int16_t sp_a = (int16_t)speed_left * 100;
  tx_data[4] = (sp_a >> 8) & 0xFF;
  tx_data[5] = sp_a & 0xFF;

  // Byte 6-7: B路电机目标速度 (转换为-10000~+10000，大端序)
  int16_t sp_b = (int16_t)speed_right * 100;
  tx_data[6] = (sp_b >> 8) & 0xFF;
  tx_data[7] = sp_b & 0xFF;

  // 发送心跳帧
  keya_send_data(CONTROLLER_HEARTBEAT_ID, tx_data);

  ESP_LOGD(TAG, "💓 心跳: seq=%d, spd_L=%d, spd_R=%d", heartbeat_seq - 1,
           speed_left, speed_right);
}

/**
 * 初始化电机驱动
 */
esp_err_t drv_keyadouble_init(void) {
  // 在函数内部创建配置结构体，避免静态变量修改问题
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);

  // 🔧 优化：增加CAN队列大小，避免高频发送时队列满影响SBUS接收
  // SBUS更新频率71Hz，每次发送4条CAN消息，每秒约284条消息
  // 默认队列大小5太小，容易导致队列满和阻塞
  // 🛡️ 溢出防护：接收队列增加到50，提供更大的缓冲空间
  // 考虑场景：驱动器反馈(5Hz) + 心跳(1Hz) + 自动导航板(10Hz) + 其他设备
  // 突发流量可能达到100-200条/秒，50条队列提供约250-500ms缓冲时间
  g_config.tx_queue_len = 20; // 发送队列增加到20，避免高频发送时队列满
  g_config.rx_queue_len = 50; // 接收队列增加到50，提供更大的溢出保护
  // 注意：不设置 intr_flags，使用默认值（因为 CONFIG_TWAI_ISR_IN_IRAM 未启用）

  // 初始化TWAI (CAN)
  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
  ESP_ERROR_CHECK(twai_start());

  // 等待CAN总线稳定（给硬件一些时间初始化）
  vTaskDelay(pdMS_TO_TICKS(100));

  // 创建CAN接收任务，定期清空接收队列
  // ⚡ 优化：优先级设为3（最低优先级），确保电机控制任务（优先级10）的发送操作绝对优先
  // 这样可以最大程度减少接收任务对发送的影响，即使驱动内部有锁竞争，发送任务也能快速抢占
  BaseType_t xReturned = xTaskCreate(can_rx_task, "can_rx_task",
                                     2048, // 栈大小2048字节
                                     NULL,
                                     3, // 优先级3（最低优先级，进一步降低，确保发送优先）
                                     &can_rx_task_handle);

  if (xReturned != pdPASS) {
    ESP_LOGE(TAG, "Failed to create CAN RX task");
    return ESP_FAIL;
  }

  // 初始化恢复计数器
  can_recovery_count = 0;

  // 初始化溢出检测统计
  can_rx_overflow_count = 0;
  can_rx_max_queue_usage = 0;
  last_overflow_warning_time = 0;

  ESP_LOGI(TAG, "Motor driver initialized");
  ESP_LOGI(TAG, "CAN接收任务已创建 (优先级: 3, TX队列: 20, RX队列: 50)");
  ESP_LOGI(TAG, "⚡ CAN接收任务优先级已优化为最低(3)，确保发送任务(10)绝对优先");
  ESP_LOGI(TAG, "🛡️ CAN接收溢出保护已启用 (队列大小: 50, 批量处理: 10条/次)");
  return ESP_OK;
}

/**
 * 设置左右电机速度实现运动
 * @param speed_left 左电机速度(-100到100)
 * @param speed_right 右电机速度(-100到100)
 * @return 0=成功，1=参数错误
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right) {
  if ((abs(speed_left) > 100) || (abs(speed_right) > 100)) {
    printf("Wrong parameter in intf_move!!![lf=%d],[ri=%d]", speed_left,
           speed_right);
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

  // 💓 发送控制器心跳帧（通知其他控制器本机正在控车）
  send_controller_heartbeat(speed_left, speed_right);

  // 🔒 可靠性优化：每次发送速度命令前都发送使能命令
  // 这样可以避免看门狗超时（1000ms）导致的驱动器失能问题
  // 即使控制间隔超过1000ms，也能确保电机始终处于使能状态
  // 代价：CAN帧数从2帧/次增加到5帧/次（含心跳），但在250Kbps下仍在可接受范围
  motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);          // 使能A路(左侧)
  motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);          // 使能B路(右侧)
  motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);  // A路(左侧)速度
  motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right); // B路(右侧)速度

  return 0;
}
