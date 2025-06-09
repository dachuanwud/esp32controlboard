#include "log_config.h"
#include "wifi_manager.h"
#include "cloud_client.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "LOG_CONFIG";

/**
 * 日志配置函数
 * 设置各个模块的日志级别
 */
void configure_logging(void)
{
    ESP_LOGI(TAG, "🔧 配置日志系统...");
    
    // 设置默认日志级别
    esp_log_level_set("*", ESP_LOG_INFO);

    // 核心模块适中日志级别
    esp_log_level_set("CLOUD_CLIENT", ESP_LOG_INFO);
    esp_log_level_set("DATA_INTEGRATION", ESP_LOG_WARN);  // 降低数据集成模块日志级别
    esp_log_level_set("WIFI_MANAGER", ESP_LOG_INFO);
    esp_log_level_set("HTTP_SERVER", ESP_LOG_INFO);
    esp_log_level_set("MAIN", ESP_LOG_INFO);

    // 控制模块优化日志级别
    esp_log_level_set("SBUS", ESP_LOG_INFO);        // 提升SBUS日志级别，便于调试
    esp_log_level_set("DRV_KEYA", ESP_LOG_WARN);    // 降低CAN驱动日志级别
    esp_log_level_set("CHAN_PARSE", ESP_LOG_INFO);  // 保持通道解析日志级别
    esp_log_level_set("OTA", ESP_LOG_INFO);
    
    // 减少噪音日志
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("tcpip_adapter", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_WARN);
    esp_log_level_set("httpd_uri", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    
    ESP_LOGI(TAG, "✅ 日志系统配置完成");
}

/**
 * 启用详细调试日志
 * 用于开发和调试阶段
 */
void enable_debug_logging(void)
{
    ESP_LOGI(TAG, "🔍 启用详细调试日志...");
    
    // 核心模块调试级别
    esp_log_level_set("CLOUD_CLIENT", ESP_LOG_DEBUG);
    esp_log_level_set("DATA_INTEGRATION", ESP_LOG_DEBUG);
    esp_log_level_set("WIFI_MANAGER", ESP_LOG_DEBUG);
    esp_log_level_set("HTTP_SERVER", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN", ESP_LOG_DEBUG);

    // 控制模块调试级别
    esp_log_level_set("SBUS", ESP_LOG_DEBUG);
    esp_log_level_set("DRV_KEYA", ESP_LOG_DEBUG);
    esp_log_level_set("CHAN_PARSE", ESP_LOG_DEBUG);
    
    ESP_LOGI(TAG, "✅ 调试日志已启用");
}

/**
 * 启用生产环境日志
 * 减少日志输出，提高性能
 */
void enable_production_logging(void)
{
    ESP_LOGI(TAG, "🏭 启用生产环境日志...");
    
    // 设置生产级别日志
    esp_log_level_set("*", ESP_LOG_WARN);
    
    // 重要模块保持INFO级别
    esp_log_level_set("CLOUD_CLIENT", ESP_LOG_INFO);
    esp_log_level_set("WIFI_MANAGER", ESP_LOG_INFO);
    esp_log_level_set("MAIN", ESP_LOG_INFO);
    esp_log_level_set("OTA", ESP_LOG_INFO);
    
    // 其他模块只显示警告和错误
    esp_log_level_set("DATA_INTEGRATION", ESP_LOG_ERROR);
    esp_log_level_set("HTTP_SERVER", ESP_LOG_WARN);
    esp_log_level_set("SBUS", ESP_LOG_WARN);        // 保持SBUS警告级别，便于故障诊断
    esp_log_level_set("DRV_KEYA", ESP_LOG_ERROR);   // CAN驱动只显示错误
    esp_log_level_set("CHAN_PARSE", ESP_LOG_WARN);  // 通道解析保持警告级别
    
    ESP_LOGI(TAG, "✅ 生产环境日志已启用");
}

/**
 * 打印系统信息
 * 显示设备基本信息和配置
 */
void print_system_info(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                    ESP32 控制板系统信息                        ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════╣");
    
    // 应用信息
    const esp_app_desc_t* app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "║ 📱 应用名称: %-45s ║", app_desc->project_name);
    ESP_LOGI(TAG, "║ 🔢 应用版本: %-45s ║", app_desc->version);
    ESP_LOGI(TAG, "║ 📅 编译时间: %-45s ║", app_desc->time);
    ESP_LOGI(TAG, "║ 📅 编译日期: %-45s ║", app_desc->date);
    
    // 硬件信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "║ 💾 芯片型号: ESP32 (Rev %d)                                  ║", chip_info.revision);
    ESP_LOGI(TAG, "║ 🔧 CPU核心数: %-44d ║", chip_info.cores);
    ESP_LOGI(TAG, "║ 📡 Wi-Fi支持: %-44s ║", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "是" : "否");
    ESP_LOGI(TAG, "║ 📶 蓝牙支持: %-45s ║", (chip_info.features & CHIP_FEATURE_BT) ? "是" : "否");
    
    // 内存信息
    ESP_LOGI(TAG, "║ 💾 可用堆内存: %-41" PRIu32 " ║", esp_get_free_heap_size());
    ESP_LOGI(TAG, "║ 💾 最小堆内存: %-41" PRIu32 " ║", esp_get_minimum_free_heap_size());
    
    // MAC地址
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "║ 🔗 MAC地址: %02x:%02x:%02x:%02x:%02x:%02x                              ║", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // 运行时间
    uint64_t uptime = esp_timer_get_time() / 1000000;
    ESP_LOGI(TAG, "║ ⏰ 运行时间: %-44lld秒 ║", uptime);
    
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}

/**
 * 打印网络状态信息
 */
void print_network_status(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                        网络状态信息                           ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════╣");
    
    bool wifi_connected = wifi_manager_is_connected();
    ESP_LOGI(TAG, "║ 📡 Wi-Fi状态: %-44s ║", wifi_connected ? "已连接" : "未连接");
    
    if (wifi_connected) {
        const char* ip = wifi_manager_get_ip_address();
        if (ip) {
            ESP_LOGI(TAG, "║ 🌐 IP地址: %-47s ║", ip);
        }
        
        wifi_status_t wifi_status;
        if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
            ESP_LOGI(TAG, "║ 📶 信号强度: %-43d dBm ║", wifi_status.rssi);
            ESP_LOGI(TAG, "║ ⏰ 连接时间: %-44" PRIu32 "ms ║", wifi_status.connect_time);
        }
    }
    
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}

/**
 * 打印云服务状态信息
 */
void print_cloud_status(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                        云服务状态信息                         ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════╣");
    
    const cloud_device_info_t* device_info = cloud_client_get_device_info();
    if (device_info) {
        ESP_LOGI(TAG, "║ 🆔 设备ID: %-47s ║", device_info->device_id);
        ESP_LOGI(TAG, "║ 📋 设备名称: %-45s ║", device_info->device_name);
        ESP_LOGI(TAG, "║ 🔧 设备类型: %-45s ║", device_info->device_type);
        ESP_LOGI(TAG, "║ 📦 固件版本: %-45s ║", device_info->firmware_version);
        ESP_LOGI(TAG, "║ 🔩 硬件版本: %-45s ║", device_info->hardware_version);
        
        const char* status_str = "未知";
        switch (device_info->status) {
            case CLOUD_STATUS_OFFLINE: status_str = "离线"; break;
            case CLOUD_STATUS_ONLINE: status_str = "在线"; break;
            case CLOUD_STATUS_ERROR: status_str = "错误"; break;
        }
        ESP_LOGI(TAG, "║ 📊 云端状态: %-45s ║", status_str);
        
        network_status_t net_status = cloud_client_get_network_status();
        const char* net_status_str = "未知";
        switch (net_status) {
            case NETWORK_DISCONNECTED: net_status_str = "未连接"; break;
            case NETWORK_CONNECTING: net_status_str = "连接中"; break;
            case NETWORK_CONNECTED: net_status_str = "已连接"; break;
            case NETWORK_ERROR: net_status_str = "错误"; break;
        }
        ESP_LOGI(TAG, "║ 🌐 网络状态: %-45s ║", net_status_str);
        
        if (device_info->last_seen > 0) {
            ESP_LOGI(TAG, "║ ⏰ 最后上报: %-44" PRIu32 "秒前 ║", device_info->last_seen);
        }
    }
    
    const char* last_error = cloud_client_get_last_error();
    if (strlen(last_error) > 0) {
        ESP_LOGI(TAG, "║ ❌ 最后错误: %-45s ║", last_error);
    }
    
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}
