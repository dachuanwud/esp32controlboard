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

static const char *TAG = "MAIN";

// CMD_VEL接收缓冲区
static uint8_t g_cmd_rx_buf[LEN_CMD] = {0};
static uint8_t g_cmd_pt = 0;

// UART事件队列
static QueueHandle_t cmd_uart_queue;

// 定时器句柄
static TimerHandle_t brake_timer_left = NULL;
static TimerHandle_t brake_timer_right = NULL;

// FreeRTOS任务句柄
static TaskHandle_t sbus_task_handle = NULL;
static TaskHandle_t cmd_task_handle = NULL;
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

// FreeRTOS队列句柄
static QueueHandle_t sbus_queue = NULL;
static QueueHandle_t cmd_queue = NULL;

// 队列数据结构
typedef struct {
    uint16_t channel[LEN_CHANEL];
} sbus_data_t;

typedef struct {
    int8_t speed_left;
    int8_t speed_right;
} motor_cmd_t;

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
 * 左刹车定时器回调函数
 */
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    if (bk_flag_left == 0) {
        // 通过CAN总线发送刹车命令
        ESP_LOGI(TAG, "Left brake applied");
        // 注销LED指示 - 红色LED亮起表示刹车（共阳极LED，低电平点亮）
        // gpio_set_level(LED1_RED_PIN, 0);
        // gpio_set_level(LED2_RED_PIN, 0);
    }
}

/**
 * 右刹车定时器回调函数
 */
static void brake_timer_right_callback(TimerHandle_t xTimer)
{
    if (bk_flag_right == 0) {
        // 通过CAN总线发送刹车命令
        ESP_LOGI(TAG, "Right brake applied");
        // 注销LED指示 - 红色LED亮起表示刹车（共阳极LED，低电平点亮）
        // gpio_set_level(LED1_RED_PIN, 0);
        // gpio_set_level(LED2_RED_PIN, 0);
    }
}

/**
 * SBUS数据处理任务
 * 接收SBUS数据并通过队列发送给控制任务
 */
static void sbus_process_task(void *pvParameters)
{
    uint8_t sbus_raw_data[LEN_SBUS] = {0};
    uint16_t ch_val[LEN_CHANEL] = {0};
    sbus_data_t sbus_data;

    // SBUS处理任务已启动

    while (1) {
        // 检查SBUS数据
        if (sbus_get_data(sbus_raw_data)) {
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
        }

        // 短暂延时，避免过度占用CPU（平衡性能和稳定性）
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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
                    if (g_cmd_pt > (LEN_CMD - 1)) {
                        // 缓冲区满，重新开始
                        g_cmd_pt = 0;
                    }

                    // 存入缓冲区
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
                }
            }
        }
    }
}

/**
 * 电机控制任务
 * 接收来自SBUS和CMD_VEL的命令，控制电机
 */
static void motor_control_task(void *pvParameters)
{
    sbus_data_t sbus_data;
    motor_cmd_t motor_cmd;
    uint32_t cmd_timeout = 0;
    bool sbus_control = false;

    ESP_LOGI(TAG, "电机控制任务已启动");

    while (1) {
        // 检查是否有CMD_VEL命令
        if (xQueueReceive(cmd_queue, &motor_cmd, 0) == pdPASS) {
            // 收到CMD_VEL命令，优先处理
            parse_cmd_vel(motor_cmd.speed_left, motor_cmd.speed_right);
            cmd_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(1000); // 1秒超时
            sbus_control = false;

            // 保存电机状态用于Web接口
            g_last_motor_left = motor_cmd.speed_left;
            g_last_motor_right = motor_cmd.speed_right;
            g_last_motor_update = xTaskGetTickCount();

            // 注销LED指示 - 接收到CMD_VEL命令时，两组LED的绿色闪烁
            // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
            // gpio_set_level(LED1_GREEN_PIN, !gpio_get_level(LED1_GREEN_PIN));
            // gpio_set_level(LED2_GREEN_PIN, !gpio_get_level(LED2_GREEN_PIN));
        }
        // 检查是否有SBUS数据
        else if (xQueueReceive(sbus_queue, &sbus_data, 0) == pdPASS) {
            // 如果没有活跃的CMD_VEL命令或CMD_VEL已超时，则处理SBUS
            if (sbus_control || xTaskGetTickCount() > cmd_timeout) {
                parse_chan_val(sbus_data.channel);
                sbus_control = true;

                // 注销LED指示 - 接收到SBUS命令时，两组LED的蓝色闪烁
                // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
                // gpio_set_level(LED1_BLUE_PIN, !gpio_get_level(LED1_BLUE_PIN));
                // gpio_set_level(LED2_BLUE_PIN, !gpio_get_level(LED2_BLUE_PIN));
            }
        }

        // 短暂延时，避免过度占用CPU（平衡性能和稳定性）
        vTaskDelay(pdMS_TO_TICKS(10));
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
    const uint32_t WIFI_CHECK_INTERVAL_MS = 60000; // 增加到60秒检查一次
    const uint32_t MIN_RECONNECT_INTERVAL_MS = 120000; // 最少2分钟才能重连一次

    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 只在指定间隔内检查Wi-Fi状态
        if (current_time - last_wifi_check_time >= WIFI_CHECK_INTERVAL_MS) {
            last_wifi_check_time = current_time;

            // 检查Wi-Fi连接状态
            if (!wifi_manager_is_connected()) {
                wifi_disconnect_count++;
                ESP_LOGW(TAG, "📡 Wi-Fi disconnected (count: %" PRIu32 "), checking if reconnection needed...", wifi_disconnect_count);

                // 只有在足够的时间间隔后才尝试重连，避免频繁重连
                static uint32_t last_reconnect_time = 0;
                if (current_time - last_reconnect_time >= MIN_RECONNECT_INTERVAL_MS) {
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
                    ESP_LOGD(TAG, "⏳ Waiting for reconnection interval (%" PRIu32 "s remaining)",
                             (MIN_RECONNECT_INTERVAL_MS - (current_time - last_reconnect_time)) / 1000);
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
 * 监控系统状态（LED显示功能已注销）
 */
static void status_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态监控任务已启动 (LED显示已注销)");

    while (1) {
        // 注销LED循环显示功能 - 原本循环显示不同颜色，表示系统正常运行
        // 注意：共阳极LED，低电平(0)点亮，高电平(1)熄灭
        /*
        switch (led_state) {
            case 0: // 红色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 0);  // 红色点亮
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 0);  // 红色点亮
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
            case 1: // 绿色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 0); // 绿色点亮
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 0); // 绿色点亮
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
            case 2: // 蓝色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 0);  // 蓝色点亮
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 0);  // 蓝色点亮
                break;
            case 3: // 全部关闭
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
        }

        // 更新状态
        led_state = (led_state + 1) % 4;
        */

        // 系统状态监控任务保持运行，但不进行LED显示
        // 可以在此处添加其他系统状态监控逻辑

        // 减少状态监控日志频率，每30秒输出一次系统状态
        static uint32_t status_count = 0;
        status_count++;

        if (status_count % 60 == 0) {  // 60 * 500ms = 30秒
            ESP_LOGI(TAG, "📊 System status - Heap: %" PRIu32 " bytes, Uptime: %" PRIu32 "s",
                     esp_get_free_heap_size(),
                     (uint32_t)(esp_timer_get_time() / 1000000));
        }

        // 延时500ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * 初始化GPIO
 */
static void gpio_init(void)
{
    // 配置LED引脚 - 两组共阳极RGB LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_RED_PIN) | (1ULL << LED1_GREEN_PIN) | (1ULL << LED1_BLUE_PIN) |
                          (1ULL << LED2_RED_PIN) | (1ULL << LED2_GREEN_PIN) | (1ULL << LED2_BLUE_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 配置按键引脚
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // 上升沿触发中断
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << KEY1_PIN) | (1ULL << KEY2_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;  // 启用内部上拉电阻
    gpio_config(&io_conf);

    // 设置LED初始状态 - 共阳极LED，高电平(1)熄灭，低电平(0)点亮
    // LED1组初始状态 - 全部熄灭
    gpio_set_level(LED1_RED_PIN, 1);
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED1_BLUE_PIN, 1);

    // LED2组初始状态 - 全部熄灭
    gpio_set_level(LED2_RED_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    gpio_set_level(LED2_BLUE_PIN, 1);
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

    // 配置CMD_VEL UART
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
}

/**
 * 初始化定时器
 */
static void app_timer_init(void)
{
    // 创建左刹车定时器 (5秒超时)
    brake_timer_left = xTimerCreate("brake_timer_left", pdMS_TO_TICKS(5000), pdFALSE, 0, brake_timer_left_callback);
    if (brake_timer_left == NULL) {
        ESP_LOGE(TAG, "Failed to create left brake timer");
    }

    // 创建右刹车定时器 (5秒超时)
    brake_timer_right = xTimerCreate("brake_timer_right", pdMS_TO_TICKS(5000), pdFALSE, 0, brake_timer_right_callback);
    if (brake_timer_right == NULL) {
        ESP_LOGE(TAG, "Failed to create right brake timer");
    }

    // 启动定时器
    xTimerStart(brake_timer_left, 0);
    xTimerStart(brake_timer_right, 0);
}

void app_main(void)
{
    // ====================================================================
    // 系统初始化 - 增加调试信息
    // ====================================================================

    printf("\n=== ESP32 Control Board Starting ===\n");
    printf("Free heap at start: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

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

    // 初始化定时器
    printf("Initializing timers...\n");
    app_timer_init();
    printf("Timers initialized OK\n");
    printf("Free heap after timers: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

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

    // 创建FreeRTOS队列
    printf("Creating FreeRTOS queues...\n");
    sbus_queue = xQueueCreate(5, sizeof(sbus_data_t));
    cmd_queue = xQueueCreate(5, sizeof(motor_cmd_t));

    if (sbus_queue == NULL || cmd_queue == NULL) {
        printf("ERROR: Failed to create queues!\n");
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }
    printf("Queues created OK\n");
    printf("Free heap after queues: %lu bytes\n", (unsigned long)esp_get_free_heap_size());

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

    // CMD_VEL处理任务已在UART初始化中创建

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
    ESP_LOGI(TAG, "✅ 已启用: SBUS处理、电机控制、CMD_VEL接收、状态监控、Wi-Fi管理");
    ESP_LOGI(TAG, "🚫 已禁用: HTTP服务器、云客户端、数据集成");
#else
    ESP_LOGI(TAG, "All FreeRTOS tasks created (including Wi-Fi and HTTP server)");
#endif
}
