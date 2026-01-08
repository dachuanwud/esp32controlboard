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
// 使用标准模式，并启用ACK应答 (TWAI_MODE_NORMAL)
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
  } else if (status_info.tx_error_counter > 127) {
    need_recovery = true;
    reason = "TX错误计数器过高";
  } else if (status_info.rx_error_counter > 127) {
    need_recovery = true;
    reason = "RX错误计数器过高";
  }

  if (!need_recovery && !force_recovery) {
    return ESP_OK; // 不需要恢复
  }

  // 冷却时间检查
  uint32_t current_time = xTaskGetTickCount();
  // 🛠️ 优化：对于 BUS-OFF 状态，缩短恢复间隔到 200ms，以便尽快恢复通信
  uint32_t min_interval_ms = (status_info.state == TWAI_STATE_BUS_OFF)
                                 ? 200
                                 : CAN_RECOVERY_MIN_INTERVAL_MS;

  if (!force_recovery && last_recovery_time != 0 &&
      (current_time - last_recovery_time) < pdMS_TO_TICKS(min_interval_ms)) {
    // 距离上次尝试恢复时间太短，跳过本次恢复
    return ESP_ERR_TIMEOUT;
  }

  // 记录恢复前的状态
  ESP_LOGW(TAG, "🔄 CAN总线触发恢复: 原因=%s | 状态=%d, TXERR=%lu, RXERR=%lu",
           reason ? reason : "强制恢复", (int)status_info.state,
           (unsigned long)status_info.tx_error_counter,
           (unsigned long)status_info.rx_error_counter);

  // 更新恢复时间戳
  last_recovery_time = current_time;

  // 🛡️ 优化：如果处于 BUS-OFF，先调用官方推荐的恢复启动函数
  if (status_info.state == TWAI_STATE_BUS_OFF) {
    ESP_LOGI(TAG, "Initiating TWAI bus recovery...");
    twai_initiate_recovery();
    int wait = 0;
    while (wait < 100) { // 最多等待1秒
      vTaskDelay(pdMS_TO_TICKS(10));
      twai_get_status_info(&status_info);
      if (status_info.state == TWAI_STATE_STOPPED)
        break;
      wait += 10;
    }
  }

  // 停止CAN驱动
  ret = twai_stop();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "停止CAN驱动失败: %s", esp_err_to_name(ret));
    // 即使失败也尝试启动，或者进行硬启动
  }

  vTaskDelay(pdMS_TO_TICKS(20));

  // 重启CAN驱动
  ret = twai_start();
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Start failed, performing driver reinstall...");
    twai_driver_uninstall();
    twai_general_config_t gc =
        TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);
    gc.tx_queue_len = 20;
    gc.rx_queue_len = 50;
    twai_driver_install(&gc, &t_config, &f_config);
    ret = twai_start();
  }

  if (ret == ESP_OK) {
    can_recovery_count++;
    consecutive_tx_failures = 0;
    twai_get_status_info(&status_info);
    ESP_LOGI(TAG, "✅ CAN总线已恢复 (次数:%lu, TXErr:%lu, RXErr:%lu)",
             (unsigned long)can_recovery_count,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter);
  }

  return ret;
}

/**
 * CAN总线恢复函数（兼容原有调用）
 */
static esp_err_t can_bus_recovery(void) { return can_bus_recovery_ex(false); }

// CAN接收溢出检测统计
static uint32_t can_rx_overflow_count = 0;      // 溢出计数
static uint32_t last_overflow_warning_time = 0; // 上次溢出警告时间
#define OVERFLOW_WARNING_INTERVAL_MS 5000       // 溢出警告间隔5秒

/**
 * CAN接收任务 - 批量清空接收队列
 */
static void can_rx_task(void *pvParameters) {
  twai_message_t message;
  uint32_t rx_count = 0;
  uint32_t batch_count = 0;
  uint32_t consecutive_empty_loops = 0;

  ESP_LOGI(TAG, "CAN接收任务已启动");

  while (1) {
    batch_count = 0;

    while (batch_count < 10) {
      esp_err_t ret = twai_receive(&message, 0);
      if (ret == ESP_OK) {
        rx_count++;
        batch_count++;
        consecutive_empty_loops = 0;

        ESP_LOGD(TAG, "📥 CAN RX #%lu: ID=0x%08" PRIX32 "...",
                 (unsigned long)rx_count, message.identifier);
      } else if (ret == ESP_ERR_TIMEOUT) {
        break;
      } else {
        ESP_LOGD(TAG, "CAN接收错误: %s", esp_err_to_name(ret));
        break;
      }
    }

    if (batch_count > 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      consecutive_empty_loops = 0;
    } else {
      consecutive_empty_loops++;
      if (consecutive_empty_loops > 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
      } else {
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    }
  }
}

// 🔧 调试：CAN发送统计
static uint32_t can_tx_success_count = 0;
static uint32_t can_tx_timeout_count = 0;
static uint32_t can_tx_error_count = 0;
static uint32_t last_status_print_time = 0;
#define CAN_STATUS_PRINT_INTERVAL_MS 5000 // 每5秒打印一次状态

/**
 * 发送CAN数据
 */
static void keya_send_data(uint32_t id, uint8_t *data) {
  twai_message_t message;
  twai_status_info_t status_info;
  esp_err_t ret;

  ret = twai_get_status_info(&status_info);

  // 🔧 调试：定期打印CAN状态
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  if (current_time - last_status_print_time > CAN_STATUS_PRINT_INTERVAL_MS) {
    last_status_print_time = current_time;
    ESP_LOGI(TAG, "📊 CAN状态: State=%d, TXErr=%lu, RXErr=%lu, TXQ=%lu, RXQ=%lu | 发送统计: OK=%lu, TIMEOUT=%lu, ERR=%lu",
             (int)status_info.state,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter,
             (unsigned long)status_info.msgs_to_tx,
             (unsigned long)status_info.msgs_to_rx,
             (unsigned long)can_tx_success_count,
             (unsigned long)can_tx_timeout_count,
             (unsigned long)can_tx_error_count);
  }

  if (ret == ESP_OK) {
    if (status_info.state == TWAI_STATE_BUS_OFF ||
        status_info.tx_error_counter > 127 ||
        status_info.rx_error_counter > 127) {

      ESP_LOGW(TAG, "⚠️ CAN异常状态检测: State=%d, TXErr=%lu, RXErr=%lu",
               (int)status_info.state,
               (unsigned long)status_info.tx_error_counter,
               (unsigned long)status_info.rx_error_counter);

      esp_err_t recovery_ret = can_bus_recovery();

      if (recovery_ret == ESP_OK) {
        twai_get_status_info(&status_info);
        if (status_info.state == TWAI_STATE_BUS_OFF) {
          ESP_LOGE(TAG, "CAN总线恢复失败，仍处于BUS-OFF状态，无法发送");
          consecutive_tx_failures++;
          can_tx_error_count++;
          return;
        }
      } else if (recovery_ret == ESP_ERR_TIMEOUT) {
        ESP_LOGD(TAG, "CAN恢复被冷却时间跳过");
        return;
      }
    }
  }

  message.extd = 1;
  message.identifier = id;
  message.data_length_code = 8;
  message.rtr = 0;

  for (int i = 0; i < 8; i++) {
    message.data[i] = data[i];
  }

  // 🔧 调试：检查TX队列是否满
  if (status_info.msgs_to_tx >= 18) {  // 队列长度20，接近满时警告
    ESP_LOGW(TAG, "⚠️ CAN TX队列接近满: %lu/20", (unsigned long)status_info.msgs_to_tx);
  }

  esp_err_t result = twai_transmit(&message, 0);

  if (result == ESP_OK) {
    can_tx_success_count++;
    if (consecutive_tx_failures > 0) {
      ESP_LOGI(TAG, "✅ CAN发送恢复正常 (之前失败%lu次)", (unsigned long)consecutive_tx_failures);
      consecutive_tx_failures = 0;
    }
  } else {
    consecutive_tx_failures++;

    if (consecutive_tx_failures >= CAN_FORCE_RECOVERY_THRESHOLD) {
      ESP_LOGW(TAG, "⚠️ CAN连续发送失败 %lu 次，触发强制恢复",
               (unsigned long)consecutive_tx_failures);
      can_bus_recovery_ex(true);
      result = twai_transmit(&message, 0);
      if (result == ESP_OK) {
        ESP_LOGI(TAG, "✅ CAN恢复后重试成功");
        consecutive_tx_failures = 0;
        can_tx_success_count++;
        return;
      }
    }

    if (result == ESP_ERR_TIMEOUT) {
      can_tx_timeout_count++;
      // 🔧 调试：每10次TIMEOUT打印一次
      if (can_tx_timeout_count % 10 == 1) {
        ESP_LOGW(TAG, "⏱️ CAN发送TIMEOUT (累计%lu次), ID=0x%08lX, TXQ=%lu",
                 (unsigned long)can_tx_timeout_count,
                 (unsigned long)id,
                 (unsigned long)status_info.msgs_to_tx);
      }
      bool is_speed_cmd =
          (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20);
      if (is_speed_cmd) {
        twai_transmit(&message, 0);
      }
      return;
    }

    if (result == ESP_ERR_INVALID_STATE) {
      can_tx_error_count++;
      ESP_LOGE(TAG, "❌ CAN INVALID_STATE, 触发恢复");
      can_bus_recovery_ex(true);
      return;
    }

    can_tx_error_count++;
    ESP_LOGW(TAG, "❌ CAN发送失败: %s, ID=0x%08lX", esp_err_to_name(result), (unsigned long)id);
  }
}

/**
 * 电机控制
 */
static void motor_control(uint8_t cmd_type, uint8_t channel, int8_t speed) {
  uint8_t tx_data[8] = {0};
  uint32_t tx_id = DRIVER_TX_ID + DRIVER_ADDRESS;

  if (cmd_type == CMD_ENABLE) {
    tx_data[0] = 0x23;
    tx_data[1] = 0x0D;
    tx_data[2] = 0x20;
    tx_data[3] = channel;
  } else if (cmd_type == CMD_DISABLE) {
    tx_data[0] = 0x23;
    tx_data[1] = 0x0C;
    tx_data[2] = 0x20;
    tx_data[3] = channel;
  } else if (cmd_type == CMD_SPEED) {
    tx_data[0] = 0x23;
    tx_data[1] = 0x00;
    tx_data[2] = 0x20;
    tx_data[3] = channel;
    int32_t sp_value = (int32_t)speed * 100;
    tx_data[4] = (sp_value >> 24) & 0xFF;
    tx_data[5] = (sp_value >> 16) & 0xFF;
    tx_data[6] = (sp_value >> 8) & 0xFF;
    tx_data[7] = sp_value & 0xFF;
  }

  keya_send_data(tx_id, tx_data);
}

/**
 * 发送控制器心跳帧
 */
static void send_controller_heartbeat(int8_t speed_left, int8_t speed_right) {
  uint8_t tx_data[8] = {0};
  tx_data[0] = CONTROLLER_ID;
  tx_data[1] = HEARTBEAT_STATUS_ACTIVE;
  tx_data[2] = (heartbeat_seq >> 8) & 0xFF;
  tx_data[3] = heartbeat_seq & 0xFF;
  heartbeat_seq++;

  int16_t sp_a = (int16_t)speed_left * 100;
  tx_data[4] = (sp_a >> 8) & 0xFF;
  tx_data[5] = sp_a & 0xFF;

  int16_t sp_b = (int16_t)speed_right * 100;
  tx_data[6] = (sp_b >> 8) & 0xFF;
  tx_data[7] = sp_b & 0xFF;

  keya_send_data(CONTROLLER_HEARTBEAT_ID, tx_data);
}

/**
 * 初始化电机驱动
 */
esp_err_t drv_keyadouble_init(void) {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, TWAI_MODE_NORMAL);

  g_config.tx_queue_len = 20;
  g_config.rx_queue_len = 50;

  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
  ESP_ERROR_CHECK(twai_start());

  vTaskDelay(pdMS_TO_TICKS(100));

  // 🔧 优化：提高 CAN RX 任务优先级到 8，确保及时清空接收队列
  xTaskCreate(can_rx_task, "can_rx_task", 2048, NULL, 8, &can_rx_task_handle);

  can_recovery_count = 0;

  // 初始化统计计数器
  can_tx_success_count = 0;
  can_tx_timeout_count = 0;
  can_tx_error_count = 0;
  last_status_print_time = 0;

  ESP_LOGI(TAG, "Motor driver initialized (Normal Mode, Priority 8 RX Task)");
  ESP_LOGI(TAG, "📊 CAN配置: TX_Q=%d, RX_Q=%d, 250kbps, GPIO16/17",
           g_config.tx_queue_len, g_config.rx_queue_len);
  return ESP_OK;
}

/**
 * 打印CAN诊断信息（可从外部调用）
 */
void drv_keyadouble_print_diag(void) {
  twai_status_info_t status_info;
  if (twai_get_status_info(&status_info) == ESP_OK) {
    const char* state_str = "UNKNOWN";
    switch(status_info.state) {
      case TWAI_STATE_STOPPED: state_str = "STOPPED"; break;
      case TWAI_STATE_RUNNING: state_str = "RUNNING"; break;
      case TWAI_STATE_BUS_OFF: state_str = "BUS_OFF"; break;
      case TWAI_STATE_RECOVERING: state_str = "RECOVERING"; break;
    }
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "📊 CAN诊断信息");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "状态: %s (%d)", state_str, status_info.state);
    ESP_LOGI(TAG, "TX错误计数: %lu (>127触发恢复, >255=BUS_OFF)",
             (unsigned long)status_info.tx_error_counter);
    ESP_LOGI(TAG, "RX错误计数: %lu", (unsigned long)status_info.rx_error_counter);
    ESP_LOGI(TAG, "TX队列待发: %lu/20", (unsigned long)status_info.msgs_to_tx);
    ESP_LOGI(TAG, "RX队列待收: %lu/50", (unsigned long)status_info.msgs_to_rx);
    ESP_LOGI(TAG, "TX失败次数: %lu", (unsigned long)status_info.tx_failed_count);
    ESP_LOGI(TAG, "RX丢失次数: %lu", (unsigned long)status_info.rx_missed_count);
    ESP_LOGI(TAG, "仲裁丢失: %lu", (unsigned long)status_info.arb_lost_count);
    ESP_LOGI(TAG, "总线错误: %lu", (unsigned long)status_info.bus_error_count);
    ESP_LOGI(TAG, "───────────────────────────────────────────");
    ESP_LOGI(TAG, "发送统计: 成功=%lu, TIMEOUT=%lu, 错误=%lu",
             (unsigned long)can_tx_success_count,
             (unsigned long)can_tx_timeout_count,
             (unsigned long)can_tx_error_count);
    ESP_LOGI(TAG, "恢复次数: %lu", (unsigned long)can_recovery_count);
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
  }
}

/**
 * 设置左右电机速度实现运动
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right) {
  if ((abs(speed_left) > 100) || (abs(speed_right) > 100))
    return 1;

  bk_flag_left = (speed_left != 0) ? 1 : 0;
  bk_flag_right = (speed_right != 0) ? 1 : 0;

  // 🔧 优化：检查TX错误计数器，在接近BUS-OFF阈值前主动重置CAN控制器
  // 心跳帧无ACK会导致TX错误+8，当计数器接近255时主动重置清零
  twai_status_info_t status_info;
  if (twai_get_status_info(&status_info) == ESP_OK) {
    // 当TX错误计数器 > 200 时，主动重置CAN控制器，清零错误计数器
    if (status_info.tx_error_counter > 200) {
      ESP_LOGW(TAG, "⚠️ TX错误计数器过高(%lu)，主动重置CAN控制器",
               (unsigned long)status_info.tx_error_counter);
      twai_stop();
      vTaskDelay(pdMS_TO_TICKS(10));
      twai_start();
    }
  }


  send_controller_heartbeat(speed_left, speed_right);

  motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);
  motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);
  motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);
  motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right);

  return 0;
}
