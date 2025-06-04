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

// 设备信息结构体
typedef struct {
    char device_id[64];
    char device_name[128];
    char local_ip[16];
    char device_type[32];
    cloud_status_t status;
    uint32_t last_seen;
} device_info_t;

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
const device_info_t* cloud_client_get_device_info(void);

#ifdef __cplusplus
}
#endif

#endif // CLOUD_CLIENT_H
