#ifndef SUPABASE_INTEGRATION_H
#define SUPABASE_INTEGRATION_H

#include "esp_err.h"
#include "cloud_client.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SBUS数据结构
typedef struct {
    uint16_t channels[16];
    bool failsafe;
    bool frame_lost;
    uint32_t timestamp;
} sbus_data_t;

// 电机数据结构
typedef struct {
    int left_speed;
    int right_speed;
    uint32_t timestamp;
} motor_data_t;

// CAN数据结构
typedef struct {
    bool connected;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint32_t timestamp;
} can_data_t;

// 数据获取回调函数类型定义
typedef esp_err_t (*get_sbus_data_func_t)(sbus_data_t* data);
typedef esp_err_t (*get_motor_data_func_t)(motor_data_t* data);
typedef esp_err_t (*get_can_data_func_t)(can_data_t* data);

/**
 * 初始化Supabase集成
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_init(void);

/**
 * 启动Supabase集成
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_start(void);

/**
 * 停止Supabase集成
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_stop(void);

/**
 * 设置数据获取回调函数
 * @param sbus_func SBUS数据获取函数
 * @param motor_func 电机数据获取函数
 * @param can_func CAN数据获取函数
 */
void supabase_integration_set_callbacks(
    get_sbus_data_func_t sbus_func,
    get_motor_data_func_t motor_func,
    get_can_data_func_t can_func);

/**
 * 收集设备状态数据
 * @param status 状态数据结构指针
 * @return ESP_OK=成功
 */
esp_err_t collect_device_status(device_status_data_t* status);

/**
 * 手动触发状态上报
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_send_status_now(void);

/**
 * 获取集成状态
 * @return true=运行中，false=已停止
 */
bool supabase_integration_is_running(void);

/**
 * 获取最后的设备状态
 * @return 设备状态数据指针
 */
const device_status_data_t* supabase_integration_get_last_status(void);

/**
 * 设置设备认证密钥
 * @param device_key 设备密钥
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_set_auth_key(const char* device_key);

/**
 * 获取网络连接状态
 * @return 网络状态
 */
network_status_t supabase_integration_get_network_status(void);

/**
 * 获取最后错误信息
 * @return 错误信息字符串
 */
const char* supabase_integration_get_last_error(void);

/**
 * 执行手动重连
 * @return ESP_OK=成功
 */
esp_err_t supabase_integration_reconnect(void);

#ifdef __cplusplus
}
#endif

#endif // SUPABASE_INTEGRATION_H
