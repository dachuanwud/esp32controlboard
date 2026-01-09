#include "drv_keyadouble.h"
#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_private/periph_ctrl.h"  // 用于外设复位

static const char *TAG = "DRV_KEYA";

static void send_controller_heartbeat(int8_t speed_left, int8_t speed_right);

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
  300 // 最小恢复间隔300ms，保证快速恢复

// 🔧 新增：连续发送失败计数器（用于触发强制恢复）
static uint32_t consecutive_tx_failures = 0;
#define CAN_FORCE_RECOVERY_THRESHOLD 10 // 连续失败10次触发强制恢复

// 🔧 新增：连续恢复失败计数器（用于暂停恢复尝试）
static uint32_t consecutive_recovery_failures = 0;
static uint32_t recovery_pause_until = 0;  // 暂停恢复直到此时间
#define CAN_MAX_RECOVERY_FAILURES 5       // 连续5次恢复失败后暂停
#define CAN_RECOVERY_PAUSE_MS 30000       // 暂停30秒

// 注意：已移除motor_enabled标志，改为每次发送速度命令时都发送使能命令
// 这样可以避免看门狗超时导致的驱动器失能问题

// TWAI (CAN) 配置 - 根据电路图SN65HVD232D CAN收发电路
// IO16连接到SN65HVD232D的D引脚(TX)，IO17连接到R引脚(RX)
// 使用NO_ACK模式，不等待ACK应答，避免错误计数器累积
// 注意：配置结构体在初始化函数中创建，避免静态初始化问题
#define CAN_MODE TWAI_MODE_NO_ACK  // 改为NO_ACK模式
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
#define CAN_RECOVERY_BUDGET_MS 300

// 🔧 标记驱动是否已安装（用于跟踪状态）
static bool twai_driver_installed = false;

static esp_err_t can_hw_reset_and_reinit(void) {
  ESP_LOGW(TAG, "🧯 硬复位TWAI外设并重装驱动");
  esp_err_t ret;
  
  // 获取当前状态
  twai_status_info_t status_info;
  ret = twai_get_status_info(&status_info);
  bool status_ok = (ret == ESP_OK);
  
  if (status_ok) {
    ESP_LOGI(TAG, "当前TWAI状态: State=%d, TXErr=%lu, RXErr=%lu",
             (int)status_info.state,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter);
  }
  
  // 🔧 关键修复：在RECOVERING状态下，等待其完成或超时
  if (status_ok && status_info.state == TWAI_STATE_RECOVERING) {
    ESP_LOGI(TAG, "等待RECOVERING状态结束...");
    uint32_t wait_start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - wait_start) < pdMS_TO_TICKS(500)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      ret = twai_get_status_info(&status_info);
      if (ret != ESP_OK || status_info.state != TWAI_STATE_RECOVERING) {
        break;
      }
    }
    // 再次获取状态
    ret = twai_get_status_info(&status_info);
    status_ok = (ret == ESP_OK);
    if (status_ok) {
      ESP_LOGI(TAG, "等待后TWAI状态: State=%d", (int)status_info.state);
    }
  }
  
  // 尝试正常流程：stop -> uninstall
  if (twai_driver_installed) {
    // 只有在非RECOVERING状态下才能stop
    if (!status_ok || status_info.state != TWAI_STATE_RECOVERING) {
      ret = twai_stop();
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "twai_stop 成功");
        vTaskDelay(pdMS_TO_TICKS(10));
        
        ret = twai_driver_uninstall();
        if (ret == ESP_OK) {
          ESP_LOGI(TAG, "twai_driver_uninstall 成功");
          twai_driver_installed = false;
        } else {
          ESP_LOGW(TAG, "twai_driver_uninstall 失败: %s", esp_err_to_name(ret));
        }
      } else {
        ESP_LOGW(TAG, "twai_stop 失败: %s", esp_err_to_name(ret));
      }
    }
  }
  
  // 🔧 如果驱动仍然安装着（卸载失败），使用激进方法
  if (twai_driver_installed) {
    ESP_LOGW(TAG, "⚠️ 正常卸载失败，尝试强制复位...");
    
    // 强制禁用外设时钟，这会使驱动状态无效
    periph_module_disable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 复位外设
    periph_module_reset(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 重新启用
    periph_module_enable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 🔧 关键：此时驱动内部状态已损坏，需要标记为未安装
    // ESP-IDF内部可能仍认为驱动已安装，但外设已被复位
    // 尝试直接安装，如果失败则说明需要更激进的处理
    twai_driver_installed = false;
  }
  
  // 安装驱动
  twai_general_config_t gc =
      TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, CAN_MODE);
  gc.tx_queue_len = 20;
  gc.rx_queue_len = 50;

  ret = twai_driver_install(&gc, &t_config, &f_config);
  if (ret == ESP_ERR_INVALID_STATE) {
    // 🔧 驱动认为自己仍然安装着，尝试强制卸载
    ESP_LOGW(TAG, "驱动状态冲突，尝试强制卸载后重装...");
    
    // 再次尝试卸载（可能在复位后状态变了）
    (void)twai_stop();
    vTaskDelay(pdMS_TO_TICKS(20));
    (void)twai_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // 完全复位外设
    periph_module_disable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));
    periph_module_reset(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));
    periph_module_enable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = twai_driver_install(&gc, &t_config, &f_config);
  }
  
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "安装TWAI驱动失败: %s", esp_err_to_name(ret));
    consecutive_recovery_failures++;
    if (consecutive_recovery_failures >= CAN_MAX_RECOVERY_FAILURES) {
      recovery_pause_until = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_RECOVERY_PAUSE_MS);
      ESP_LOGE(TAG, "🛑 CAN恢复连续失败%lu次，暂停恢复%d秒",
               (unsigned long)consecutive_recovery_failures,
               CAN_RECOVERY_PAUSE_MS / 1000);
    }
    return ret;
  }
  
  twai_driver_installed = true;
  
  ret = twai_start();
  if (ret == ESP_OK) {
    can_recovery_count++;
    consecutive_tx_failures = 0;
    consecutive_recovery_failures = 0;  // 🔧 恢复成功，重置失败计数
    ESP_LOGI(TAG, "✅ TWAI硬复位恢复成功 (次数:%lu)", (unsigned long)can_recovery_count);
  } else {
    ESP_LOGE(TAG, "硬复位后启动TWAI失败: %s", esp_err_to_name(ret));
    consecutive_recovery_failures++;
    if (consecutive_recovery_failures >= CAN_MAX_RECOVERY_FAILURES) {
      recovery_pause_until = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_RECOVERY_PAUSE_MS);
      ESP_LOGE(TAG, "🛑 CAN恢复连续失败%lu次，暂停恢复%d秒",
               (unsigned long)consecutive_recovery_failures,
               CAN_RECOVERY_PAUSE_MS / 1000);
    }
  }
  return ret;
}

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
  uint32_t current_tick = xTaskGetTickCount();
  uint32_t start_ms = current_tick * portTICK_PERIOD_MS;

  // 🔧 检查是否在恢复暂停期间
  if (recovery_pause_until != 0 && current_tick < recovery_pause_until) {
    // 每5秒打印一次暂停状态
    static uint32_t last_pause_log = 0;
    if (current_tick - last_pause_log > pdMS_TO_TICKS(5000)) {
      last_pause_log = current_tick;
      uint32_t remaining_ms = (recovery_pause_until - current_tick) * portTICK_PERIOD_MS;
      ESP_LOGW(TAG, "⏸️ CAN恢复暂停中，剩余%lu秒", (unsigned long)(remaining_ms / 1000));
    }
    return ESP_ERR_TIMEOUT;
  }
  
  // 暂停期结束，重置
  if (recovery_pause_until != 0 && current_tick >= recovery_pause_until) {
    ESP_LOGI(TAG, "▶️ CAN恢复暂停期结束，恢复尝试恢复");
    recovery_pause_until = 0;
    consecutive_recovery_failures = 0;
  }

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
  } else if (status_info.state == TWAI_STATE_STOPPED) {
    need_recovery = true;
    reason = "STOPPED状态";
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

  bool skip_cooldown = (status_info.state == TWAI_STATE_STOPPED);
  if (!force_recovery && !skip_cooldown && last_recovery_time != 0 &&
      (current_time - last_recovery_time) < pdMS_TO_TICKS(min_interval_ms)) {
    // 距离上次尝试恢复时间太短，跳过本次恢复（静默返回，不打印日志）
    return ESP_ERR_NOT_FINISHED;  // 🔧 用不同的错误码区分"冷却中"和"真正超时"
  }

  // 记录恢复前的状态
  ESP_LOGW(TAG, "🔄 CAN总线触发恢复: 原因=%s | 状态=%d, TXERR=%lu, RXERR=%lu",
           reason ? reason : "强制恢复", (int)status_info.state,
           (unsigned long)status_info.tx_error_counter,
           (unsigned long)status_info.rx_error_counter);

  // 更新恢复时间戳
  last_recovery_time = current_time;

  // RECOVERING 状态下等待一小段时间
  // 🔧 优化：如果错误计数器已饱和(255)，直接硬复位，不等待
  if (status_info.state == TWAI_STATE_RECOVERING) {
    if (status_info.tx_error_counter >= 255 || status_info.rx_error_counter >= 255) {
      ESP_LOGW(TAG, "⚠️ 错误计数器饱和 (TX=%lu, RX=%lu)，直接硬复位",
               (unsigned long)status_info.tx_error_counter,
               (unsigned long)status_info.rx_error_counter);
      return can_hw_reset_and_reinit();
    }
    
    // 错误计数器未饱和，等待自动恢复
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_ms) <
           CAN_RECOVERY_BUDGET_MS) {
      vTaskDelay(pdMS_TO_TICKS(10));
      if (twai_get_status_info(&status_info) != ESP_OK) {
        break;
      }
      if (status_info.state != TWAI_STATE_RECOVERING) {
        break;
      }
    }
    if (status_info.state == TWAI_STATE_RECOVERING) {
      return can_hw_reset_and_reinit();
    }
  }

  // BUS-OFF 需要先发起恢复
  if (status_info.state == TWAI_STATE_BUS_OFF) {
    ESP_LOGI(TAG, "Initiating TWAI bus recovery...");
    twai_initiate_recovery();
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_ms) <
           CAN_RECOVERY_BUDGET_MS) {
      vTaskDelay(pdMS_TO_TICKS(10));
      if (twai_get_status_info(&status_info) != ESP_OK) {
        break;
      }
      if (status_info.state == TWAI_STATE_STOPPED ||
          status_info.state == TWAI_STATE_RUNNING) {
        break;
      }
    }
    if (status_info.state == TWAI_STATE_RECOVERING ||
        status_info.state == TWAI_STATE_BUS_OFF) {
      return can_hw_reset_and_reinit();
    }
  }

  if (status_info.state == TWAI_STATE_STOPPED) {
    ret = twai_start();
    if (ret != ESP_OK) {
      return can_hw_reset_and_reinit();
    }
    if (ret == ESP_OK) {
      can_recovery_count++;
      consecutive_tx_failures = 0;
      if (twai_get_status_info(&status_info) == ESP_OK) {
        ESP_LOGI(TAG, "✅ CAN总线已恢复 (次数:%lu, TXErr:%lu, RXErr:%lu)",
                 (unsigned long)can_recovery_count,
                 (unsigned long)status_info.tx_error_counter,
                 (unsigned long)status_info.rx_error_counter);
      }
    }
    return ret;
  }

  if (status_info.state == TWAI_STATE_RUNNING) {
    ret = twai_stop();
    if (ret != ESP_OK) {
      return can_hw_reset_and_reinit();
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    ret = twai_start();
    if (ret != ESP_OK) {
      return can_hw_reset_and_reinit();
    }

    if (ret == ESP_OK) {
      can_recovery_count++;
      consecutive_tx_failures = 0;
      if (twai_get_status_info(&status_info) == ESP_OK) {
        ESP_LOGI(TAG, "✅ CAN总线已恢复 (次数:%lu, TXErr:%lu, RXErr:%lu)",
                 (unsigned long)can_recovery_count,
                 (unsigned long)status_info.tx_error_counter,
                 (unsigned long)status_info.rx_error_counter);
      }
    }
    return ret;
  }

  return ESP_ERR_TIMEOUT;
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
#define CAN_STATUS_PRINT_INTERVAL_MS 1000 // 每1秒打印一次状态
#if ENABLE_CAN_DEBUG
#define CAN_TX_DEBUG_EVERY_N 5  // 临时改为5，用于调试左右电机命令
#define CAN_SKIP_LOG_INTERVAL_MS 500
static uint32_t can_tx_debug_count = 0;
static uint32_t last_can_skip_log_time = 0;
static twai_state_t last_can_state = TWAI_STATE_STOPPED;
#define CAN_ERROR_DELTA_LOG_INTERVAL_MS 300
static uint32_t last_error_delta_log_time = 0;
static uint32_t last_tx_err = 0;
static uint32_t last_rx_err = 0;
static uint32_t last_bus_err = 0;
static uint32_t last_arb_lost = 0;
static uint32_t last_tx_failed = 0;
static uint32_t last_rx_missed = 0;
static uint32_t can_counter_delta(uint32_t current, uint32_t last) {
  return (current >= last) ? (current - last) : current;
}
#endif

/**
 * 发送CAN数据
 */
static void keya_send_data(uint32_t id, uint8_t *data) {
  twai_message_t message;
  twai_status_info_t status_info;
  esp_err_t ret;

  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ret = twai_get_status_info(&status_info);
  bool status_ok = (ret == ESP_OK);
  if (!status_ok) {
    if (current_time - last_status_print_time > CAN_STATUS_PRINT_INTERVAL_MS) {
      last_status_print_time = current_time;
      ESP_LOGW(TAG, "⚠️ 无法获取CAN状态信息: %s", esp_err_to_name(ret));
    }
    memset(&status_info, 0, sizeof(status_info));
  }

  // 🔧 调试：定期打印CAN状态，并检查错误计数器
  if (status_ok &&
      current_time - last_status_print_time > CAN_STATUS_PRINT_INTERVAL_MS) {
    last_status_print_time = current_time;
    
    ESP_LOGI(TAG, "📊 CAN状态: State=%d, TXErr=%lu, RXErr=%lu, TXQ=%lu, RXQ=%lu, BusErr=%lu, ArbLost=%lu, TxFail=%lu, RxMiss=%lu | 发送统计: OK=%lu, TIMEOUT=%lu, ERR=%lu",
             (int)status_info.state,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter,
             (unsigned long)status_info.msgs_to_tx,
             (unsigned long)status_info.msgs_to_rx,
             (unsigned long)status_info.bus_error_count,
             (unsigned long)status_info.arb_lost_count,
             (unsigned long)status_info.tx_failed_count,
             (unsigned long)status_info.rx_missed_count,
             (unsigned long)can_tx_success_count,
             (unsigned long)can_tx_timeout_count,
             (unsigned long)can_tx_error_count);
  }

#if ENABLE_CAN_DEBUG
  if (status_ok && status_info.state != last_can_state) {
    ESP_LOGI(TAG, "🔁 CAN状态变化: %d -> %d (TXErr=%lu RXErr=%lu)",
             (int)last_can_state,
             (int)status_info.state,
             (unsigned long)status_info.tx_error_counter,
             (unsigned long)status_info.rx_error_counter);
    last_can_state = status_info.state;
  }

  if (status_ok) {
    bool counters_changed =
        status_info.tx_error_counter != last_tx_err ||
        status_info.rx_error_counter != last_rx_err ||
        status_info.bus_error_count != last_bus_err ||
        status_info.arb_lost_count != last_arb_lost ||
        status_info.tx_failed_count != last_tx_failed ||
        status_info.rx_missed_count != last_rx_missed;
    if (counters_changed &&
        current_time - last_error_delta_log_time > CAN_ERROR_DELTA_LOG_INTERVAL_MS) {
      last_error_delta_log_time = current_time;
      ESP_LOGW(TAG, "⚠️ CAN计数变化: TXErr+%lu RXErr+%lu BusErr+%lu ArbLost+%lu TxFail+%lu RxMiss+%lu",
               (unsigned long)can_counter_delta(status_info.tx_error_counter, last_tx_err),
               (unsigned long)can_counter_delta(status_info.rx_error_counter, last_rx_err),
               (unsigned long)can_counter_delta(status_info.bus_error_count, last_bus_err),
               (unsigned long)can_counter_delta(status_info.arb_lost_count, last_arb_lost),
               (unsigned long)can_counter_delta(status_info.tx_failed_count, last_tx_failed),
               (unsigned long)can_counter_delta(status_info.rx_missed_count, last_rx_missed));
    }
    last_tx_err = status_info.tx_error_counter;
    last_rx_err = status_info.rx_error_counter;
    last_bus_err = status_info.bus_error_count;
    last_arb_lost = status_info.arb_lost_count;
    last_tx_failed = status_info.tx_failed_count;
    last_rx_missed = status_info.rx_missed_count;
  }
#endif

  // 🔧 发送前检查CAN状态，非RUNNING状态下不发送，触发恢复
  if (status_ok &&
      (status_info.state != TWAI_STATE_RUNNING ||
       status_info.tx_error_counter > 127 ||
       status_info.rx_error_counter > 127)) {
    // 🔧 限制日志频率，每秒最多打印一次
    static uint32_t last_abnormal_log_time = 0;
    if (current_time - last_abnormal_log_time > 1000) {
      last_abnormal_log_time = current_time;
      ESP_LOGW(TAG, "⚠️ CAN异常状态检测: State=%d, TXErr=%lu, RXErr=%lu",
               (int)status_info.state,
               (unsigned long)status_info.tx_error_counter,
               (unsigned long)status_info.rx_error_counter);
    }

    esp_err_t recovery_ret = can_bus_recovery_ex(true);
    if (recovery_ret != ESP_OK) {
      // 🔧 区分冷却中（静默）和真正的失败（每秒最多打印一次）
      if (recovery_ret != ESP_ERR_NOT_FINISHED) {
        static uint32_t last_recovery_fail_log = 0;
        if (current_time - last_recovery_fail_log > 1000) {
          last_recovery_fail_log = current_time;
          ESP_LOGW(TAG, "CAN恢复失败: %s", esp_err_to_name(recovery_ret));
        }
      }
      // 冷却期间或恢复失败都跳过本次发送
      return;
    }

    if (twai_get_status_info(&status_info) != ESP_OK ||
        status_info.state != TWAI_STATE_RUNNING) {
      ESP_LOGW(TAG, "CAN未恢复到RUNNING状态，跳过发送");
      return;
    }
  } else if (!status_ok) {
    return;
  }

  message.extd = 1;
  message.identifier = id;
  message.data_length_code = 8;
  message.rtr = 0;

  for (int i = 0; i < 8; i++) {
    message.data[i] = data[i];
  }

  // 🔧 调试：检查TX队列是否满
  if (status_ok && status_info.msgs_to_tx >= 18) {  // 队列长度20，接近满时警告
    ESP_LOGW(TAG, "⚠️ CAN TX队列接近满: %lu/20", (unsigned long)status_info.msgs_to_tx);
  }

  esp_err_t result = twai_transmit(&message, 0);

  if (result == ESP_OK) {
    can_tx_success_count++;
    if (consecutive_tx_failures > 0) {
      ESP_LOGI(TAG, "✅ CAN发送恢复正常 (之前失败%lu次)", (unsigned long)consecutive_tx_failures);
      consecutive_tx_failures = 0;
    }
#if ENABLE_CAN_DEBUG
    can_tx_debug_count++;
    if (can_tx_debug_count % CAN_TX_DEBUG_EVERY_N == 0) {
      ESP_LOGI(TAG, "📤 CAN TX OK #%lu: ID=0x%08lX, DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
               (unsigned long)can_tx_success_count,
               (unsigned long)id,
               message.data[0], message.data[1], message.data[2], message.data[3],
               message.data[4], message.data[5], message.data[6], message.data[7]);
    }
#endif
  } else {
    consecutive_tx_failures++;

    // 连续失败时，只在非 RECOVERING 状态下尝试恢复
    if (consecutive_tx_failures >= CAN_FORCE_RECOVERY_THRESHOLD) {
      if (status_ok && status_info.state != TWAI_STATE_RECOVERING) {
        ESP_LOGW(TAG, "⚠️ CAN连续发送失败 %lu 次，触发恢复",
                 (unsigned long)consecutive_tx_failures);
        can_bus_recovery_ex(true);
      }
      // 不重试发送，让下次循环处理
    }

    if (result == ESP_ERR_TIMEOUT) {
      can_tx_timeout_count++;
      // 🔧 调试：每10次TIMEOUT打印一次
      if (can_tx_timeout_count % 10 == 1) {
        if (status_ok) {
          ESP_LOGW(TAG, "⏱️ CAN发送TIMEOUT (累计%lu次), ID=0x%08lX, TXQ=%lu, DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned long)can_tx_timeout_count,
                   (unsigned long)id,
                   (unsigned long)status_info.msgs_to_tx,
                   message.data[0], message.data[1], message.data[2], message.data[3],
                   message.data[4], message.data[5], message.data[6], message.data[7]);
        } else {
          ESP_LOGW(TAG, "⏱️ CAN发送TIMEOUT (累计%lu次), ID=0x%08lX, DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned long)can_tx_timeout_count,
                   (unsigned long)id,
                   message.data[0], message.data[1], message.data[2], message.data[3],
                   message.data[4], message.data[5], message.data[6], message.data[7]);
        }
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
      // INVALID_STATE 理论上在前面的状态检查中已被拦截
      // 这里只做日志记录（不频繁打印，每100次打印一次）
      if (can_tx_error_count % 100 == 1) {
        ESP_LOGW(TAG, "⚠️ CAN INVALID_STATE (累计%lu次)，State=%d",
                 (unsigned long)can_tx_error_count,
                 status_ok ? (int)status_info.state : -1);
      }
      return;
    }

    can_tx_error_count++;
    ESP_LOGW(TAG, "❌ CAN发送失败: %s, ID=0x%08lX, DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
             esp_err_to_name(result),
             (unsigned long)id,
             message.data[0], message.data[1], message.data[2], message.data[3],
             message.data[4], message.data[5], message.data[6], message.data[7]);
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
 * 注意：心跳帧是广播帧，没有接收方会发送ACK
 * 在 NO_ACK 模式下不应该累积错误，但为安全起见仍做检查
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

void drv_keyadouble_send_heartbeat(int8_t speed_left, int8_t speed_right) {
  send_controller_heartbeat(speed_left, speed_right);
}

/**
 * 初始化电机驱动
 */
esp_err_t drv_keyadouble_init(void) {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_16, GPIO_NUM_17, CAN_MODE);

  g_config.tx_queue_len = 20;
  g_config.rx_queue_len = 50;

  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
  twai_driver_installed = true;  // 🔧 标记驱动已安装
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

  const char *mode_str =
#if CAN_MODE == TWAI_MODE_NO_ACK
      "No-ACK Mode";
#else
      "Normal Mode";
#endif
  ESP_LOGI(TAG, "Motor driver initialized (%s, Priority 8 RX Task)", mode_str);
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

// 🔧 电机使能状态跟踪（用于减少CAN消息数量）
static bool motor_a_enabled = false;
static bool motor_b_enabled = false;
static int8_t last_speed_left = 0;
static int8_t last_speed_right = 0;
static uint32_t last_enable_time = 0;
#define ENABLE_RESEND_INTERVAL_MS 5000  // 每5秒重发一次使能命令（保活）

/**
 * 设置左右电机速度实现运动
 * 🔧 优化：只在首次/状态变化/定时保活时发送使能命令，减少CAN流量
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right) {
  if ((abs(speed_left) > 100) || (abs(speed_right) > 100))
    return 1;

  bk_flag_left = (speed_left != 0) ? 1 : 0;
  bk_flag_right = (speed_right != 0) ? 1 : 0;

  // 🔧 仅记录非RUNNING状态，恢复交给发送逻辑处理
  twai_status_info_t status_info;
  if (twai_get_status_info(&status_info) == ESP_OK) {
    if (status_info.state != TWAI_STATE_RUNNING) {
      static uint32_t last_non_running_warn = 0;
      uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (now - last_non_running_warn > 1000) {
        ESP_LOGW(TAG, "⚠️ CAN状态异常: State=%d", (int)status_info.state);
        last_non_running_warn = now;
      }
      // CAN异常时重置使能状态，下次恢复后需要重新使能
      motor_a_enabled = false;
      motor_b_enabled = false;
    }
  }

  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  // 🔧 检查是否需要发送使能命令
  bool need_enable_a = false;
  bool need_enable_b = false;
  
  // 条件1：首次使能（电机从停止到运动）
  if (speed_left != 0 && !motor_a_enabled) {
    need_enable_a = true;
  }
  if (speed_right != 0 && !motor_b_enabled) {
    need_enable_b = true;
  }
  
  // 条件2：定时保活（每5秒重发使能命令，防止驱动器看门狗超时）
  if (current_time - last_enable_time > ENABLE_RESEND_INTERVAL_MS) {
    if (speed_left != 0 || speed_right != 0) {
      need_enable_a = true;
      need_enable_b = true;
    }
    last_enable_time = current_time;
  }

  // 🔍 调试：只在速度变化时打印（减少日志）
  if (speed_left != last_speed_left || speed_right != last_speed_right) {
    ESP_LOGI(TAG, "🚗 电机命令: Left=%d Right=%d", speed_left, speed_right);
    last_speed_left = speed_left;
    last_speed_right = speed_right;
  }

  // 发送心跳（每次都发）
  send_controller_heartbeat(speed_left, speed_right);

  // 🔧 条件发送使能命令（减少CAN流量）
  if (need_enable_a) {
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0);
    motor_a_enabled = true;
    ESP_LOGD(TAG, "📤 发送A路使能命令");
  }
  if (need_enable_b) {
    motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0);
    motor_b_enabled = true;
    ESP_LOGD(TAG, "📤 发送B路使能命令");
  }

  // 发送速度命令（每次都发）
  motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left);
  motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right);

  // 更新使能状态（速度为0时标记为未使能，下次非零时重新使能）
  if (speed_left == 0) {
    motor_a_enabled = false;
  }
  if (speed_right == 0) {
    motor_b_enabled = false;
  }

  return 0;
}
