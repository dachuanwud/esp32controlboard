#ifndef DATA_INTEGRATION_H
#define DATA_INTEGRATION_H

#include "esp_err.h"
#include "cloud_client.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 数据获取函数指针类型
typedef esp_err_t (*get_sbus_status_func_t)(bool* connected, uint16_t* channels, uint32_t* last_time);
typedef esp_err_t (*get_motor_status_func_t)(int* left_speed, int* right_speed, uint32_t* last_time);
typedef esp_err_t (*get_can_status_func_t)(bool* connected, uint32_t* tx_count, uint32_t* rx_count);

/**
 * 初始化数据集成模块
 * @return ESP_OK=成功
 */
esp_err_t data_integration_init(void);

/**
 * 设置数据获取回调函数
 * @param sbus_func SBUS状态获取函数
 * @param motor_func 电机状态获取函数
 * @param can_func CAN状态获取函数
 */
void data_integration_set_callbacks(
    get_sbus_status_func_t sbus_func,
    get_motor_status_func_t motor_func,
    get_can_status_func_t can_func);

/**
 * 收集完整的设备状态数据
 * @param status 设备状态数据结构指针
 * @return ESP_OK=成功
 */
esp_err_t data_integration_collect_status(device_status_data_t* status);

/**
 * 获取SBUS状态（内部使用）
 * @param connected 连接状态
 * @param channels 通道数据数组
 * @param last_time 最后更新时间
 * @return ESP_OK=成功
 */
esp_err_t data_integration_get_sbus_status(bool* connected, uint16_t* channels, uint32_t* last_time);

/**
 * 获取电机状态（内部使用）
 * @param left_speed 左电机速度
 * @param right_speed 右电机速度
 * @param last_time 最后更新时间
 * @return ESP_OK=成功
 */
esp_err_t data_integration_get_motor_status(int* left_speed, int* right_speed, uint32_t* last_time);

/**
 * 获取CAN状态（内部使用）
 * @param connected 连接状态
 * @param tx_count 发送计数
 * @param rx_count 接收计数
 * @return ESP_OK=成功
 */
esp_err_t data_integration_get_can_status(bool* connected, uint32_t* tx_count, uint32_t* rx_count);

#ifdef __cplusplus
}
#endif

#endif // DATA_INTEGRATION_H
