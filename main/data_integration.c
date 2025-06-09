#include "data_integration.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "DATA_INTEGRATION";

// 全局回调函数指针
static get_sbus_status_func_t s_get_sbus_status = NULL;
static get_motor_status_func_t s_get_motor_status = NULL;
static get_can_status_func_t s_get_can_status = NULL;

/**
 * 初始化数据集成模块
 */
esp_err_t data_integration_init(void)
{
    ESP_LOGI(TAG, "📊 初始化数据集成模块...");
    
    // 清空回调函数
    s_get_sbus_status = NULL;
    s_get_motor_status = NULL;
    s_get_can_status = NULL;
    
    ESP_LOGI(TAG, "✅ 数据集成模块初始化完成");
    return ESP_OK;
}

/**
 * 设置数据获取回调函数
 */
void data_integration_set_callbacks(
    get_sbus_status_func_t sbus_func,
    get_motor_status_func_t motor_func,
    get_can_status_func_t can_func)
{
    s_get_sbus_status = sbus_func;
    s_get_motor_status = motor_func;
    s_get_can_status = can_func;
    
    ESP_LOGI(TAG, "📋 数据获取回调函数已设置: SBUS=%s, Motor=%s, CAN=%s",
             sbus_func ? "✅" : "❌",
             motor_func ? "✅" : "❌",
             can_func ? "✅" : "❌");
}

/**
 * 收集完整的设备状态数据
 */
esp_err_t data_integration_collect_status(device_status_data_t* status)
{
    if (!status) {
        ESP_LOGE(TAG, "❌ 状态数据指针为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGV(TAG, "📊 开始收集设备状态数据...");
    memset(status, 0, sizeof(device_status_data_t));

    // 基础系统信息
    status->uptime_seconds = esp_timer_get_time() / 1000000;
    status->free_heap = esp_get_free_heap_size();
    status->total_heap = esp_get_minimum_free_heap_size();
    status->task_count = uxTaskGetNumberOfTasks();
    status->timestamp = status->uptime_seconds;

    ESP_LOGV(TAG, "💾 系统信息 - 堆内存: %" PRIu32 "/%" PRIu32 ", 运行时间: %" PRIu32 "s, 任务数: %d",
             status->free_heap, status->total_heap, status->uptime_seconds, status->task_count);

    // Wi-Fi状态
    status->wifi_connected = wifi_manager_is_connected();
    if (status->wifi_connected) {
        const char* ip = wifi_manager_get_ip_address();
        if (ip) {
            strncpy(status->wifi_ip, ip, sizeof(status->wifi_ip) - 1);
            ESP_LOGV(TAG, "🌐 Wi-Fi IP: %s", status->wifi_ip);
        }

        wifi_status_t wifi_status;
        if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
            status->wifi_rssi = wifi_status.rssi;
            ESP_LOGV(TAG, "📶 Wi-Fi信号强度: %d dBm", status->wifi_rssi);
        }
    } else {
        ESP_LOGV(TAG, "📡 Wi-Fi未连接");
    }

    // SBUS数据
    if (s_get_sbus_status) {
        uint16_t channels[16];
        esp_err_t ret = s_get_sbus_status(&status->sbus_connected, channels, &status->last_sbus_time);
        if (ret == ESP_OK) {
            if (status->sbus_connected) {
                for (int i = 0; i < 16; i++) {
                    status->sbus_channels[i] = channels[i];
                }
                ESP_LOGV(TAG, "🎮 SBUS已连接，最后更新: %" PRIu32, status->last_sbus_time);
                ESP_LOGV(TAG, "📊 SBUS通道示例 - CH1: %d, CH2: %d, CH3: %d, CH4: %d",
                         channels[0], channels[1], channels[2], channels[3]);
            } else {
                ESP_LOGV(TAG, "🎮 SBUS未连接");
            }
        } else {
            ESP_LOGW(TAG, "⚠️ 获取SBUS状态失败");
        }
    } else {
        // 默认值
        status->sbus_connected = false;
        for (int i = 0; i < 16; i++) {
            status->sbus_channels[i] = 1500;  // 中位值
        }
        status->last_sbus_time = 0;
        ESP_LOGV(TAG, "🎮 SBUS回调未设置，使用默认值");
    }

    // 电机数据
    if (s_get_motor_status) {
        esp_err_t ret = s_get_motor_status(&status->motor_left_speed, &status->motor_right_speed, &status->last_cmd_time);
        if (ret == ESP_OK) {
            ESP_LOGV(TAG, "🚗 电机状态 - 左: %d, 右: %d, 最后更新: %" PRIu32,
                     status->motor_left_speed, status->motor_right_speed, status->last_cmd_time);
        } else {
            ESP_LOGW(TAG, "⚠️ 获取电机状态失败");
        }
    } else {
        status->motor_left_speed = 0;
        status->motor_right_speed = 0;
        status->last_cmd_time = 0;
        ESP_LOGV(TAG, "🚗 电机回调未设置，使用默认值");
    }

    // CAN数据
    if (s_get_can_status) {
        esp_err_t ret = s_get_can_status(&status->can_connected, &status->can_tx_count, &status->can_rx_count);
        if (ret == ESP_OK) {
            ESP_LOGV(TAG, "🚌 CAN状态 - 连接: %s, TX: %" PRIu32 ", RX: %" PRIu32,
                     status->can_connected ? "是" : "否",
                     status->can_tx_count, status->can_rx_count);
        } else {
            ESP_LOGW(TAG, "⚠️ 获取CAN状态失败");
        }
    } else {
        status->can_connected = false;
        status->can_tx_count = 0;
        status->can_rx_count = 0;
        ESP_LOGV(TAG, "🚌 CAN回调未设置，使用默认值");
    }

    ESP_LOGD(TAG, "✅ 设备状态收集完成");
    ESP_LOGD(TAG, "📊 状态摘要 - 堆内存: %" PRIu32 ", 运行时间: %" PRIu32 "s, WiFi: %s, SBUS: %s, CAN: %s",
             status->free_heap, status->uptime_seconds,
             status->wifi_connected ? "✅" : "❌",
             status->sbus_connected ? "✅" : "❌",
             status->can_connected ? "✅" : "❌");

    return ESP_OK;
}

/**
 * 获取SBUS状态（内部使用）
 */
esp_err_t data_integration_get_sbus_status(bool* connected, uint16_t* channels, uint32_t* last_time)
{
    if (!connected || !channels || !last_time) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_get_sbus_status) {
        return s_get_sbus_status(connected, channels, last_time);
    }
    
    // 默认值
    *connected = false;
    for (int i = 0; i < 16; i++) {
        channels[i] = 1500;
    }
    *last_time = 0;
    
    return ESP_OK;
}

/**
 * 获取电机状态（内部使用）
 */
esp_err_t data_integration_get_motor_status(int* left_speed, int* right_speed, uint32_t* last_time)
{
    if (!left_speed || !right_speed || !last_time) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_get_motor_status) {
        return s_get_motor_status(left_speed, right_speed, last_time);
    }
    
    // 默认值
    *left_speed = 0;
    *right_speed = 0;
    *last_time = 0;
    
    return ESP_OK;
}

/**
 * 获取CAN状态（内部使用）
 */
esp_err_t data_integration_get_can_status(bool* connected, uint32_t* tx_count, uint32_t* rx_count)
{
    if (!connected || !tx_count || !rx_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_get_can_status) {
        return s_get_can_status(connected, tx_count, rx_count);
    }
    
    // 默认值
    *connected = false;
    *tx_count = 0;
    *rx_count = 0;
    
    return ESP_OK;
}
