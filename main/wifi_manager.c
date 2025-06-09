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
// static esp_netif_t *s_ap_netif = NULL;  // 暂时未使用，AP模式功能待实现

// 添加连接状态标志，防止重复连接
static bool s_connecting_in_progress = false;

/**
 * Wi-Fi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // 不在这里自动连接，等待显式调用wifi_manager_connect
        ESP_LOGI(TAG, "📡 Wi-Fi station started, ready for connection");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "🔌 Wi-Fi disconnected, reason: %d", disconnected->reason);

        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        s_connecting_in_progress = false;

        // 只有在重试次数未超限且不是手动断开的情况下才重试
        if (s_wifi_status.retry_count < WIFI_RETRY_MAX &&
            disconnected->reason != WIFI_REASON_ASSOC_LEAVE) {

            ESP_LOGI(TAG, "🔄 Retry connecting to Wi-Fi (%d/%d)",
                    s_wifi_status.retry_count + 1, WIFI_RETRY_MAX);

            // 延迟重连，避免立即重试
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (!s_connecting_in_progress) {
                s_connecting_in_progress = true;
                s_wifi_status.retry_count++;
                s_wifi_status.state = WIFI_STATE_CONNECTING;

                esp_err_t ret = esp_wifi_connect();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "❌ Retry connection failed: %s", esp_err_to_name(ret));
                    s_connecting_in_progress = false;
                    if (s_wifi_status.retry_count >= WIFI_RETRY_MAX) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                        s_wifi_status.state = WIFI_STATE_FAILED;
                    }
                }
            }
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
        s_connecting_in_progress = false;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "✅ Connected to Wi-Fi, IP: %s", s_wifi_status.ip_address);
    }
}

/**
 * 检查Wi-Fi是否准备就绪
 */
static bool wifi_is_ready(void)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }
    return (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA);
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
        ESP_LOGW(TAG, "⚠️ NVS partition was truncated and will be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ NVS initialized successfully");

    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create Wi-Fi event group");
        return ESP_FAIL;
    }

    // 初始化网络接口
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "❌ Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // 创建默认Wi-Fi STA
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create default Wi-Fi STA");
        return ESP_FAIL;
    }

    // 初始化Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册事件处理器
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_event_handler,
                                             NULL,
                                             NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to register Wi-Fi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                             IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler,
                                             NULL,
                                             NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置Wi-Fi模式为STA
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set Wi-Fi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动Wi-Fi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待Wi-Fi启动完成
    int wait_count = 0;
    while (!wifi_is_ready() && wait_count < 50) { // 最多等待5秒
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }

    if (!wifi_is_ready()) {
        ESP_LOGE(TAG, "❌ Wi-Fi failed to start properly");
        return ESP_FAIL;
    }

    // 初始化状态
    s_wifi_status.state = WIFI_STATE_DISCONNECTED;
    s_wifi_status.retry_count = 0;
    s_connecting_in_progress = false;
    memset(s_wifi_status.ip_address, 0, sizeof(s_wifi_status.ip_address));

    ESP_LOGI(TAG, "✅ Wi-Fi Manager initialized successfully");
    ESP_LOGI(TAG, "📡 Wi-Fi ready for connection");
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

    // 检查是否已经在连接中
    if (s_connecting_in_progress) {
        ESP_LOGW(TAG, "⚠️ Wi-Fi connection already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    // 检查当前Wi-Fi状态
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to get Wi-Fi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "🔗 Connecting to Wi-Fi: %s", ssid);

    // 如果已经连接到相同的网络，先断开
    if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
        ESP_LOGI(TAG, "🔌 Disconnecting from current network...");
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500)); // 等待断开完成
    }

    // 配置Wi-Fi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL && strlen(password) > 0) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    // 设置其他Wi-Fi配置参数
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待配置生效
    vTaskDelay(pdMS_TO_TICKS(100));

    // 重置状态和事件组
    s_wifi_status.state = WIFI_STATE_CONNECTING;
    s_wifi_status.retry_count = 0;
    s_connecting_in_progress = true;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // 开始连接
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start Wi-Fi connection: %s", esp_err_to_name(ret));
        s_wifi_status.state = WIFI_STATE_FAILED;
        s_connecting_in_progress = false;
        return ret;
    }

    ESP_LOGI(TAG, "⏳ Waiting for Wi-Fi connection...");

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Connected to Wi-Fi: %s", ssid);
        ESP_LOGI(TAG, "📍 IP Address: %s", s_wifi_status.ip_address);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ Failed to connect to Wi-Fi: %s", ssid);
        s_connecting_in_progress = false;
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "⏰ Wi-Fi connection timeout after %d ms", WIFI_CONNECT_TIMEOUT_MS);
        s_wifi_status.state = WIFI_STATE_FAILED;
        s_connecting_in_progress = false;
        // 停止连接尝试
        esp_wifi_disconnect();
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * 断开Wi-Fi连接
 */
esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "🔌 Disconnecting from Wi-Fi...");

    // 重置连接状态标志
    s_connecting_in_progress = false;

    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        s_wifi_status.retry_count = 0;
        memset(s_wifi_status.ip_address, 0, sizeof(s_wifi_status.ip_address));

        // 清除事件组标志
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

        ESP_LOGI(TAG, "✅ Wi-Fi disconnected");
    } else {
        ESP_LOGE(TAG, "❌ Failed to disconnect Wi-Fi: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * 重置Wi-Fi连接状态
 */
esp_err_t wifi_manager_reset(void)
{
    ESP_LOGI(TAG, "🔄 Resetting Wi-Fi manager state...");

    // 先断开连接
    esp_wifi_disconnect();

    // 重置所有状态
    s_wifi_status.state = WIFI_STATE_DISCONNECTED;
    s_wifi_status.retry_count = 0;
    s_connecting_in_progress = false;
    memset(s_wifi_status.ip_address, 0, sizeof(s_wifi_status.ip_address));

    // 清除事件组
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // 等待状态稳定
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "✅ Wi-Fi manager state reset complete");
    return ESP_OK;
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
    // 首先检查内部状态
    if (s_wifi_status.state != WIFI_STATE_CONNECTED) {
        return false;
    }

    // 进一步验证实际的Wi-Fi连接状态
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "📡 Wi-Fi AP信息获取失败，可能未连接: %s", esp_err_to_name(ret));
        return false;
    }

    // 检查IP地址是否有效
    if (strlen(s_wifi_status.ip_address) == 0) {
        ESP_LOGD(TAG, "📡 IP地址为空，Wi-Fi可能未完全连接");
        return false;
    }

    return true;
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

/**
 * 获取Wi-Fi信号强度(RSSI)
 */
int8_t wifi_manager_get_rssi(void)
{
    if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            return ap_info.rssi;
        }
    }
    return 0;
}

/**
 * 获取详细的Wi-Fi调试信息
 */
esp_err_t wifi_manager_get_debug_info(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_record_t ap_info = {0};
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    int len = snprintf(buffer, buffer_size,
        "Wi-Fi Debug Info:\n"
        "  State: %s\n"
        "  Mode: %d\n"
        "  IP: %s\n"
        "  Retry Count: %d\n"
        "  Connecting: %s\n"
        "  Connect Time: %lu\n",
        (s_wifi_status.state == WIFI_STATE_DISCONNECTED) ? "DISCONNECTED" :
        (s_wifi_status.state == WIFI_STATE_CONNECTING) ? "CONNECTING" :
        (s_wifi_status.state == WIFI_STATE_CONNECTED) ? "CONNECTED" : "FAILED",
        mode,
        s_wifi_status.ip_address,
        s_wifi_status.retry_count,
        s_connecting_in_progress ? "YES" : "NO",
        (unsigned long)s_wifi_status.connect_time
    );

    if (s_wifi_status.state == WIFI_STATE_CONNECTED &&
        esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        len += snprintf(buffer + len, buffer_size - len,
            "  SSID: %s\n"
            "  RSSI: %d dBm\n"
            "  Channel: %d\n"
            "  Auth Mode: %d\n",
            ap_info.ssid,
            ap_info.rssi,
            ap_info.primary,
            ap_info.authmode
        );
    }

    return ESP_OK;
}
