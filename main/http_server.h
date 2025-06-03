#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

// HTTP服务器配置
#define HTTP_SERVER_PORT        80
#define HTTP_MAX_URI_LEN        128
#define HTTP_MAX_RESP_LEN       4096
#define HTTP_UPLOAD_CHUNK_SIZE  4096

// API端点定义
#define API_DEVICE_INFO         "/api/device/info"
#define API_DEVICE_STATUS       "/api/device/status"
#define API_DEVICE_HEALTH       "/api/device/health"
#define API_DEVICE_UPTIME       "/api/device/uptime"
#define API_OTA_UPLOAD          "/api/ota/upload"
#define API_OTA_START           "/api/ota/start"
#define API_OTA_PROGRESS        "/api/ota/progress"
#define API_OTA_ROLLBACK        "/api/ota/rollback"
#define API_WIFI_SCAN           "/api/wifi/scan"
#define API_WIFI_CONNECT        "/api/wifi/connect"
#define API_WIFI_STATUS         "/api/wifi/status"

// 设备信息结构体
typedef struct {
    char device_name[32];
    char firmware_version[16];
    char hardware_version[16];
    char chip_model[16];
    uint32_t flash_size;
    uint32_t free_heap;
    uint32_t uptime_seconds;
    char mac_address[18];
} device_info_t;

// 设备状态结构体
typedef struct {
    bool sbus_connected;
    bool can_connected;
    bool wifi_connected;
    char wifi_ip[16];
    int8_t wifi_rssi;
    uint16_t sbus_channels[16];
    int8_t motor_left_speed;
    int8_t motor_right_speed;
    uint32_t last_sbus_time;
    uint32_t last_cmd_time;
} device_status_t;

// 系统健康检查结构体
typedef struct {
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint8_t cpu_usage_percent;
    float cpu_temperature;
    bool watchdog_triggered;
    uint32_t task_count;
    bool wifi_healthy;
    bool sbus_healthy;
    bool motor_healthy;
} system_health_t;

// OTA进度结构体
typedef struct {
    bool in_progress;
    uint32_t total_size;
    uint32_t written_size;
    uint8_t progress_percent;
    char status_message[64];
    bool success;
    char error_message[128];
} ota_progress_t;

/**
 * 初始化HTTP服务器
 * @return ESP_OK=成功
 */
esp_err_t http_server_init(void);

/**
 * 启动HTTP服务器
 * @return ESP_OK=成功
 */
esp_err_t http_server_start(void);

/**
 * 停止HTTP服务器
 * @return ESP_OK=成功
 */
esp_err_t http_server_stop(void);

/**
 * 检查HTTP服务器是否运行
 * @return true=运行中，false=已停止
 */
bool http_server_is_running(void);

/**
 * 获取设备信息
 * @param info 输出设备信息
 * @return ESP_OK=成功
 */
esp_err_t http_server_get_device_info(device_info_t* info);

/**
 * 获取设备状态
 * @param status 输出设备状态
 * @return ESP_OK=成功
 */
esp_err_t http_server_get_device_status(device_status_t* status);

/**
 * 获取系统健康状态
 * @param health 输出系统健康状态
 * @return ESP_OK=成功
 */
esp_err_t http_server_get_system_health(system_health_t* health);

/**
 * 获取OTA进度
 * @param progress 输出OTA进度
 * @return ESP_OK=成功
 */
esp_err_t http_server_get_ota_progress(ota_progress_t* progress);

/**
 * 设置SBUS状态回调函数
 * @param callback 回调函数指针
 */
void http_server_set_sbus_callback(bool (*callback)(uint16_t* channels));

/**
 * 设置电机状态回调函数
 * @param callback 回调函数指针
 */
void http_server_set_motor_callback(bool (*callback)(int8_t* left, int8_t* right));

#endif /* HTTP_SERVER_H */
