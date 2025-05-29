#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 时间管理器配置
#define TIME_MANAGER_NTP_SERVER1    "pool.ntp.org"
#define TIME_MANAGER_NTP_SERVER2    "time.nist.gov"
#define TIME_MANAGER_NTP_SERVER3    "cn.pool.ntp.org"
#define TIME_MANAGER_TIMEZONE       "CST-8"  // 中国标准时间 UTC+8
#define TIME_MANAGER_SYNC_TIMEOUT   30000    // 30秒同步超时

// 时间同步状态
typedef enum {
    TIME_SYNC_STATUS_RESET = 0,     // 时间未设置
    TIME_SYNC_STATUS_IN_PROGRESS,   // 正在同步
    TIME_SYNC_STATUS_COMPLETED,     // 同步完成
    TIME_SYNC_STATUS_FAILED         // 同步失败
} time_sync_status_t;

// 时间信息结构
typedef struct {
    time_t timestamp;               // Unix时间戳
    struct tm timeinfo;             // 时间结构
    char formatted_time[32];        // 格式化时间字符串
    bool is_valid;                  // 时间是否有效
    time_sync_status_t sync_status; // 同步状态
    uint32_t last_sync_time;        // 上次同步时间(系统滴答)
} time_info_t;

/**
 * @brief 初始化时间管理器
 *
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_init(void);

/**
 * @brief 启动NTP时间同步
 *
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_start_sync(void);

/**
 * @brief 停止NTP时间同步
 *
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_stop_sync(void);

/**
 * @brief 获取当前时间信息
 *
 * @param time_info 时间信息结构指针
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_get_time(time_info_t* time_info);

/**
 * @brief 获取Unix时间戳
 *
 * @return time_t Unix时间戳，0表示时间无效
 */
time_t time_manager_get_timestamp(void);

/**
 * @brief 获取智能时间戳（混合模式）
 *
 * @param use_relative 如果为true，在NTP未同步时使用相对时间
 * @return time_t 时间戳（Unix时间戳或相对时间）
 */
time_t time_manager_get_smart_timestamp(bool use_relative);

/**
 * @brief 获取格式化时间字符串
 *
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_get_formatted_time(char* buffer, size_t buffer_size);

/**
 * @brief 检查时间是否有效
 *
 * @return true 时间有效，false 时间无效
 */
bool time_manager_is_time_valid(void);

/**
 * @brief 获取时间同步状态
 *
 * @return time_sync_status_t 同步状态
 */
time_sync_status_t time_manager_get_sync_status(void);

/**
 * @brief 设置时区
 *
 * @param timezone 时区字符串，如"CST-8"
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_set_timezone(const char* timezone);

/**
 * @brief 强制重新同步时间
 *
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t time_manager_force_sync(void);

/**
 * @brief 获取上次同步时间
 *
 * @return uint32_t 上次同步的系统滴答时间
 */
uint32_t time_manager_get_last_sync_time(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_MANAGER_H
