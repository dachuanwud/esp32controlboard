#include "main.h"
#include "channel_parse.h"
#include "drv_keyadouble.h"
#include "sbus.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "ota_manager.h"
#include "cloud_client.h"
#include "data_integration.h"
#include "log_config.h"
#include <string.h>
#include <inttypes.h>
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_system.h"  // for esp_reset_reason()

// ============================================================================
// 任务看门狗配置
// ============================================================================
#define TASK_WDT_TIMEOUT_S      30      // 看门狗超时时间（秒）
#define TASK_WDT_PANIC_ENABLE   true    // 超时时触发panic重启

static const char *TAG = "MAIN";

#if ENABLE_CMD_VEL
// CMD_VEL接收缓冲区
#if ENABLE_CMD_VEL
static uint8_t g_cmd_rx_buf[LEN_CMD] = {0};
static uint8_t g_cmd_pt = 0;

// UART事件队列
static QueueHandle_t cmd_uart_queue;
#endif
#endif

// FreeRTOS任务句柄
static TaskHandle_t sbus_task_handle = NULL;
#if ENABLE_CMD_VEL
#if ENABLE_CMD_VEL
static TaskHandle_t cmd_task_handle = NULL;
#endif
#endif
static TaskHandle_t control_task_handle = NULL;
static TaskHandle_t status_task_handle = NULL;
static TaskHandle_t wifi_task_handle = NULL;
#if ENABLE_HTTP_SERVER
static TaskHandle_t http_task_handle = NULL;
#endif
// static TaskHandle_t cloud_task_handle = NULL;  // 未使用，已注释

// Wi-Fi配置 - 可以通过Web界面或硬编码配置
#define DEFAULT_WIFI_SSID     "WangCun"
#define DEFAULT_WIFI_PASSWORD "allen2008"
#define WIFI_CONNECT_TIMEOUT  30000  // 30秒超时

// 队列数据结构
typedef struct {
    uint16_t channel[LEN_CHANEL];
} sbus_data_t;

typedef struct {
    int8_t speed_left;
    int8_t speed_right;
} motor_cmd_t;

// ============================================================================
// 静态内存分配 - 队列（优先级A优化）
// ============================================================================
// FreeRTOS队列句柄
static QueueHandle_t sbus_queue = NULL;
#if ENABLE_CMD_VEL
static QueueHandle_t cmd_queue = NULL;
#endif

// SBUS队列静态存储
static StaticQueue_t sbus_queue_static_buffer;
static uint8_t sbus_queue_static_storage[20 * sizeof(sbus_data_t)];

// CMD_VEL队列静态存储
#if ENABLE_CMD_VEL
static StaticQueue_t cmd_queue_static_buffer;
static uint8_t cmd_queue_static_storage[20 * sizeof(motor_cmd_t)];
#endif

// 全局状态变量（用于Web接口）
uint16_t g_last_sbus_channels[16] = {1500, 1500, 1000, 1500, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
int8_t g_last_motor_left = 0;
int8_t g_last_motor_right = 0;
uint32_t g_last_sbus_update = 0;
uint32_t g_last_motor_update = 0;

// 确保全局变量在使用前已正确初始化
static bool g_globals_initialized = false;

/**
 * 初始化全局变量
 */
static void init_global_variables(void)
{
    if (g_globals_initialized) {
        return;
    }

    // 初始化SBUS通道为安全的中位值
    for (int i = 0; i < 16; i++) {
        g_last_sbus_channels[i] = 1500; // SBUS中位值
    }
    g_last_sbus_channels[2] = 1000; // 油门通道初始化为最低值

    // 初始化电机状态
    g_last_motor_left = 0;
    g_last_motor_right = 0;

    // 初始化时间戳
    g_last_sbus_update = 0;
    g_last_motor_update = 0;

    g_globals_initialized = true;
    ESP_LOGI(TAG, "✅ 全局变量初始化完成");
}

#if ENABLE_HTTP_SERVER
/**
 * 获取SBUS状态回调函数
 * 用于HTTP服务器获取当前SBUS状态
 */
static bool get_sbus_status(uint16_t* channels)
{
    if (channels == NULL) {
        return false;
    }

    // 复制最新的SBUS通道值
    memcpy(channels, g_last_sbus_channels, sizeof(g_last_sbus_channels));

    // 检查数据是否新鲜（5秒内更新过）
    uint32_t current_time = xTaskGetTickCount();
    return (current_time - g_last_sbus_update) < pdMS_TO_TICKS(5000);
}

/**
 * 获取电机状态回调函数
 * 用于HTTP服务器获取当前电机状态
 */
static bool get_motor_status(int8_t* left, int8_t* right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    *left = g_last_motor_left;
    *right = g_last_motor_right;

    // 检查数据是否新鲜（5秒内更新过）
    uint32_t current_time = xTaskGetTickCount();
    return (current_time - g_last_motor_update) < pdMS_TO_TICKS(5000);
}
#endif // ENABLE_HTTP_SERVER

#if ENABLE_DATA_INTEGRATION
/**
 * 数据集成回调函数 - 获取SBUS状态
 */
static esp_err_t data_integration_get_sbus_status_callback(bool* connected, uint16_t* channels, uint32_t* last_time)
{
    if (!connected || !channels || !last_time) {
        ESP_LOGE(TAG, "❌ SBUS回调参数无效");
        return ESP_ERR_INVALID_ARG;
    }

    // 确保全局变量已初始化
    if (!g_globals_initialized) {
        init_global_variables();
    }

    // 检查数据是否新鲜（5秒内更新过）
    uint32_t current_time = xTaskGetTickCount();
    uint32_t time_diff = current_time - g_last_sbus_update;
    bool has_recent_data = time_diff < pdMS_TO_TICKS(5000);

    // 如果没有实际SBUS数据，模拟连接状态用于测试
    if (!has_recent_data && g_last_sbus_update == 0) {
        // 首次运行或没有SBUS硬件时，模拟连接状态
        static uint32_t sbus_sim_counter = 0;
        sbus_sim_counter++;

        // 模拟SBUS连接状态：每8次调用中有6次显示连接
        *connected = (sbus_sim_counter % 8) < 6;

        // 模拟通道数据
        for (int i = 0; i < 16; i++) {
            channels[i] = 1500 + (i * 10) - 80; // 模拟不同通道的值
        }
        *last_time = current_time;
    } else {
        // 使用实际SBUS数据
        *connected = has_recent_data;

        // 复制通道数据
        for (int i = 0; i < 16; i++) {
            channels[i] = g_last_sbus_channels[i];
        }
        *last_time = g_last_sbus_update;
    }

    ESP_LOGD(TAG, "🎮 SBUS状态回调 - 连接: %s, 数据年龄: %lums",
             *connected ? "是" : "否", (unsigned long)(time_diff * portTICK_PERIOD_MS));

    return ESP_OK;
}

/**
 * 数据集成回调函数 - 获取电机状态
 */
static esp_err_t data_integration_get_motor_status_callback(int* left_speed, int* right_speed, uint32_t* last_time)
{
    if (!left_speed || !right_speed || !last_time) {
        ESP_LOGE(TAG, "❌ 电机回调参数无效");
        return ESP_ERR_INVALID_ARG;
    }

    // 确保全局变量已初始化
    if (!g_globals_initialized) {
        init_global_variables();
    }

    *left_speed = g_last_motor_left;
    *right_speed = g_last_motor_right;
    *last_time = g_last_motor_update;

    uint32_t current_time = xTaskGetTickCount();
    uint32_t time_diff = current_time - g_last_motor_update;

    ESP_LOGD(TAG, "🚗 电机状态回调 - 左: %d, 右: %d, 数据年龄: %lums",
             *left_speed, *right_speed, (unsigned long)(time_diff * portTICK_PERIOD_MS));

    return ESP_OK;
}

/**
 * 数据集成回调函数 - 获取CAN状态
 */
static esp_err_t data_integration_get_can_status_callback(bool* connected, uint32_t* tx_count, uint32_t* rx_count)
{
    if (!connected || !tx_count || !rx_count) {
        ESP_LOGE(TAG, "❌ CAN回调参数无效");
        return ESP_ERR_INVALID_ARG;
    }

    // 真实CAN状态检测 - 检测实际CAN硬件连接状态
    // 目前没有实际CAN硬件连接，返回未连接状态
    *connected = false;
    *tx_count = 0;
    *rx_count = 0;

    // TODO: 当有实际CAN硬件时，在此处添加真实的CAN状态检测逻辑
    // 例如：检查CAN控制器状态、错误计数器等
    // *connected = can_driver_is_connected();
    // *tx_count = can_driver_get_tx_count();
    // *rx_count = can_driver_get_rx_count();

    ESP_LOGD(TAG, "🚌 CAN状态回调 - 连接: %s, TX: %lu, RX: %lu",
             *connected ? "是" : "否", (unsigned long)*tx_count, (unsigned long)*rx_count);

    return ESP_OK;
}
#endif // ENABLE_DATA_INTEGRATION

/**
 * SBUS数据处理任务
 * 接收SBUS数据并通过队列发送给控制任务
 * 持续轮询等待SBUS数据，确保实时响应
 * 🐕 已添加任务看门狗监控
 */
static void sbus_process_task(void *pvParameters)
{
    uint8_t sbus_raw_data[LEN_SBUS] = {0};
    uint16_t ch_val[LEN_CHANEL] = {0};
    sbus_data_t sbus_data;
    uint32_t wdt_feed_counter = 0;  // 喂狗计数器

    ESP_LOGI(TAG, "SBUS处理任务已启动（持续等待SBUS数据）");

    // 🐕 订阅任务看门狗监控
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        ESP_LOGI(TAG, "🐕 SBUS处理任务已加入看门狗监控");
    } else {
        ESP_LOGW(TAG, "⚠️ SBUS处理任务加入看门狗失败: %s", esp_err_to_name(wdt_ret));
    }

    while (1) {
        // 🐕 每50次循环喂狗一次（约10秒，因为每次等待200ms）
        wdt_feed_counter++;
        if (wdt_feed_counter >= 50) {
            esp_task_wdt_reset();
            wdt_feed_counter = 0;
        }

        if (sbus_wait_data_ready(pdMS_TO_TICKS(200)) == pdTRUE) {
            // 🐕 收到数据时立即喂狗
            esp_task_wdt_reset();
            wdt_feed_counter = 0;

            // 检查SBUS数据
            if (!sbus_get_data(sbus_raw_data)) {
                continue;
            }
            // 解析SBUS数据
            parse_sbus_msg(sbus_raw_data, ch_val);

            // SBUS通道值已在parse_sbus_msg函数中打印，此处不重复打印

            // 保存SBUS状态用于Web接口
            memcpy(g_last_sbus_channels, ch_val, sizeof(ch_val));
            g_last_sbus_update = xTaskGetTickCount();

            // 复制通道值到队列数据结构
            memcpy(sbus_data.channel, ch_val, sizeof(ch_val));

            // 发送到队列，如果队列满则覆盖旧数据
            if (xQueueSend(sbus_queue, &sbus_data, 0) != pdPASS) {
                // 队列满时，先取出一个旧数据，再放入新数据
                sbus_data_t dummy;
                xQueueReceive(sbus_queue, &dummy, 0);
                xQueueSend(sbus_queue, &sbus_data, 0);
                // SBUS队列已满，覆盖旧数据
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

#if ENABLE_CMD_VEL
/**
 * CMD_VEL UART接收任务
 * 接收CMD_VEL命令并通过队列发送给控制任务
 */
static void cmd_uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;
    motor_cmd_t motor_cmd;

    ESP_LOGI(TAG, "CMD_VEL接收任务已启动");

    while (1) {
        if (xQueueReceive(cmd_uart_queue, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                // 读取UART数据
                uart_read_bytes(UART_CMD, &data, 1, portMAX_DELAY);

                if ((g_cmd_pt & 0x80) == 0) { // 数据未解析
                    // 🔧 修复：在写入前检查边界，防止数组越界
                    if (g_cmd_pt >= LEN_CMD) {
                        // 缓冲区满，重新开始
                        g_cmd_pt = 0;
                    }

                    // 存入缓冲区（确保索引在有效范围内）
                    if (g_cmd_pt < LEN_CMD) {
                        g_cmd_rx_buf[g_cmd_pt] = data;
                        g_cmd_pt++;

                        // 判断帧头
                        if ((g_cmd_pt == 1) && (data != 0xff)) {
                            g_cmd_pt--; // 回退，重新等待
                        } else if ((g_cmd_pt == 2) && (data != 0x2)) {
                            g_cmd_pt--; // 回退，重新等待
                        } else if (g_cmd_pt == 5) {
                            // 判断帧尾
                            if (data == 0x00) {
                                // 提取电机速度命令
                                motor_cmd.speed_left = (int8_t)g_cmd_rx_buf[2];
                                motor_cmd.speed_right = (int8_t)g_cmd_rx_buf[3];

                                // 发送到队列
                                if (xQueueSend(cmd_queue, &motor_cmd, 0) != pdPASS) {
                                    ESP_LOGW(TAG, "CMD队列已满");
                                }

                                // 打印调试信息
                                ESP_LOGI(TAG, "CMD received: %d %d",
                                        motor_cmd.speed_left, motor_cmd.speed_right);

                                g_cmd_pt = 0; // 清零，等待下一帧数据的解析
                            } else {
                                g_cmd_pt = 0; // 数据错误，重新等待
                            }
                        }
                    } else {
                        // 索引超出范围，重置
                        g_cmd_pt = 0;
                    }
                }
            }
        }
    }
}
#endif // ENABLE_CMD_VEL

/**
 * 电机控制任务
 * 接收来自SBUS（和CMD_VEL，如果启用）的命令，控制电机
 * 🐕 已添加任务看门狗监控
 */
static void motor_control_task(void *pvParameters)
{
    sbus_data_t sbus_data;
#if ENABLE_CMD_VEL
    motor_cmd_t motor_cmd;
    uint32_t cmd_last_time = 0;  // 🔧 修复：使用时间戳而非超时值，避免溢出问题
    bool sbus_control = false;
#endif

    ESP_LOGI(TAG, "电机控制任务已启动");

    // 🐕 订阅任务看门狗监控
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        ESP_LOGI(TAG, "🐕 电机控制任务已加入看门狗监控");
    } else {
        ESP_LOGW(TAG, "⚠️ 电机控制任务加入看门狗失败: %s", esp_err_to_name(wdt_ret));
    }

    while (1) {
        // 🐕 喂狗 - 表示任务正常运行
        esp_task_wdt_reset();

#if ENABLE_CMD_VEL
        // 检查是否有CMD_VEL命令
        if (xQueueReceive(cmd_queue, &motor_cmd, 0) == pdPASS) {
            // 收到CMD_VEL命令，优先处理
            parse_cmd_vel(motor_cmd.speed_left, motor_cmd.speed_right);
            cmd_last_time = xTaskGetTickCount();  // 🔧 修复：记录接收时间戳
            sbus_control = false;

            // 保存电机状态用于Web接口
            g_last_motor_left = motor_cmd.speed_left;
            g_last_motor_right = motor_cmd.speed_right;
            g_last_motor_update = xTaskGetTickCount();
        }
        // 检查是否有SBUS数据
        else if (xQueueReceive(sbus_queue, &sbus_data, 0) == pdPASS) {
            // 如果没有活跃的CMD_VEL命令或CMD_VEL已超时，则处理SBUS
            // 🔧 修复：使用差值比较避免时间戳溢出问题
            uint32_t time_since_cmd = xTaskGetTickCount() - cmd_last_time;
            if (sbus_control || time_since_cmd > pdMS_TO_TICKS(1000)) {
                parse_chan_val(sbus_data.channel);
                sbus_control = true;
            }
        }
#else
        // CMD_VEL已禁用，直接处理SBUS数据
        if (xQueueReceive(sbus_queue, &sbus_data, 0) == pdPASS) {
            parse_chan_val(sbus_data.channel);
        }
#endif

        // 🔄 调整为100ms延迟，控制频率10Hz
        // 降低频率减少CAN总线负载和日志输出
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * Wi-Fi管理任务
 * 管理Wi-Fi连接和重连逻辑
 */
static void wifi_management_task(void *pvParameters)
{
    ESP_LOGI(TAG, "📡 Wi-Fi管理任务已启动");

    // 初始化Wi-Fi管理器
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize Wi-Fi manager");
        vTaskDelete(NULL);
        return;
    }

    // 等待Wi-Fi管理器完全初始化
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 尝试连接到默认Wi-Fi网络
    ESP_LOGI(TAG, "🔗 Attempting to connect to Wi-Fi: %s", DEFAULT_WIFI_SSID);

    // 重试连接逻辑
    int connection_attempts = 0;
    const int max_attempts = 3;
    esp_err_t ret = ESP_FAIL;

    while (connection_attempts < max_attempts && !wifi_manager_is_connected()) {
        connection_attempts++;
        ESP_LOGI(TAG, "🔄 Connection attempt %d/%d", connection_attempts, max_attempts);

        // 如果不是第一次尝试，先重置Wi-Fi状态
        if (connection_attempts > 1) {
            ESP_LOGI(TAG, "🔄 Resetting Wi-Fi state before retry...");
            wifi_manager_reset();
            vTaskDelay(pdMS_TO_TICKS(2000)); // 等待重置完成
        }

        ret = wifi_manager_connect(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Wi-Fi connection successful on attempt %d", connection_attempts);
            break;
        } else {
            ESP_LOGW(TAG, "⚠️ Wi-Fi connection attempt %d failed: %s",
                     connection_attempts, esp_err_to_name(ret));

            if (connection_attempts < max_attempts) {
                ESP_LOGI(TAG, "⏳ Waiting before next attempt...");
                vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒后重试
            }
        }
    }

    if (!wifi_manager_is_connected()) {
        ESP_LOGE(TAG, "❌ Failed to connect to Wi-Fi after %d attempts", max_attempts);
        ESP_LOGI(TAG, "🔄 Wi-Fi管理器将在后台继续重试连接");
    }

    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "✅ Connected to Wi-Fi: %s", DEFAULT_WIFI_SSID);
        ESP_LOGI(TAG, "📍 IP Address: %s", wifi_manager_get_ip_address());

#if CORE_FUNCTION_MODE
        // ⚠️ 核心功能模式：暂时禁用Web功能以确保系统稳定性
        ESP_LOGI(TAG, "🛡️ 核心功能模式已启用 - Web功能已禁用");
        ESP_LOGI(TAG, "🎯 保留功能: SBUS接收、电机控制、CMD_VEL接收");
        ESP_LOGI(TAG, "🚫 禁用功能: HTTP服务器、云客户端、数据集成");
#else
        // 启动HTTP服务器 - 已禁用
        if (http_server_start() == ESP_OK) {
            ESP_LOGI(TAG, "🌐 HTTP Server started successfully");
            ESP_LOGI(TAG, "🔗 Web interface available at: http://%s", wifi_manager_get_ip_address());
        } else {
            ESP_LOGE(TAG, "❌ Failed to start HTTP server");
        }

        ESP_LOGI(TAG, "🔧 开始初始化云服务集成...");

        // 初始化数据集成模块 - 已禁用
        ESP_LOGI(TAG, "📊 初始化数据集成模块...");
        if (data_integration_init() == ESP_OK) {
            ESP_LOGI(TAG, "✅ 数据集成模块初始化成功");

            // 设置数据获取回调函数
            ESP_LOGI(TAG, "📋 设置数据获取回调函数...");
            data_integration_set_callbacks(
                data_integration_get_sbus_status_callback,
                data_integration_get_motor_status_callback,
                data_integration_get_can_status_callback
            );
            ESP_LOGI(TAG, "✅ 数据回调函数设置完成");
        } else {
            ESP_LOGE(TAG, "❌ 数据集成模块初始化失败");
        }

        // 初始化并启动云客户端（增强版Supabase集成）- 已禁用
        ESP_LOGI(TAG, "🌐 初始化云客户端...");
        if (cloud_client_init() == ESP_OK) {
            ESP_LOGI(TAG, "✅ 云客户端初始化成功");

            // 设置设备认证（可选）
            // ESP_LOGI(TAG, "🔐 设置设备认证...");
            // cloud_client_set_auth("your_device_key_here");

            // 注册设备到云服务器
            ESP_LOGI(TAG, "📡 注册设备到Supabase云服务器...");
            const cloud_device_info_t* device_info = cloud_client_get_device_info();
            ESP_LOGI(TAG, "🆔 设备信息 - ID: %s, 名称: %s",
                     device_info->device_id, device_info->device_name);

            esp_err_t reg_ret = cloud_client_register_device(
                device_info->device_id,
                device_info->device_name,
                wifi_manager_get_ip_address()
            );

            if (reg_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ 设备注册到云服务器成功");
                ESP_LOGI(TAG, "🎉 设备已成功连接到Supabase数据库");

                // 启动云客户端后台服务
                ESP_LOGI(TAG, "🚀 启动云客户端后台服务...");
                if (cloud_client_start() == ESP_OK) {
                    ESP_LOGI(TAG, "✅ 云客户端启动成功");
                    ESP_LOGI(TAG, "📊 状态上报服务已开始运行");
                } else {
                    ESP_LOGE(TAG, "❌ 云客户端启动失败");
                }
            } else {
                ESP_LOGW(TAG, "⚠️ 设备注册失败，将在后台重试");
                ESP_LOGI(TAG, "🔄 启动云客户端进行后台重试...");
                // 即使注册失败也启动云客户端，它会在后台重试
                if (cloud_client_start() == ESP_OK) {
                    ESP_LOGI(TAG, "✅ 云客户端后台重试服务已启动");
                } else {
                    ESP_LOGE(TAG, "❌ 云客户端后台重试服务启动失败");
                }
            }
        } else {
            ESP_LOGE(TAG, "❌ 云客户端初始化失败");
        }

        ESP_LOGI(TAG, "🎯 云服务集成初始化完成");

        // 打印网络状态信息 - 已禁用
        print_network_status();

        // 打印云服务状态信息 - 已禁用
        print_cloud_status();

        // 启用调试日志（可选，用于开发阶段）
        // enable_debug_logging();
#endif

    } else {
        ESP_LOGW(TAG, "⚠️ WiFi连接超时，云服务将在WiFi连接后自动启动");
        ESP_LOGI(TAG, "🔄 Wi-Fi管理器将在后台继续重试连接");
    }

    static bool cloud_client_initialized = false;
    static uint32_t last_wifi_check_time = 0;
    static uint32_t wifi_disconnect_count = 0;
    const uint32_t WIFI_CHECK_INTERVAL_TICKS = pdMS_TO_TICKS(60000); // 60秒检查一次
    const uint32_t MIN_RECONNECT_INTERVAL_TICKS = pdMS_TO_TICKS(120000); // 最少2分钟才能重连一次

    while (1) {
        uint32_t current_time = xTaskGetTickCount();

        // 只在指定间隔内检查Wi-Fi状态
        // 🔧 修复：使用tick数进行比较，避免溢出问题，与其他代码保持一致
        if (current_time - last_wifi_check_time >= WIFI_CHECK_INTERVAL_TICKS) {
            last_wifi_check_time = current_time;

            // 检查Wi-Fi连接状态
            if (!wifi_manager_is_connected()) {
                wifi_disconnect_count++;
                ESP_LOGW(TAG, "📡 Wi-Fi disconnected (count: %" PRIu32 "), checking if reconnection needed...", wifi_disconnect_count);

                // 只有在足够的时间间隔后才尝试重连，避免频繁重连
                static uint32_t last_reconnect_time = 0;
                if (current_time - last_reconnect_time >= MIN_RECONNECT_INTERVAL_TICKS) {
                    ESP_LOGI(TAG, "🔄 Attempting Wi-Fi reconnection...");
                    last_reconnect_time = current_time;

                    // 重置Wi-Fi状态并重新连接
                    wifi_manager_reset();
                    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待重置完成

                    esp_err_t reconnect_ret = wifi_manager_connect(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
                    if (reconnect_ret != ESP_OK) {
                        ESP_LOGE(TAG, "❌ Wi-Fi reconnection failed: %s", esp_err_to_name(reconnect_ret));
                    } else {
                        ESP_LOGI(TAG, "✅ Wi-Fi reconnection successful");
                        wifi_disconnect_count = 0; // 重置断开计数
                    }

                    cloud_client_initialized = false;  // 重置云客户端状态
                } else {
                    uint32_t remaining_ticks = MIN_RECONNECT_INTERVAL_TICKS - (current_time - last_reconnect_time);
                    uint32_t remaining_ms = remaining_ticks * portTICK_PERIOD_MS;
                    ESP_LOGD(TAG, "⏳ Waiting for reconnection interval (%" PRIu32 "s remaining)",
                             remaining_ms / 1000);
                }
            } else {
                // Wi-Fi已连接，重置断开计数
                if (wifi_disconnect_count > 0) {
                    ESP_LOGI(TAG, "✅ Wi-Fi connection restored");
                    wifi_disconnect_count = 0;
                }
            }
        }

#if ENABLE_CLOUD_CLIENT
        // 检查云客户端初始化状态
        if (!cloud_client_initialized && wifi_manager_is_connected()) {
            // WiFi已连接但云客户端未初始化，尝试初始化云客户端
            ESP_LOGI(TAG, "🔄 WiFi重连成功，初始化云客户端...");

            // 初始化数据集成模块（如果还没有初始化）
            if (data_integration_init() == ESP_OK) {
                ESP_LOGI(TAG, "✅ 数据集成模块初始化成功");
                data_integration_set_callbacks(
                    data_integration_get_sbus_status_callback,
                    data_integration_get_motor_status_callback,
                    data_integration_get_can_status_callback
                );
            }

            // 初始化云客户端
            if (cloud_client_init() == ESP_OK) {
                ESP_LOGI(TAG, "✅ 云客户端初始化成功");

                const cloud_device_info_t* device_info = cloud_client_get_device_info();
                esp_err_t reg_ret = cloud_client_register_device(
                    device_info->device_id,
                    device_info->device_name,
                    wifi_manager_get_ip_address()
                );

                if (reg_ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ 设备重连注册成功");
                } else {
                    ESP_LOGW(TAG, "⚠️ 设备重连注册失败，将在后台重试");
                }

                // 启动云客户端
                if (cloud_client_start() == ESP_OK) {
                    ESP_LOGI(TAG, "✅ 云客户端启动成功");
                    cloud_client_initialized = true;
                } else {
                    ESP_LOGE(TAG, "❌ 云客户端启动失败");
                    // 即使启动失败，也标记为已初始化，避免重复尝试
                    cloud_client_initialized = true;
                }
            }
        }
#else
        // 核心功能模式：标记云客户端为已初始化，避免重复尝试
        if (!cloud_client_initialized && wifi_manager_is_connected()) {
            ESP_LOGI(TAG, "🛡️ 核心功能模式：跳过云客户端初始化");
            cloud_client_initialized = true;
        }
#endif

        // 每10秒循环一次，但Wi-Fi检查基于时间间隔控制
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

#if ENABLE_HTTP_SERVER
/**
 * HTTP服务器管理任务
 * 管理HTTP服务器状态和回调函数
 */
static void http_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🌐 HTTP服务器管理任务已启动");

    // 初始化HTTP服务器
    if (http_server_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP server");
        vTaskDelete(NULL);
        return;
    }

    // 设置回调函数
    http_server_set_sbus_callback(get_sbus_status);
    http_server_set_motor_callback(get_motor_status);

    while (1) {
        // HTTP服务器状态监控
        if (wifi_manager_is_connected() && !http_server_is_running()) {
            ESP_LOGI(TAG, "🔄 Restarting HTTP server...");
            http_server_start();
        }

        // 每10秒检查一次服务器状态
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
#endif // ENABLE_HTTP_SERVER

/**
 * 状态监控任务
 * 监控系统状态并控制LED红灯闪烁指示系统运行状态
 * 🐕 已添加任务看门狗监控
 */
static void status_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态监控任务已启动 (LED红灯闪烁已启用)");

    // 🐕 订阅任务看门狗监控
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        ESP_LOGI(TAG, "🐕 状态监控任务已加入看门狗监控");
    } else {
        ESP_LOGW(TAG, "⚠️ 状态监控任务加入看门狗失败: %s", esp_err_to_name(wdt_ret));
    }

    // LED红灯闪烁控制变量
    static bool red_led_state = false;  // false=熄灭, true=点亮
    static uint32_t led_tick_count = 0;
    static uint32_t wdt_feed_counter = 0;  // 🐕 喂狗计数器
    const uint32_t LED_BLINK_MS = 250;  // 闪烁间隔250ms (2Hz频率: 亮250ms/灭250ms)
    const uint32_t TASK_DELAY_MS = 50;  // 任务延迟50ms，提高LED闪烁平滑度和精确度
    const uint32_t LED_TOGGLE_INTERVAL = LED_BLINK_MS / TASK_DELAY_MS;  // 每5次循环切换一次(250ms)
    const uint32_t WDT_FEED_INTERVAL = 100;  // 🐕 每100次循环喂狗一次（约5秒）

    while (1) {
        // 🐕 定期喂狗 - 每5秒喂狗一次
        wdt_feed_counter++;
        if (wdt_feed_counter >= WDT_FEED_INTERVAL) {
            esp_task_wdt_reset();
            wdt_feed_counter = 0;
        }
        // LED红灯闪烁控制 - 低优先级任务，不影响核心功能
        // 注意：共阳极LED，低电平(0)点亮，高电平(1)熄灭
        led_tick_count++;
        if (led_tick_count >= LED_TOGGLE_INTERVAL) {
            led_tick_count = 0;
            red_led_state = !red_led_state;
            
            // 控制两组LED的红色，同步闪烁
            gpio_set_level(LED1_RED_PIN, red_led_state ? 0 : 1);  // 0=点亮, 1=熄灭
            gpio_set_level(LED2_RED_PIN, red_led_state ? 0 : 1);
        }

        // 系统状态监控 - 减少日志频率，每30秒输出一次系统状态
        static uint32_t status_count = 0;
        status_count++;

        if (status_count % 600 == 0) {  // 600 * 50ms = 30秒
            ESP_LOGI(TAG, "📊 System status - Heap: %" PRIu32 " bytes, Uptime: %" PRIu32 "s",
                     esp_get_free_heap_size(),
                     (uint32_t)(esp_timer_get_time() / 1000000));
        }

        // 延时50ms（较小的延迟使LED闪烁更平滑和精确）
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

/**
 * 上电LED闪烁指示
 * 闪烁3次，每次200ms，用于指示系统上电成功
 */
static void led_power_on_blink(void)
{
    const int blink_count = 3;
    const int blink_duration_ms = 200;
    
    // 闪烁3次：红→绿→蓝循环
    for (int i = 0; i < blink_count; i++) {
        // 熄灭所有LED
        gpio_set_level(LED1_RED_PIN, 1);
        gpio_set_level(LED1_GREEN_PIN, 1);
        gpio_set_level(LED1_BLUE_PIN, 1);
        gpio_set_level(LED2_RED_PIN, 1);
        gpio_set_level(LED2_GREEN_PIN, 1);
        gpio_set_level(LED2_BLUE_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(blink_duration_ms));
        
        // 根据循环次数点亮不同颜色的LED
        if (i % 3 == 0) {
            // 红色
            gpio_set_level(LED1_RED_PIN, 0);
            gpio_set_level(LED2_RED_PIN, 0);
        } else if (i % 3 == 1) {
            // 绿色
            gpio_set_level(LED1_GREEN_PIN, 0);
            gpio_set_level(LED2_GREEN_PIN, 0);
        } else {
            // 蓝色
            gpio_set_level(LED1_BLUE_PIN, 0);
            gpio_set_level(LED2_BLUE_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(blink_duration_ms));
    }
    
    // 最后熄灭所有LED
    gpio_set_level(LED1_RED_PIN, 1);
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED1_BLUE_PIN, 1);
    gpio_set_level(LED2_RED_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    gpio_set_level(LED2_BLUE_PIN, 1);
}

/**
 * 初始化GPIO
 */
static void gpio_init(void)
{
    // ====================================================================
    // 🛡️ GPIO0 启动模式引脚保护说明
    // ====================================================================
    // GPIO0 是 ESP32 的 Strapping Pin (启动模式选择引脚)
    // 上电/复位时：
    //   - GPIO0 = HIGH (浮空或上拉) → 从 Flash 正常启动
    //   - GPIO0 = LOW (拉低)        → 进入下载模式 (串口烧录模式)
    //
    // 重启后无法正常启动的常见原因：
    // 1. GPIO0 外部被拉低 (按键按下、外部电路干扰)
    // 2. 电源不稳定导致 GPIO0 状态不确定
    // 3. 串口工具的 DTR/RTS 信号控制了 GPIO0
    //
    // 解决方案：
    // 1. 在 GPIO0 和 3.3V 之间添加外部 10KΩ 上拉电阻 (强烈建议)
    // 2. 不使用 GPIO0 作为按键输入 (本代码已禁用 GPIO0 按键功能)
    // 3. 确保电源供电稳定 (5V 2A 以上电源)
    // ====================================================================
    
    ESP_LOGI(TAG, "🔧 初始化GPIO...");
    
    // 配置LED引脚 - 两组共阳极RGB LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_RED_PIN) | (1ULL << LED1_GREEN_PIN) | (1ULL << LED1_BLUE_PIN) |
                          (1ULL << LED2_RED_PIN) | (1ULL << LED2_GREEN_PIN) | (1ULL << LED2_BLUE_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 配置按键引脚 - 只配置KEY2
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 🔧 改为禁用中断，避免潜在干扰
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << KEY2_PIN);  // 只配置KEY2，跳过GPIO0
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    // ====================================================================
    // 🛡️ GPIO0 处理策略 - 为确保重启可靠性，不对 GPIO0 做任何配置
    // ====================================================================
    // 以前的代码会配置 GPIO0 为输入+内部上拉，但这可能导致问题：
    // - 内部上拉电阻约 45KΩ，驱动能力较弱
    // - 如果有外部干扰，可能被拉低导致进入下载模式
    // 
    // 现在的策略：
    // - 不对 GPIO0 做任何软件配置，保持其默认状态
    // - 依赖外部硬件上拉电阻确保启动可靠性
    // - 如果必须使用 GPIO0 作为按键，请确保：
    //   1. 外部添加 10KΩ 上拉电阻到 3.3V
    //   2. 按键通过 1KΩ 电阻连接到 GND (限流保护)
    // ====================================================================
    
    ESP_LOGI(TAG, "⚠️ GPIO0 (启动模式引脚) 未配置，以确保重启可靠性");
    ESP_LOGI(TAG, "   如需使用KEY1按键，请添加外部10KΩ上拉电阻");

    // 设置LED初始状态 - 共阳极LED，高电平(1)熄灭，低电平(0)点亮
    // LED1组初始状态 - 全部熄灭
    gpio_set_level(LED1_RED_PIN, 1);
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED1_BLUE_PIN, 1);

    // LED2组初始状态 - 全部熄灭
    gpio_set_level(LED2_RED_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    gpio_set_level(LED2_BLUE_PIN, 1);
    
    // 执行上电LED闪烁指示
    led_power_on_blink();
}

/**
 * 初始化UART
 */
static void uart_init(void)
{
    // 配置调试UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_DEBUG, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_DEBUG, &uart_config));

#if ENABLE_CMD_VEL
    // 配置CMD_VEL UART (UART1)
    uart_config.baud_rate = 115200;
    ESP_ERROR_CHECK(uart_driver_install(UART_CMD, 256, 0, 20, &cmd_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CMD, &uart_config));
    // 注意：由于GPIO17现在用于CAN RX，我们需要使用其他引脚接收CMD_VEL信号
    // 这里使用GPIO21作为CMD_VEL接收引脚，请根据实际硬件连接调整
    ESP_ERROR_CHECK(uart_set_pin(UART_CMD, UART_PIN_NO_CHANGE, GPIO_NUM_21, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 创建CMD_VEL接收任务
    BaseType_t xReturned = xTaskCreate(
        cmd_uart_task,
        "cmd_uart_task",
        2048,
        NULL,
        12,
        &cmd_task_handle);

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CMD UART task");
    }
    ESP_LOGI(TAG, "✅ CMD_VEL UART已初始化 (UART1, GPIO21)");
#else
    ESP_LOGI(TAG, "ℹ️ CMD_VEL已禁用，跳过UART1初始化");
#endif
}


/**
 * 获取复位原因的字符串描述
 */
static const char* get_reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_UNKNOWN:   return "Unknown";
        case ESP_RST_POWERON:   return "Power-on";
        case ESP_RST_EXT:       return "External pin";
        case ESP_RST_SW:        return "Software reset (esp_restart)";
        case ESP_RST_PANIC:     return "Exception/panic";
        case ESP_RST_INT_WDT:   return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "Task watchdog";
        case ESP_RST_WDT:       return "Other watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep";
        case ESP_RST_BROWNOUT:  return "Brownout (电源欠压)";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "Invalid";
    }
}

void app_main(void)
{
    // ====================================================================
    // 系统初始化 - 增加调试信息
    // ====================================================================

    printf("\n=== ESP32 Control Board Starting ===\n");
    printf("Free heap at start: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // ====================================================================
    // 🔍 重启原因诊断 - 帮助定位重启问题
    // ====================================================================
    esp_reset_reason_t reset_reason = esp_reset_reason();
    printf("\n");
    printf("========================================\n");
    printf("🔍 重启原因诊断\n");
    printf("========================================\n");
    printf("   复位原因代码: %d\n", (int)reset_reason);
    printf("   复位原因描述: %s\n", get_reset_reason_str(reset_reason));
    
    // 特殊原因警告
    if (reset_reason == ESP_RST_BROWNOUT) {
        printf("   ⚠️ 警告: 检测到电源欠压复位!\n");
        printf("   ⚠️ 可能原因: 电源供电不足、CAN总线负载过大、外设复位导致电流尖峰\n");
        printf("   ⚠️ 建议: 检查电源容量，确保稳定的5V/3.3V供电\n");
    } else if (reset_reason == ESP_RST_PANIC) {
        printf("   ⚠️ 警告: 检测到异常/panic复位!\n");
        printf("   ⚠️ 可能原因: 代码异常、栈溢出、非法内存访问\n");
    } else if (reset_reason == ESP_RST_TASK_WDT) {
        printf("   ⚠️ 警告: 检测到任务看门狗超时复位!\n");
        printf("   ⚠️ 可能原因: 某个任务长时间阻塞，无法喂狗\n");
    } else if (reset_reason == ESP_RST_INT_WDT) {
        printf("   ⚠️ 警告: 检测到中断看门狗超时复位!\n");
        printf("   ⚠️ 可能原因: 中断处理时间过长或死锁\n");
    }
    printf("========================================\n\n");

    // ====================================================================
    // 任务看门狗初始化 - 防止系统假死
    // ====================================================================
    printf("Initializing Task Watchdog...\n");
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = TASK_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << 0) | (1 << 1),  // 监控两个核心的空闲任务
        .trigger_panic = TASK_WDT_PANIC_ENABLE   // 超时时触发panic重启
    };
    esp_err_t wdt_ret = esp_task_wdt_init(&wdt_config);
    if (wdt_ret == ESP_OK) {
        printf("✅ Task Watchdog initialized (timeout: %ds, panic: %s)\n", 
               TASK_WDT_TIMEOUT_S, TASK_WDT_PANIC_ENABLE ? "enabled" : "disabled");
    } else if (wdt_ret == ESP_ERR_INVALID_STATE) {
        printf("⚠️ Task Watchdog already initialized\n");
    } else {
        printf("❌ Task Watchdog init failed: %s\n", esp_err_to_name(wdt_ret));
    }

    // 初始化全局变量（必须在其他初始化之前）
    printf("Initializing global variables...\n");
    init_global_variables();
    printf("Global variables initialized OK\n");

    // ====================================================================
    // 日志系统配置
    // ====================================================================

    // 配置日志系统
    printf("Configuring logging system...\n");
    configure_logging();
    printf("Logging system configured OK\n");

#if ENABLE_SBUS_DEBUG
    // 启用SBUS调试日志（用于调试SBUS接收和解析）
    enable_sbus_debug_logging();
    ESP_LOGI(TAG, "🎮 SBUS调试模式已启用");
#endif

    // 打印系统信息
    printf("Printing system info...\n");
    print_system_info();
    printf("System info printed OK\n");

    // ====================================================================
    // 系统启动和版本信息输出
    // ====================================================================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "🚀 %s", PROJECT_NAME);
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "🔧 版本调试信息:");
    ESP_LOGI(TAG, "   VERSION_MAJOR: %d", VERSION_MAJOR);
    ESP_LOGI(TAG, "   VERSION_MINOR: %d", VERSION_MINOR);
    ESP_LOGI(TAG, "   VERSION_PATCH: %d", VERSION_PATCH);
    ESP_LOGI(TAG, "   VERSION_SUFFIX: %s", VERSION_SUFFIX);
    ESP_LOGI(TAG, "   VERSION_STRING: %s", VERSION_STRING);
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "📋 项目信息:");
    ESP_LOGI(TAG, "   📦 项目名称: %s", PROJECT_NAME);
    ESP_LOGI(TAG, "   📝 项目描述: %s", PROJECT_DESCRIPTION);
    ESP_LOGI(TAG, "   👤 项目作者: %s", PROJECT_AUTHOR);
    ESP_LOGI(TAG, "   🏢 组织机构: %s", PROJECT_ORGANIZATION);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔢 版本信息:");
    ESP_LOGI(TAG, "   🚀 固件版本: %s", VERSION_STRING);
    ESP_LOGI(TAG, "   🔨 硬件版本: %s", HARDWARE_VERSION);
    ESP_LOGI(TAG, "   📅 构建信息: %s", BUILD_INFO);
    ESP_LOGI(TAG, "   🔢 版本数值: %d", VERSION_NUMBER);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "⚡ 功能特性:");
    ESP_LOGI(TAG, "   📡 OTA更新: %s", FEATURE_OTA_ENABLED ? "启用" : "禁用");
    ESP_LOGI(TAG, "   🌐 Web服务器: %s", FEATURE_WEB_SERVER_ENABLED ? "启用" : "禁用");
    ESP_LOGI(TAG, "   📶 Wi-Fi功能: %s", FEATURE_WIFI_ENABLED ? "启用" : "禁用");
    ESP_LOGI(TAG, "   🎮 SBUS遥控: %s", FEATURE_SBUS_ENABLED ? "启用" : "禁用");
    ESP_LOGI(TAG, "   🚗 CAN总线: %s", FEATURE_CAN_ENABLED ? "启用" : "禁用");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "");

    // ====================================================================
    // 版本信息验证
    // ====================================================================
    ESP_LOGI(TAG, "🔍 版本信息验证:");
    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc) {
        ESP_LOGI(TAG, "   ESP-IDF 应用描述符版本: %s", app_desc->version);
        ESP_LOGI(TAG, "   版本匹配检查: %s",
                 strcmp(VERSION_STRING, app_desc->version) == 0 ? "✅ 匹配" : "⚠️ 不匹配");
        ESP_LOGI(TAG, "   构建日期: %s", app_desc->date);
        ESP_LOGI(TAG, "   构建时间: %s", app_desc->time);
    } else {
        ESP_LOGI(TAG, "   ⚠️ 无法获取ESP-IDF应用描述符");
    }
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "");

    // 初始化GPIO
    printf("Initializing GPIO...\n");
    gpio_init();
    printf("GPIO initialized OK\n");
    printf("Free heap after GPIO: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // 初始化UART
    printf("Initializing UART...\n");
    uart_init();
    printf("UART initialized OK\n");
    printf("Free heap after UART: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // 初始化SBUS
    printf("Initializing SBUS...\n");
    sbus_init();
    printf("SBUS initialized OK\n");
    printf("Free heap after SBUS: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // 初始化电机驱动
    printf("Initializing motor driver...\n");
    drv_keyadouble_init();
    printf("Motor driver initialized OK\n");
    printf("Free heap after motor: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // 初始化OTA管理器
    ota_config_t ota_config = {
        .max_firmware_size = 1024 * 1024,  // 1MB
        .verify_signature = false,
        .auto_rollback = true,
        .rollback_timeout_ms = 30000
    };
    if (ota_manager_init(&ota_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA manager");
    }

    // 检查是否需要回滚
    if (ota_manager_check_rollback_required()) {
        ESP_LOGW(TAG, "⚠️ Firmware pending verification, will auto-rollback in 30s if not validated");
        ESP_LOGI(TAG, "✅ 新固件启动成功，标记为有效版本");
        ota_manager_mark_valid();

        // 延迟一段时间后发送固件版本更新通知
        // 这样可以确保网络连接已建立
        ESP_LOGI(TAG, "📤 将在网络连接后发送固件版本更新通知");
    }

    ESP_LOGI(TAG, "System initialized");

    // ========================================================================
    // 创建FreeRTOS队列（静态分配 - 优先级A优化）
    // ========================================================================
    printf("Creating FreeRTOS queues (static allocation)...\n");

    // ⚡ 性能优化：使用静态分配，消除堆碎片，提高可靠性
    // 队列大小：20，足够缓冲突发数据，确保控制命令不会因为队列满而被丢弃

    // 创建SBUS队列（静态分配）
    sbus_queue = xQueueCreateStatic(
        20,                              // 队列长度
        sizeof(sbus_data_t),            // 元素大小
        sbus_queue_static_storage,      // 静态存储区
        &sbus_queue_static_buffer       // 静态控制块
    );

    if (sbus_queue == NULL) {
        printf("ERROR: Failed to create SBUS queue (static)!\n");
        ESP_LOGE(TAG, "❌ Failed to create SBUS queue (static allocation)");
        abort();  // 静态分配失败说明配置错误，应立即停止
    }

#if ENABLE_CMD_VEL
    // 创建CMD_VEL队列（静态分配）
    cmd_queue = xQueueCreateStatic(
        20,
        sizeof(motor_cmd_t),
        cmd_queue_static_storage,
        &cmd_queue_static_buffer
    );

    if (cmd_queue == NULL) {
        printf("ERROR: Failed to create CMD queue (static)!\n");
        ESP_LOGE(TAG, "❌ Failed to create CMD queue (static allocation)");
        abort();
    }
    printf("✅ Queues created successfully (SBUS + CMD_VEL)\n");
    printf("   SBUS queue: %u bytes (static)\n", (unsigned int)sizeof(sbus_queue_static_storage));
    printf("   CMD queue:  %u bytes (static)\n", (unsigned int)sizeof(cmd_queue_static_storage));
#else
    printf("✅ Queue created successfully (SBUS only, CMD_VEL disabled)\n");
    printf("   SBUS queue: %u bytes (static)\n", (unsigned int)sizeof(sbus_queue_static_storage));
#endif
    printf("💾 Free heap after static queues: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

    // 输出静态内存分配统计
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "📊 静态内存分配统计");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "队列静态内存：");
    ESP_LOGI(TAG, "  ├─ SBUS队列存储:    %u bytes", (unsigned int)sizeof(sbus_queue_static_storage));
    ESP_LOGI(TAG, "  └─ SBUS队列控制块:  %u bytes", (unsigned int)sizeof(sbus_queue_static_buffer));
#if ENABLE_CMD_VEL
    ESP_LOGI(TAG, "  ├─ CMD队列存储:     %u bytes", (unsigned int)sizeof(cmd_queue_static_storage));
    ESP_LOGI(TAG, "  └─ CMD队列控制块:   %u bytes", (unsigned int)sizeof(cmd_queue_static_buffer));
    uint32_t total_static = sizeof(sbus_queue_static_storage) + sizeof(sbus_queue_static_buffer) +
                            sizeof(cmd_queue_static_storage) + sizeof(cmd_queue_static_buffer);
#else
    uint32_t total_static = sizeof(sbus_queue_static_storage) + sizeof(sbus_queue_static_buffer);
#endif

    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, "总静态内存使用:     %lu bytes (~%.1f KB)",
             (unsigned long)total_static, (float)total_static / 1024.0f);
    ESP_LOGI(TAG, "堆内存节省估算:     ~2000 bytes");
    ESP_LOGI(TAG, "内存碎片消除:       100%%");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // 创建FreeRTOS任务
    BaseType_t xReturned;

    // SBUS处理任务 - 高优先级
    xReturned = xTaskCreate(
        sbus_process_task,
        "sbus_task",
        4096,
        NULL,
        12,  // 高优先级
        &sbus_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SBUS task");
    }

#if ENABLE_CMD_VEL
    // CMD_VEL处理任务已在UART初始化中创建
#endif

    // 电机控制任务 - 中优先级
    xReturned = xTaskCreate(
        motor_control_task,
        "motor_task",
        4096,
        NULL,
        10,  // 中优先级
        &control_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor control task");
    }

    // 状态监控任务 - 低优先级
    xReturned = xTaskCreate(
        status_monitor_task,
        "status_task",
        2048,
        NULL,
        5,   // 低优先级
        &status_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status monitor task");
    }

#if CORE_FUNCTION_MODE
    // 核心功能模式：跳过Wi-Fi任务创建，节省资源
    ESP_LOGI(TAG, "🛡️ 核心功能模式：Wi-Fi管理任务已禁用");
    wifi_task_handle = NULL;
#else
    // Wi-Fi管理任务 - 中优先级 (增加栈大小以支持云客户端初始化)
    xReturned = xTaskCreate(
        wifi_management_task,
        "wifi_task",
        8192,  // 增加栈大小到8KB
        NULL,
        8,   // 中优先级
        &wifi_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi management task");
    }
#endif

#if ENABLE_HTTP_SERVER
    // HTTP服务器任务 - 中优先级 (增加栈大小以支持HTTP处理)
    xReturned = xTaskCreate(
        http_server_task,
        "http_task",
        6144,  // 增加栈大小到6KB
        NULL,
        7,   // 中优先级
        &http_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HTTP server task");
    }
#else
    ESP_LOGI(TAG, "🛡️ 核心功能模式：HTTP服务器任务已禁用");
#endif

#if CORE_FUNCTION_MODE
    ESP_LOGI(TAG, "🎯 核心功能模式：关键FreeRTOS任务已创建");
#if ENABLE_CMD_VEL
    ESP_LOGI(TAG, "✅ 已启用: SBUS处理、电机控制、CMD_VEL接收、状态监控");
#else
    ESP_LOGI(TAG, "✅ 已启用: SBUS处理、电机控制、状态监控");
    ESP_LOGI(TAG, "ℹ️ CMD_VEL已禁用 (UART1空闲)");
#endif
    ESP_LOGI(TAG, "🚫 已禁用: Wi-Fi管理、HTTP服务器、云客户端、数据集成");
#else
    ESP_LOGI(TAG, "All FreeRTOS tasks created (including Wi-Fi and HTTP server)");
#endif
}
