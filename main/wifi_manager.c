#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

// Wi-Fi事件组
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Wi-Fi状态变量
static wifi_status_t s_wifi_status = {0};
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

/**
 * Wi-Fi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "📡 Wi-Fi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_status.retry_count < WIFI_RETRY_MAX) {
            esp_wifi_connect();
            s_wifi_status.retry_count++;
            s_wifi_status.state = WIFI_STATE_CONNECTING;
            ESP_LOGI(TAG, "🔄 Retry connecting to Wi-Fi (%d/%d)",
                    s_wifi_status.retry_count, WIFI_RETRY_MAX);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_status.state = WIFI_STATE_FAILED;
            ESP_LOGE(TAG, "❌ Failed to connect to Wi-Fi after %d retries", WIFI_RETRY_MAX);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(s_wifi_status.ip_address, sizeof(s_wifi_status.ip_address),
                IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_status.state = WIFI_STATE_CONNECTED;
        s_wifi_status.retry_count = 0;
        s_wifi_status.connect_time = xTaskGetTickCount();
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "✅ Connected to Wi-Fi, IP: %s", s_wifi_status.ip_address);
    }
}

/**
 * 初始化Wi-Fi管理器
 */
esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "🚀 Initializing Wi-Fi Manager...");

    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create Wi-Fi event group");
        return ESP_FAIL;
    }

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建默认Wi-Fi STA
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create default Wi-Fi STA");
        return ESP_FAIL;
    }

    // 初始化Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // 设置Wi-Fi模式为STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 初始化状态
    s_wifi_status.state = WIFI_STATE_DISCONNECTED;
    s_wifi_status.retry_count = 0;
    memset(s_wifi_status.ip_address, 0, sizeof(s_wifi_status.ip_address));

    ESP_LOGI(TAG, "✅ Wi-Fi Manager initialized successfully");
    return ESP_OK;
}

/**
 * 连接到Wi-Fi网络
 */
esp_err_t wifi_manager_connect(const char* ssid, const char* password)
{
    if (ssid == NULL) {
        ESP_LOGE(TAG, "❌ SSID cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "🔗 Connecting to Wi-Fi: %s", ssid);

    // 配置Wi-Fi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 重置状态
    s_wifi_status.state = WIFI_STATE_CONNECTING;
    s_wifi_status.retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // 开始连接
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start Wi-Fi connection: %s", esp_err_to_name(ret));
        s_wifi_status.state = WIFI_STATE_FAILED;
        return ret;
    }

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Connected to Wi-Fi: %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ Failed to connect to Wi-Fi: %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "⏰ Wi-Fi connection timeout");
        s_wifi_status.state = WIFI_STATE_FAILED;
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * 断开Wi-Fi连接
 */
esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "🔌 Disconnecting from Wi-Fi...");

    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        memset(s_wifi_status.ip_address, 0, sizeof(s_wifi_status.ip_address));
        ESP_LOGI(TAG, "✅ Wi-Fi disconnected");
    } else {
        ESP_LOGE(TAG, "❌ Failed to disconnect Wi-Fi: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * 获取Wi-Fi状态
 */
esp_err_t wifi_manager_get_status(wifi_status_t* status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_wifi_status, sizeof(wifi_status_t));

    // 如果已连接，获取RSSI
    if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status->rssi = ap_info.rssi;
        }
    }

    return ESP_OK;
}

/**
 * 检查Wi-Fi是否已连接
 */
bool wifi_manager_is_connected(void)
{
    return (s_wifi_status.state == WIFI_STATE_CONNECTED);
}

/**
 * 获取IP地址字符串
 */
const char* wifi_manager_get_ip_address(void)
{
    if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
        return s_wifi_status.ip_address;
    }
    return NULL;
}
