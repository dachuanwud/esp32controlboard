#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"

// Wi-Fi配置参数
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASSWORD_MAX_LEN   64
#define WIFI_RETRY_MAX          5
#define WIFI_CONNECT_TIMEOUT_MS 10000

// Wi-Fi状态枚举
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// Wi-Fi配置结构体
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    bool auto_connect;
    uint8_t retry_count;
} my_wifi_config_t;

// Wi-Fi状态信息结构体
typedef struct {
    wifi_state_t state;
    char ip_address[16];
    int8_t rssi;
    uint8_t retry_count;
    uint32_t connect_time;
} wifi_status_t;

/**
 * 初始化Wi-Fi管理器
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_init(void);

/**
 * 连接到Wi-Fi网络
 * @param ssid Wi-Fi网络名称
 * @param password Wi-Fi密码
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_connect(const char* ssid, const char* password);

/**
 * 断开Wi-Fi连接
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * 获取Wi-Fi状态
 * @param status 输出状态信息
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_get_status(wifi_status_t* status);

/**
 * 检查Wi-Fi是否已连接
 * @return true=已连接，false=未连接
 */
bool wifi_manager_is_connected(void);

/**
 * 获取IP地址字符串
 * @return IP地址字符串指针
 */
const char* wifi_manager_get_ip_address(void);

/**
 * 获取Wi-Fi信号强度(RSSI)
 * @return RSSI值(dBm)，如果未连接返回0
 */
int8_t wifi_manager_get_rssi(void);

/**
 * 启动Wi-Fi AP模式（用于配置）
 * @param ssid AP网络名称
 * @param password AP密码
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_start_ap(const char* ssid, const char* password);

/**
 * 停止Wi-Fi AP模式
 * @return ESP_OK=成功
 */
esp_err_t wifi_manager_stop_ap(void);

/**
 * 扫描可用的Wi-Fi网络
 * @param scan_results 扫描结果数组
 * @param max_results 最大结果数量
 * @return 实际扫描到的网络数量
 */
uint16_t wifi_manager_scan_networks(wifi_ap_record_t* scan_results, uint16_t max_results);

#endif /* WIFI_MANAGER_H */
