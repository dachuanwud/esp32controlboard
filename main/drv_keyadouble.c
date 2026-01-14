#include "drv_keyadouble.h"
#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_private/periph_ctrl.h"  // 用于外设复位
#include "esp_task_wdt.h"             // 🐕 任务看门狗

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

// CAN task handle (TX/RX/recovery in one task)
static TaskHandle_t can_task_handle = NULL;

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

// 🔧 新增：硬复位保护计数器（防止频繁硬复位导致系统不稳定）
static uint32_t hw_reset_count = 0;          // 硬复位计数
static uint32_t last_hw_reset_time = 0;      // 上次硬复位时间
#define CAN_HW_RESET_MAX_COUNT 3             // 短时间内最多允许3次硬复位
#define CAN_HW_RESET_WINDOW_MS 60000         // 计数窗口60秒
#define CAN_HW_RESET_COOLDOWN_MS 120000      // 硬复位过多后冷却2分钟

// 注意：已移除motor_enabled标志，改为每次发送速度命令时都发送使能命令
// 这样可以避免看门狗超时导致的驱动器失能问题

// TWAI (CAN) 配置 - 根据电路图SN65HVD232D CAN收发电路
// IO16连接到SN65HVD232D的D引脚(TX)，IO17连接到R引脚(RX)
// 使用NO_ACK模式，不等待ACK应答，避免错误计数器累积
// 注意：配置结构体在初始化函数中创建，避免静态初始化问题
#define CAN_MODE TWAI_MODE_NO_ACK  // 改为NO_ACK模式
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();

// ============================================================================
// CAN过滤器配置 - 多主控制器架构优化
// ============================================================================
// 场景：ESP32与自动导航模块共用CAN总线，都向电机驱动器发送指令
// 问题：两个控制器发送相同ID(0x0600001)会导致TX/RX错误累积
// 方案：配置过滤器只接收电机驱动器的反馈消息，忽略其他控制器的消息
//
// 电机驱动器反馈ID: 0x05800001 (驱动器->控制器)
// 心跳包ID:        0x07000001 (驱动器->控制器)
// 控制指令ID:      0x06000001 (控制器->驱动器) - 不需要接收
//
// 过滤器设计：使用双过滤器模式，分别匹配0x058xxxxx和0x070xxxxx
// 注意：过滤器只减少RX队列压力，TX错误（发送冲突）无法通过过滤器解决
// ============================================================================
static const twai_filter_config_t f_config = {
    .acceptance_code = (0x05800000 << 3),  // 29位扩展帧ID左移3位
    .acceptance_mask = (0x02FFFFFF << 3),  // 掩码：允许0x05xxxxxx和0x07xxxxxx
    .single_filter = true
};

// Software TX queue and CAN task config
#define CAN_TX_QUEUE_LEN 20  // 🔧 减少队列长度，避免旧命令堆积
#define CAN_TX_BURST_MAX 10  // 🔧 增加单次发送数量，加快队列清空
#define CAN_RX_BURST_MAX 10
#define CAN_TASK_STACK_SIZE 4096
#define CAN_TASK_PRIORITY 8
#define CAN_INIT_MAX_RETRIES 3
#define CAN_INIT_RETRY_DELAY_MS 200
#define CAN_INIT_RESET_DELAY_MS 50

// 🔧 最新速度命令（覆盖式存储，只保留最新值）
static volatile int8_t latest_speed_left = 0;
static volatile int8_t latest_speed_right = 0;
static volatile bool speed_cmd_pending = false;  // 标记有新的速度命令待发送

// ============================================================================
// 🔧 CAN恢复优化配置 - 防止假死
// ============================================================================
#define CAN_RECOVERY_BUDGET_MS 100          // 🔧 优化：单次恢复等待时间从300ms减少到100ms
#define CAN_RECOVERY_TOTAL_TIMEOUT_MS 500   // 🆕 总恢复超时限制500ms，防止长时间阻塞
#define CAN_RECOVERY_POLL_INTERVAL_MS 10    // 恢复轮询间隔10ms
#define CAN_MAX_RECOVERY_ITERATIONS 5       // 🆕 单次恢复最大迭代次数

// ============================================================================
// 🔧 多主控制器架构 - 错误阈值配置
// ============================================================================
// 场景：ESP32与自动导航模块共用CAN总线，发送相同ID时会产生仲裁冲突
// 冲突会导致TX/RX错误计数器累积，但这是多主架构的正常现象
// 提高阈值可以避免频繁触发恢复，让系统更稳定
//
// CAN错误计数器含义：
//   0-95:    Error Active  - 正常工作
//   96-127:  Warning       - 错误增多但仍可工作
//   128-255: Error Passive - 限制发送能力
//   256+:    BUS-OFF       - 停止通信
//
// 默认阈值127（进入Error Passive时触发恢复）
// 多主架构建议200（允许更多冲突，只有接近BUS-OFF时才恢复）
// ============================================================================
#define CAN_ERROR_THRESHOLD 200             // 🔧 错误计数器阈值（多主架构提高到200）

// 🔧 标记驱动是否已安装（用于跟踪状态）
static bool twai_driver_installed = false;

typedef struct {
  twai_message_t message;
} can_tx_item_t;

static QueueHandle_t can_tx_queue = NULL;
static twai_status_info_t can_last_status_info;
static bool can_last_status_valid = false;
static uint32_t can_last_status_time = 0;
static volatile twai_state_t can_last_state = TWAI_STATE_STOPPED;
static uint32_t can_tx_queue_drop_count = 0;

static void can_update_status_cache(const twai_status_info_t *status_info, uint32_t now_ms);
static void can_send_message(const twai_message_t *message);
static void can_task(void *pvParameters);

static esp_err_t can_hw_reset_and_reinit(void) {
  esp_err_t ret;
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  // ====================================================================
  // 🛡️ 硬复位频率保护 - 防止频繁硬复位导致系统不稳定或重启失败
  // ====================================================================
  
  // 检查是否在冷却期（硬复位过多后的强制等待）
  static uint32_t hw_reset_cooldown_until = 0;
  if (hw_reset_cooldown_until != 0 && current_time < hw_reset_cooldown_until) {
    ESP_LOGW(TAG, "⏸️ CAN硬复位冷却中，跳过硬复位 (剩余%lus)", 
             (unsigned long)((hw_reset_cooldown_until - current_time) / 1000));
    return ESP_ERR_NOT_ALLOWED;
  }
  
  // 重置冷却期
  if (hw_reset_cooldown_until != 0 && current_time >= hw_reset_cooldown_until) {
    ESP_LOGI(TAG, "▶️ CAN硬复位冷却期结束");
    hw_reset_cooldown_until = 0;
    hw_reset_count = 0;
  }
  
  // 检查短时间内硬复位次数
  if (last_hw_reset_time != 0 && (current_time - last_hw_reset_time) < CAN_HW_RESET_WINDOW_MS) {
    hw_reset_count++;
    if (hw_reset_count > CAN_HW_RESET_MAX_COUNT) {
      hw_reset_cooldown_until = current_time + CAN_HW_RESET_COOLDOWN_MS;
      ESP_LOGE(TAG, "🛑 CAN硬复位过于频繁 (%lu次/%lus内)，进入冷却期%ds",
               (unsigned long)hw_reset_count,
               (unsigned long)(CAN_HW_RESET_WINDOW_MS / 1000),
               CAN_HW_RESET_COOLDOWN_MS / 1000);
      ESP_LOGE(TAG, "⚠️ 请检查: 1.CAN总线连接 2.电源供电 3.终端电阻");
      return ESP_ERR_NOT_ALLOWED;
    }
  } else {
    // 超出时间窗口，重新计数
    hw_reset_count = 1;
  }
  last_hw_reset_time = current_time;
  
  ESP_LOGW(TAG, "🧯 硬复位TWAI外设并重装驱动 (本窗口第%lu次)", (unsigned long)hw_reset_count);
  
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
  
  // 🔧 如果驱动仍然安装着（卸载失败），使用激进方法（但增加延时保护）
  if (twai_driver_installed) {
    ESP_LOGW(TAG, "⚠️ 正常卸载失败，尝试强制复位...");
    
    // 🛡️ 增加延时，减少对电源的冲击
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 强制禁用外设时钟，这会使驱动状态无效
    periph_module_disable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));  // 🔧 增加延时从50ms到100ms
    
    // 复位外设
    periph_module_reset(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));  // 🔧 增加延时
    
    // 重新启用
    periph_module_enable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(100));  // 🔧 增加延时
    
    // 🔧 关键：此时驱动内部状态已损坏，需要标记为未安装
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
    vTaskDelay(pdMS_TO_TICKS(50));
    (void)twai_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 🛡️ 完全复位外设（增加延时保护）
    vTaskDelay(pdMS_TO_TICKS(100));
    periph_module_disable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(150));  // 🔧 增加延时
    periph_module_reset(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(150));
    periph_module_enable(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(150));
    
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
    ESP_LOGI(TAG, "✅ TWAI硬复位恢复成功 (总次数:%lu)", (unsigned long)can_recovery_count);
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
 * CAN总线恢复函数（优化版）
 * 当错误计数器过高或处于BUS-OFF状态时，停止并重启CAN驱动
 * 🆕 优化：添加总超时限制和迭代次数限制，防止长时间阻塞导致假死
 * @param force_recovery 是否强制恢复（跳过时间间隔限制）
 * @return
 * ESP_OK=恢复成功/不需要恢复，ESP_ERR_TIMEOUT=需要恢复但被时间限制跳过，其他=恢复失败
 */
static esp_err_t can_bus_recovery_ex(bool force_recovery) {
  twai_status_info_t status_info;
  esp_err_t ret;
  uint32_t current_tick = xTaskGetTickCount();
  uint32_t start_ms = current_tick * portTICK_PERIOD_MS;
  uint32_t iteration_count = 0;  // 🆕 迭代计数器，防止无限循环

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
  } else if (status_info.tx_error_counter > CAN_ERROR_THRESHOLD) {
    need_recovery = true;
    reason = "TX错误计数器过高";
  } else if (status_info.rx_error_counter > CAN_ERROR_THRESHOLD) {
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
    
    // 🆕 优化：使用迭代次数限制代替纯时间循环，防止长时间阻塞
    iteration_count = 0;
    while (iteration_count < CAN_MAX_RECOVERY_ITERATIONS) {
      // 🆕 检查总超时
      uint32_t elapsed_ms = xTaskGetTickCount() * portTICK_PERIOD_MS - start_ms;
      if (elapsed_ms >= CAN_RECOVERY_TOTAL_TIMEOUT_MS) {
        ESP_LOGW(TAG, "⏱️ CAN恢复总超时(%lums)，直接硬复位", (unsigned long)elapsed_ms);
        return can_hw_reset_and_reinit();
      }
      
      vTaskDelay(pdMS_TO_TICKS(CAN_RECOVERY_POLL_INTERVAL_MS));
      iteration_count++;
      
      if (twai_get_status_info(&status_info) != ESP_OK) {
        break;
      }
      if (status_info.state != TWAI_STATE_RECOVERING) {
        break;
      }
    }
    if (status_info.state == TWAI_STATE_RECOVERING) {
      ESP_LOGW(TAG, "⏱️ RECOVERING状态等待超时(迭代%lu次)，硬复位", (unsigned long)iteration_count);
      return can_hw_reset_and_reinit();
    }
  }

  // BUS-OFF 需要先发起恢复
  if (status_info.state == TWAI_STATE_BUS_OFF) {
    // 🆕 检查总超时
    uint32_t elapsed_ms = xTaskGetTickCount() * portTICK_PERIOD_MS - start_ms;
    if (elapsed_ms >= CAN_RECOVERY_TOTAL_TIMEOUT_MS) {
      ESP_LOGW(TAG, "⏱️ BUS-OFF恢复前已超时(%lums)，直接硬复位", (unsigned long)elapsed_ms);
      return can_hw_reset_and_reinit();
    }
    
    ESP_LOGI(TAG, "Initiating TWAI bus recovery...");
    twai_initiate_recovery();
    
    // 🆕 优化：使用迭代次数限制
    iteration_count = 0;
    while (iteration_count < CAN_MAX_RECOVERY_ITERATIONS) {
      // 🆕 检查总超时
      elapsed_ms = xTaskGetTickCount() * portTICK_PERIOD_MS - start_ms;
      if (elapsed_ms >= CAN_RECOVERY_TOTAL_TIMEOUT_MS) {
        ESP_LOGW(TAG, "⏱️ BUS-OFF恢复超时(%lums)，直接硬复位", (unsigned long)elapsed_ms);
        return can_hw_reset_and_reinit();
      }
      
      vTaskDelay(pdMS_TO_TICKS(CAN_RECOVERY_POLL_INTERVAL_MS));
      iteration_count++;
      
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
      ESP_LOGW(TAG, "⏱️ BUS-OFF恢复未完成(迭代%lu次)，硬复位", (unsigned long)iteration_count);
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
      consecutive_recovery_failures = 0;  // 🆕 恢复成功，重置失败计数
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

    vTaskDelay(pdMS_TO_TICKS(CAN_RECOVERY_POLL_INTERVAL_MS));  // 🔧 使用配置的间隔

    ret = twai_start();
    if (ret != ESP_OK) {
      return can_hw_reset_and_reinit();
    }

    if (ret == ESP_OK) {
      can_recovery_count++;
      consecutive_tx_failures = 0;
      consecutive_recovery_failures = 0;  // 🆕 恢复成功，重置失败计数
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

// 🔧 调试：CAN发送统计
static uint32_t can_tx_success_count = 0;
static uint32_t can_tx_timeout_count = 0;
static uint32_t can_tx_error_count = 0;
static uint32_t last_status_print_time = 0;
#define CAN_STATUS_PRINT_INTERVAL_MS 5000 // 每5秒打印一次状态
#if ENABLE_CAN_DEBUG
#define CAN_TX_DEBUG_EVERY_N 100  // 每100次发送打印一次
#define CAN_SKIP_LOG_INTERVAL_MS 500
static uint32_t can_tx_debug_count = 0;
static uint32_t last_can_skip_log_time = 0;
static twai_state_t last_can_state = TWAI_STATE_STOPPED;
#define CAN_ERROR_DELTA_LOG_INTERVAL_MS 5000  // 每5秒打印一次错误计数变化
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

static void can_update_status_cache(const twai_status_info_t *status_info,
                                    uint32_t now_ms) {
  if (status_info == NULL) {
    return;
  }
  can_last_status_info = *status_info;
  can_last_status_valid = true;
  can_last_status_time = now_ms;
  can_last_state = status_info->state;
}

/**
 * Send CAN frame (runs in CAN task).
 */
static void can_send_message(const twai_message_t *message) {
  twai_status_info_t status_info;
  esp_err_t ret;

  if (message == NULL) {
    return;
  }

  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ret = twai_get_status_info(&status_info);
  bool status_ok = (ret == ESP_OK);
  if (!status_ok) {
    if (current_time - last_status_print_time > CAN_STATUS_PRINT_INTERVAL_MS) {
      last_status_print_time = current_time;
      ESP_LOGW(TAG, "⚠️ 无法获取CAN状态信息: %s", esp_err_to_name(ret));
    }
    memset(&status_info, 0, sizeof(status_info));
  } else {
    can_update_status_cache(&status_info, current_time);
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
       status_info.tx_error_counter > CAN_ERROR_THRESHOLD ||
       status_info.rx_error_counter > CAN_ERROR_THRESHOLD)) {
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
    can_update_status_cache(&status_info, current_time);
  } else if (!status_ok) {
    return;
  }

  twai_message_t tx_message = *message;

  // 🔧 调试：检查TX队列是否满
  if (status_ok && status_info.msgs_to_tx >= 18) {  // 队列长度20，接近满时警告
    ESP_LOGW(TAG, "⚠️ CAN TX队列接近满: %lu/20", (unsigned long)status_info.msgs_to_tx);
  }

  esp_err_t result = twai_transmit(&tx_message, 0);

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
               (unsigned long)tx_message.identifier,
               tx_message.data[0], tx_message.data[1], tx_message.data[2], tx_message.data[3],
               tx_message.data[4], tx_message.data[5], tx_message.data[6], tx_message.data[7]);
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
                   (unsigned long)tx_message.identifier,
                   (unsigned long)status_info.msgs_to_tx,
                   tx_message.data[0], tx_message.data[1], tx_message.data[2], tx_message.data[3],
                   tx_message.data[4], tx_message.data[5], tx_message.data[6], tx_message.data[7]);
        } else {
          ESP_LOGW(TAG, "⏱️ CAN发送TIMEOUT (累计%lu次), ID=0x%08lX, DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned long)can_tx_timeout_count,
                   (unsigned long)tx_message.identifier,
                   tx_message.data[0], tx_message.data[1], tx_message.data[2], tx_message.data[3],
                   tx_message.data[4], tx_message.data[5], tx_message.data[6], tx_message.data[7]);
        }
      }
      bool is_speed_cmd =
          (tx_message.data[0] == 0x23 && tx_message.data[1] == 0x00 &&
           tx_message.data[2] == 0x20);
      if (is_speed_cmd) {
        twai_transmit(&tx_message, 0);
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
             (unsigned long)tx_message.identifier,
             tx_message.data[0], tx_message.data[1], tx_message.data[2], tx_message.data[3],
             tx_message.data[4], tx_message.data[5], tx_message.data[6], tx_message.data[7]);
  }
}

/**
 * CAN任务
 * 处理CAN发送和接收
 * 🐕 已添加任务看门狗监控
 */
static void can_task(void *pvParameters) {
  twai_message_t rx_message;
  can_tx_item_t tx_item;
  uint32_t rx_count = 0;
  uint32_t batch_count = 0;
  uint32_t consecutive_empty_loops = 0;
  uint32_t wdt_feed_counter = 0;  // 🐕 喂狗计数器

  (void)pvParameters;
  ESP_LOGI(TAG, "CAN task started");

  // 🐕 订阅任务看门狗监控
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret == ESP_OK) {
    ESP_LOGI(TAG, "🐕 CAN任务已加入看门狗监控");
  } else {
    ESP_LOGW(TAG, "⚠️ CAN任务加入看门狗失败: %s", esp_err_to_name(wdt_ret));
  }

  twai_status_info_t init_status;
  uint32_t init_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  if (twai_get_status_info(&init_status) == ESP_OK) {
    can_update_status_cache(&init_status, init_time);
  }

  while (1) {
    // 🐕 定期喂狗 - 每500次循环喂狗一次（约5秒，因为每次循环2-10ms）
    wdt_feed_counter++;
    if (wdt_feed_counter >= 500) {
      esp_task_wdt_reset();
      wdt_feed_counter = 0;
    }

    bool did_work = false;
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (!can_last_status_valid ||
        (now_ms - can_last_status_time) > CAN_STATUS_PRINT_INTERVAL_MS) {
      twai_status_info_t status_info;
      if (twai_get_status_info(&status_info) == ESP_OK) {
        can_update_status_cache(&status_info, now_ms);
      }
    }

    for (int i = 0; i < CAN_TX_BURST_MAX; i++) {
      if (xQueueReceive(can_tx_queue, &tx_item, 0) != pdTRUE) {
        break;
      }
      did_work = true;
      can_send_message(&tx_item.message);
    }

    // 🔧 发送最新速度命令（覆盖式，只发最新值）
    // 这样即使上层调用频繁，CAN总线也只发送最新的速度命令
    if (speed_cmd_pending) {
      speed_cmd_pending = false;
      
      // 读取最新速度值
      int8_t sp_left = latest_speed_left;
      int8_t sp_right = latest_speed_right;
      
      // 发送心跳（包含速度信息）
      uint8_t hb_data[8] = {0};
      hb_data[0] = CONTROLLER_ID;
      hb_data[1] = HEARTBEAT_STATUS_ACTIVE;
      hb_data[2] = (heartbeat_seq >> 8) & 0xFF;
      hb_data[3] = heartbeat_seq & 0xFF;
      heartbeat_seq++;
      int16_t sp_a_hb = (int16_t)sp_left * 100;
      hb_data[4] = (sp_a_hb >> 8) & 0xFF;
      hb_data[5] = sp_a_hb & 0xFF;
      int16_t sp_b_hb = (int16_t)sp_right * 100;
      hb_data[6] = (sp_b_hb >> 8) & 0xFF;
      hb_data[7] = sp_b_hb & 0xFF;
      
      twai_message_t hb_msg = {
        .extd = 1,
        .identifier = CONTROLLER_HEARTBEAT_ID,
        .data_length_code = 8,
        .rtr = 0
      };
      memcpy(hb_msg.data, hb_data, 8);
      can_send_message(&hb_msg);
      
      // 发送左电机速度命令
      uint8_t speed_data_a[8] = {0x23, 0x00, 0x20, MOTOR_CHANNEL_A, 0, 0, 0, 0};
      int16_t sp_a = (int16_t)sp_left * 100;
      speed_data_a[4] = (sp_a >> 8) & 0xFF;
      speed_data_a[5] = sp_a & 0xFF;
      
      twai_message_t speed_msg_a = {
        .extd = 1,
        .identifier = DRIVER_TX_ID + DRIVER_ADDRESS,
        .data_length_code = 8,
        .rtr = 0
      };
      memcpy(speed_msg_a.data, speed_data_a, 8);
      can_send_message(&speed_msg_a);
      
      // 发送右电机速度命令
      uint8_t speed_data_b[8] = {0x23, 0x00, 0x20, MOTOR_CHANNEL_B, 0, 0, 0, 0};
      int16_t sp_b = (int16_t)sp_right * 100;
      speed_data_b[4] = (sp_b >> 8) & 0xFF;
      speed_data_b[5] = sp_b & 0xFF;
      
      twai_message_t speed_msg_b = {
        .extd = 1,
        .identifier = DRIVER_TX_ID + DRIVER_ADDRESS,
        .data_length_code = 8,
        .rtr = 0
      };
      memcpy(speed_msg_b.data, speed_data_b, 8);
      can_send_message(&speed_msg_b);
      
      did_work = true;
    }

    batch_count = 0;
    while (batch_count < CAN_RX_BURST_MAX) {
      esp_err_t ret = twai_receive(&rx_message, 0);
      if (ret == ESP_OK) {
        rx_count++;
        batch_count++;
        did_work = true;
        ESP_LOGD(TAG, "CAN RX #%lu: ID=0x%08" PRIX32 "...",
                 (unsigned long)rx_count, rx_message.identifier);
      } else if (ret == ESP_ERR_TIMEOUT) {
        break;
      } else {
        ESP_LOGD(TAG, "CAN RX error: %s", esp_err_to_name(ret));
        break;
      }
    }

    if (batch_count > 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      consecutive_empty_loops = 0;
    } else if (!did_work) {
      consecutive_empty_loops++;
      if (consecutive_empty_loops > 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
      } else {
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    } else {
      consecutive_empty_loops = 0;
    }
  }
}

/**
 * Enqueue CAN frame for CAN task.
 */
static void keya_send_data(uint32_t id, uint8_t *data) {
  static uint32_t last_queue_drop_log_time = 0;
  uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

  if (can_tx_queue == NULL) {
    if (now_ms - last_queue_drop_log_time > 1000) {
      last_queue_drop_log_time = now_ms;
      ESP_LOGW(TAG, "CAN TX queue not ready, drop msg");
    }
    return;
  }

  can_tx_item_t item;
  memset(&item, 0, sizeof(item));
  item.message.extd = 1;
  item.message.identifier = id;
  item.message.data_length_code = 8;
  item.message.rtr = 0;
  for (int i = 0; i < 8; i++) {
    item.message.data[i] = data[i];
  }

  if (xQueueSend(can_tx_queue, &item, 0) != pdTRUE) {
    can_tx_queue_drop_count++;
    if (now_ms - last_queue_drop_log_time > 1000) {
      last_queue_drop_log_time = now_ms;
      ESP_LOGW(TAG, "CAN TX queue full, drop msg (drop=%lu)",
               (unsigned long)can_tx_queue_drop_count);
    }
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

  esp_err_t ret = ESP_OK;
  for (int attempt = 1; attempt <= CAN_INIT_MAX_RETRIES; attempt++) {
    if (twai_driver_installed) {
      twai_stop();
      twai_driver_uninstall();
      twai_driver_installed = false;
    }

    periph_module_reset(PERIPH_TWAI_MODULE);
    vTaskDelay(pdMS_TO_TICKS(CAN_INIT_RESET_DELAY_MS));

    ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "CAN install failed (%d/%d): %s",
               attempt, CAN_INIT_MAX_RETRIES, esp_err_to_name(ret));
      if (attempt < CAN_INIT_MAX_RETRIES) {
        vTaskDelay(pdMS_TO_TICKS(CAN_INIT_RETRY_DELAY_MS));
      }
      continue;
    }

    twai_driver_installed = true;
    ret = twai_start();
    if (ret == ESP_OK) {
      break;
    }

    ESP_LOGW(TAG, "CAN start failed (%d/%d): %s",
             attempt, CAN_INIT_MAX_RETRIES, esp_err_to_name(ret));
    twai_driver_uninstall();
    twai_driver_installed = false;

    if (attempt < CAN_INIT_MAX_RETRIES) {
      vTaskDelay(pdMS_TO_TICKS(CAN_INIT_RETRY_DELAY_MS));
    }
  }

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "CAN init failed after %d attempts: %s",
             CAN_INIT_MAX_RETRIES, esp_err_to_name(ret));
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  can_tx_queue = xQueueCreate(CAN_TX_QUEUE_LEN, sizeof(can_tx_item_t));
  if (can_tx_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create CAN TX queue");
    twai_stop();
    twai_driver_uninstall();
    twai_driver_installed = false;
    return ESP_ERR_NO_MEM;
  }

  if (xTaskCreate(can_task, "can_task", CAN_TASK_STACK_SIZE, NULL,
                  CAN_TASK_PRIORITY, &can_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create CAN task");
    vQueueDelete(can_tx_queue);
    can_tx_queue = NULL;
    twai_stop();
    twai_driver_uninstall();
    twai_driver_installed = false;
    return ESP_ERR_NO_MEM;
  }

  can_recovery_count = 0;

  // 初始化统计计数器
  can_tx_success_count = 0;
  can_tx_timeout_count = 0;
  can_tx_error_count = 0;
  last_status_print_time = 0;
  can_tx_queue_drop_count = 0;
  can_last_status_valid = false;
  can_last_status_time = 0;
  can_last_state = TWAI_STATE_STOPPED;

  const char *mode_str =
#if CAN_MODE == TWAI_MODE_NO_ACK
      "No-ACK Mode";
#else
      "Normal Mode";
#endif
  ESP_LOGI(TAG, "Motor driver initialized (%s, CAN task prio %d)",
           mode_str, CAN_TASK_PRIORITY);
  ESP_LOGI(TAG, "CAN config: TX_Q=%d, RX_Q=%d, SW_TX_Q=%d, 250kbps, GPIO16/17",
           g_config.tx_queue_len, g_config.rx_queue_len, CAN_TX_QUEUE_LEN);
  return ESP_OK;
}

/**
 * 打印CAN诊断信息（可从外部调用）
 */
void drv_keyadouble_print_diag(void) {
  if (!can_last_status_valid) {
    ESP_LOGW(TAG, "CAN status not ready");
    return;
  }

  twai_status_info_t status_info = can_last_status_info;
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
  ESP_LOGI(TAG, "TX错误计数: %lu (>%d触发恢复, >255=BUS_OFF)",
           (unsigned long)status_info.tx_error_counter, CAN_ERROR_THRESHOLD);
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
  ESP_LOGI(TAG, "TX queue drops: %lu", (unsigned long)can_tx_queue_drop_count);
  ESP_LOGI(TAG, "恢复次数: %lu", (unsigned long)can_recovery_count);
  ESP_LOGI(TAG, "═══════════════════════════════════════════");
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
  if (can_last_status_valid && can_last_state != TWAI_STATE_RUNNING) {
    static uint32_t last_non_running_warn = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_non_running_warn > 1000) {
      ESP_LOGW(TAG, "⚠️ CAN状态异常: State=%d", (int)can_last_state);
      last_non_running_warn = now;
    }
    // CAN异常时重置使能状态，下次恢复后需要重新使能
    motor_a_enabled = false;
    motor_b_enabled = false;
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

  // 🔧 只更新最新速度值，不直接发送到队列
  // CAN task 会周期性读取并发送最新值，避免队列堆积旧命令
  latest_speed_left = speed_left;
  latest_speed_right = speed_right;
  speed_cmd_pending = true;

  // 🔧 条件发送使能命令（通过队列，优先级较低）
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

  // 🔧 注意：速度命令不再这里发送，改由 CAN task 周期性发送最新值

  // 更新使能状态（速度为0时标记为未使能，下次非零时重新使能）
  if (speed_left == 0) {
    motor_a_enabled = false;
  }
  if (speed_right == 0) {
    motor_b_enabled = false;
  }

  return 0;
}
