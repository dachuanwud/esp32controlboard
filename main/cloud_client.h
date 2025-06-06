#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 云服务器配置
#define CLOUD_SERVER_HOST "www.nagaflow.top"
#define CLOUD_SERVER_PORT 80
#define CLOUD_SERVER_URL "http://www.nagaflow.top"
#define DEVICE_STATUS_INTERVAL_MS 30000  // 30秒上报一次状态
#define COMMAND_POLL_INTERVAL_MS 10000   // 10秒轮询一次指令

// Supabase集成配置
#define SUPABASE_PROJECT_URL "https://hfmifzmuwcmtgyjfhxvx.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhmbWlmem11d2NtdGd5amZoeHZ4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDkwMjIzNTEsImV4cCI6MjA2NDU5ODM1MX0.YPTUXgVdb8YMwwUWmG4nGdGIOvnTe6zvavMieL-RlTE"
#define MAX_HTTP_RESPONSE_SIZE 4096
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_DELAY_MS 5000
#define MAX_COMMANDS_PER_REQUEST 10

// 设备状态枚举
typedef enum {
    CLOUD_STATUS_OFFLINE = 0,
    CLOUD_STATUS_ONLINE,
    CLOUD_STATUS_ERROR
} cloud_status_t;

// 指令类型枚举
typedef enum {
    CLOUD_CMD_UNKNOWN = 0,
    CLOUD_CMD_SBUS_UPDATE,
    CLOUD_CMD_MOTOR_CONTROL,
    CLOUD_CMD_WIFI_CONFIG,
    CLOUD_CMD_OTA_UPDATE,
    CLOUD_CMD_REBOOT
} cloud_command_type_t;

// 指令结构体
typedef struct {
    uint32_t id;
    cloud_command_type_t command;
    char data[512];
    uint32_t timestamp;
} cloud_command_t;

// 网络连接状态
typedef enum {
    NETWORK_DISCONNECTED = 0,
    NETWORK_CONNECTING,
    NETWORK_CONNECTED,
    NETWORK_ERROR
} network_status_t;

// 云设备信息结构体
typedef struct {
    char device_id[64];
    char device_name[128];
    char local_ip[16];
    char device_type[32];
    char firmware_version[32];
    char hardware_version[32];
    char mac_address[18];
    cloud_status_t status;
    network_status_t network_status;
    uint32_t last_seen;
    uint32_t retry_count;
} cloud_device_info_t;

// 设备状态数据结构体（与Supabase兼容）
typedef struct {
    bool sbus_connected;
    bool can_connected;
    bool wifi_connected;
    char wifi_ip[16];
    int wifi_rssi;
    uint32_t free_heap;
    uint32_t total_heap;
    uint32_t uptime_seconds;
    int task_count;
    uint32_t can_tx_count;
    uint32_t can_rx_count;
    int sbus_channels[16];  // 支持更多通道
    int motor_left_speed;
    int motor_right_speed;
    uint32_t last_sbus_time;
    uint32_t last_cmd_time;
    uint32_t timestamp;
} device_status_data_t;

/**
 * 初始化云客户端
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_init(void);

/**
 * 启动云客户端
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_start(void);

/**
 * 停止云客户端
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_stop(void);

/**
 * 注册设备到云服务器
 * @param device_id 设备ID
 * @param device_name 设备名称
 * @param local_ip 本地IP地址
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_register_device(const char* device_id, const char* device_name, const char* local_ip);

/**
 * 向云服务器发送状态更新
 * @param status 设备状态
 * @param data 状态数据（JSON格式）
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_send_status(cloud_status_t status, const char* data);

/**
 * 从云服务器获取指令
 * @param commands 指令数组
 * @param max_commands 最大指令数量
 * @return 实际获取的指令数量，-1表示错误
 */
int cloud_client_get_commands(cloud_command_t* commands, int max_commands);

/**
 * 设置指令处理回调函数
 * @param callback 回调函数指针
 */
void cloud_client_set_command_callback(void (*callback)(const cloud_command_t* command));

/**
 * 检查云客户端是否已连接
 * @return true=已连接，false=未连接
 */
bool cloud_client_is_connected(void);

/**
 * 获取设备信息
 * @return 设备信息指针
 */
const cloud_device_info_t* cloud_client_get_device_info(void);

/**
 * 发送完整设备状态到Supabase
 * @param status_data 设备状态数据
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_send_device_status(const device_status_data_t* status_data);

/**
 * 获取网络连接状态
 * @return 网络状态
 */
network_status_t cloud_client_get_network_status(void);

/**
 * 设置设备认证信息
 * @param device_key 设备密钥
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_set_auth(const char* device_key);

/**
 * 执行网络重连
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_reconnect(void);

/**
 * 获取最后错误信息
 * @return 错误信息字符串
 */
const char* cloud_client_get_last_error(void);

/**
 * 设置状态更新回调函数
 * @param callback 状态更新回调
 */
void cloud_client_set_status_callback(void (*callback)(const device_status_data_t* status));

/**
 * 注销设备从云服务器
 * @param reason 注销原因
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_unregister_device(const char* reason);

/**
 * 优雅关闭云客户端 (包含设备注销)
 * @param reason 关闭原因
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_graceful_shutdown(const char* reason);

/**
 * 发送OTA进度到云端
 * @param command_id 指令ID
 * @param progress 进度百分比 (0-100)
 * @param message 状态消息
 * @return ESP_OK=成功
 */
esp_err_t cloud_client_send_ota_progress(const char* command_id, uint8_t progress, const char* message);

#ifdef __cplusplus
}
#endif

#endif // CLOUD_CLIENT_H
