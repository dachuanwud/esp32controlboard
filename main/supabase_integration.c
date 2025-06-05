#include "supabase_integration.h"
#include "cloud_client.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "SUPABASE_INTEGRATION";

// 全局变量
static bool s_integration_running = false;
static TimerHandle_t s_status_timer = NULL;
static TimerHandle_t s_heartbeat_timer = NULL;
static device_status_data_t s_current_status = {0};

// 外部数据获取函数指针
static get_sbus_data_func_t s_get_sbus_data = NULL;
static get_motor_data_func_t s_get_motor_data = NULL;
static get_can_data_func_t s_get_can_data = NULL;

/**
 * 状态定时器回调函数
 */
static void status_timer_callback(TimerHandle_t xTimer)
{
    if (!s_integration_running) {
        return;
    }
    
    // 收集设备状态数据
    collect_device_status(&s_current_status);
    
    // 发送状态到Supabase
    esp_err_t ret = cloud_client_send_device_status(&s_current_status);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ 状态上报失败: %s", cloud_client_get_last_error());
        
        // 检查网络状态，必要时重连
        network_status_t net_status = cloud_client_get_network_status();
        if (net_status == NETWORK_ERROR || net_status == NETWORK_DISCONNECTED) {
            ESP_LOGI(TAG, "🔄 尝试网络重连...");
            cloud_client_reconnect();
        }
    }
}

/**
 * 心跳定时器回调函数
 */
static void heartbeat_timer_callback(TimerHandle_t xTimer)
{
    if (!s_integration_running) {
        return;
    }
    
    // 发送简单的心跳包
    device_status_data_t heartbeat = {0};
    heartbeat.wifi_connected = wifi_manager_is_connected();
    heartbeat.uptime_seconds = esp_timer_get_time() / 1000000;
    heartbeat.free_heap = esp_get_free_heap_size();
    heartbeat.timestamp = heartbeat.uptime_seconds;
    
    if (heartbeat.wifi_connected) {
        strcpy(heartbeat.wifi_ip, wifi_manager_get_ip_address() ?: "0.0.0.0");
        
        wifi_status_t wifi_status;
        if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
            heartbeat.wifi_rssi = wifi_status.rssi;
        }
    }
    
    cloud_client_send_device_status(&heartbeat);
}

/**
 * 收集设备状态数据
 */
esp_err_t collect_device_status(device_status_data_t* status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(status, 0, sizeof(device_status_data_t));
    
    // 基础系统信息
    status->uptime_seconds = esp_timer_get_time() / 1000000;
    status->free_heap = esp_get_free_heap_size();
    status->total_heap = esp_get_minimum_free_heap_size();
    status->task_count = uxTaskGetNumberOfTasks();
    status->timestamp = status->uptime_seconds;
    
    // Wi-Fi状态
    status->wifi_connected = wifi_manager_is_connected();
    if (status->wifi_connected) {
        const char* ip = wifi_manager_get_ip_address();
        if (ip) {
            strncpy(status->wifi_ip, ip, sizeof(status->wifi_ip) - 1);
        }
        
        wifi_status_t wifi_status;
        if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
            status->wifi_rssi = wifi_status.rssi;
        }
    }
    
    // SBUS数据
    if (s_get_sbus_data) {
        sbus_data_t sbus_data;
        if (s_get_sbus_data(&sbus_data) == ESP_OK) {
            status->sbus_connected = true;
            for (int i = 0; i < 16 && i < sizeof(sbus_data.channels)/sizeof(sbus_data.channels[0]); i++) {
                status->sbus_channels[i] = sbus_data.channels[i];
            }
            status->last_sbus_time = sbus_data.timestamp;
        }
    }
    
    // 电机数据
    if (s_get_motor_data) {
        motor_data_t motor_data;
        if (s_get_motor_data(&motor_data) == ESP_OK) {
            status->motor_left_speed = motor_data.left_speed;
            status->motor_right_speed = motor_data.right_speed;
            status->last_cmd_time = motor_data.timestamp;
        }
    }
    
    // CAN数据
    if (s_get_can_data) {
        can_data_t can_data;
        if (s_get_can_data(&can_data) == ESP_OK) {
            status->can_connected = can_data.connected;
            status->can_tx_count = can_data.tx_count;
            status->can_rx_count = can_data.rx_count;
        }
    }
    
    ESP_LOGD(TAG, "📊 设备状态收集完成: heap=%d, uptime=%d, wifi=%s", 
             status->free_heap, status->uptime_seconds, 
             status->wifi_connected ? "connected" : "disconnected");
    
    return ESP_OK;
}

/**
 * 初始化Supabase集成
 */
esp_err_t supabase_integration_init(void)
{
    ESP_LOGI(TAG, "🚀 初始化Supabase集成...");
    
    // 初始化云客户端
    esp_err_t ret = cloud_client_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 云客户端初始化失败");
        return ret;
    }
    
    // 创建状态上报定时器（30秒间隔）
    s_status_timer = xTimerCreate(
        "status_timer",
        pdMS_TO_TICKS(DEVICE_STATUS_INTERVAL_MS),
        pdTRUE,  // 自动重载
        NULL,
        status_timer_callback
    );
    
    if (!s_status_timer) {
        ESP_LOGE(TAG, "❌ 创建状态定时器失败");
        return ESP_FAIL;
    }
    
    // 创建心跳定时器（5分钟间隔）
    s_heartbeat_timer = xTimerCreate(
        "heartbeat_timer",
        pdMS_TO_TICKS(300000),  // 5分钟
        pdTRUE,  // 自动重载
        NULL,
        heartbeat_timer_callback
    );
    
    if (!s_heartbeat_timer) {
        ESP_LOGE(TAG, "❌ 创建心跳定时器失败");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Supabase集成初始化完成");
    return ESP_OK;
}

/**
 * 启动Supabase集成
 */
esp_err_t supabase_integration_start(void)
{
    if (s_integration_running) {
        ESP_LOGW(TAG, "⚠️ Supabase集成已在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🚀 启动Supabase集成...");
    
    // 启动云客户端
    esp_err_t ret = cloud_client_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 启动云客户端失败");
        return ret;
    }
    
    // 启动定时器
    if (xTimerStart(s_status_timer, pdMS_TO_TICKS(1000)) != pdPASS) {
        ESP_LOGE(TAG, "❌ 启动状态定时器失败");
        return ESP_FAIL;
    }
    
    if (xTimerStart(s_heartbeat_timer, pdMS_TO_TICKS(1000)) != pdPASS) {
        ESP_LOGE(TAG, "❌ 启动心跳定时器失败");
        return ESP_FAIL;
    }
    
    s_integration_running = true;
    ESP_LOGI(TAG, "✅ Supabase集成启动成功");
    
    return ESP_OK;
}

/**
 * 停止Supabase集成
 */
esp_err_t supabase_integration_stop(void)
{
    if (!s_integration_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🛑 停止Supabase集成...");
    
    s_integration_running = false;
    
    // 停止定时器
    if (s_status_timer) {
        xTimerStop(s_status_timer, pdMS_TO_TICKS(1000));
    }
    
    if (s_heartbeat_timer) {
        xTimerStop(s_heartbeat_timer, pdMS_TO_TICKS(1000));
    }
    
    // 停止云客户端
    cloud_client_stop();
    
    ESP_LOGI(TAG, "✅ Supabase集成已停止");
    return ESP_OK;
}

/**
 * 设置数据获取回调函数
 */
void supabase_integration_set_callbacks(
    get_sbus_data_func_t sbus_func,
    get_motor_data_func_t motor_func,
    get_can_data_func_t can_func)
{
    s_get_sbus_data = sbus_func;
    s_get_motor_data = motor_func;
    s_get_can_data = can_func;
    
    ESP_LOGI(TAG, "📋 数据获取回调函数已设置");
}

/**
 * 手动触发状态上报
 */
esp_err_t supabase_integration_send_status_now(void)
{
    if (!s_integration_running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    collect_device_status(&s_current_status);
    return cloud_client_send_device_status(&s_current_status);
}

/**
 * 获取集成状态
 */
bool supabase_integration_is_running(void)
{
    return s_integration_running;
}

/**
 * 获取最后的设备状态
 */
const device_status_data_t* supabase_integration_get_last_status(void)
{
    return &s_current_status;
}
