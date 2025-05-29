#include "time_manager.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "TIME_MGR";

// 时间同步事件组
static EventGroupHandle_t s_time_event_group = NULL;
#define TIME_SYNC_DONE_BIT BIT0

// 时间管理器状态
static bool s_time_manager_initialized = false;
static time_sync_status_t s_sync_status = TIME_SYNC_STATUS_RESET;
static uint32_t s_last_sync_time = 0;

/**
 * @brief SNTP时间同步回调函数
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "⏰ Time synchronization completed");
    s_sync_status = TIME_SYNC_STATUS_COMPLETED;
    s_last_sync_time = xTaskGetTickCount();

    if (s_time_event_group) {
        xEventGroupSetBits(s_time_event_group, TIME_SYNC_DONE_BIT);
    }

    // 打印同步后的时间
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "✅ Current time: %s", strftime_buf);
}

/**
 * @brief 初始化时间管理器
 */
esp_err_t time_manager_init(void)
{
    if (s_time_manager_initialized) {
        ESP_LOGW(TAG, "⚠️ Time manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🚀 Initializing Time Manager...");

    // 创建事件组
    s_time_event_group = xEventGroupCreate();
    if (s_time_event_group == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create time event group");
        return ESP_FAIL;
    }

    // 设置时区
    setenv("TZ", TIME_MANAGER_TIMEZONE, 1);
    tzset();

    // 初始化SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, TIME_MANAGER_NTP_SERVER1);
    esp_sntp_setservername(1, TIME_MANAGER_NTP_SERVER2);
    esp_sntp_setservername(2, TIME_MANAGER_NTP_SERVER3);

    // 设置时间同步回调
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    s_time_manager_initialized = true;
    s_sync_status = TIME_SYNC_STATUS_RESET;

    ESP_LOGI(TAG, "✅ Time Manager initialized successfully");
    ESP_LOGI(TAG, "📡 NTP Servers: %s, %s, %s",
             TIME_MANAGER_NTP_SERVER1,
             TIME_MANAGER_NTP_SERVER2,
             TIME_MANAGER_NTP_SERVER3);
    ESP_LOGI(TAG, "🌍 Timezone: %s", TIME_MANAGER_TIMEZONE);

    return ESP_OK;
}

/**
 * @brief 启动NTP时间同步（非阻塞）
 */
esp_err_t time_manager_start_sync(void)
{
    if (!s_time_manager_initialized) {
        ESP_LOGE(TAG, "❌ Time manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sync_status == TIME_SYNC_STATUS_IN_PROGRESS) {
        ESP_LOGW(TAG, "⚠️ Time sync already in progress");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🔄 Starting NTP time synchronization (non-blocking)...");

    s_sync_status = TIME_SYNC_STATUS_IN_PROGRESS;
    xEventGroupClearBits(s_time_event_group, TIME_SYNC_DONE_BIT);

    esp_sntp_init();

    // 非阻塞启动，不等待同步完成
    ESP_LOGI(TAG, "📡 NTP sync started in background, system continues normally");
    return ESP_OK;
}

/**
 * @brief 停止NTP时间同步
 */
esp_err_t time_manager_stop_sync(void)
{
    if (!s_time_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "🛑 Stopping NTP time synchronization...");
    esp_sntp_stop();

    return ESP_OK;
}

/**
 * @brief 获取当前时间信息
 */
esp_err_t time_manager_get_time(time_info_t* time_info)
{
    if (time_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(time_info, 0, sizeof(time_info_t));

    time(&time_info->timestamp);
    localtime_r(&time_info->timestamp, &time_info->timeinfo);

    // 检查时间是否有效（大于2020年1月1日）
    time_info->is_valid = (time_info->timestamp > 1577836800); // 2020-01-01 00:00:00 UTC

    if (time_info->is_valid) {
        strftime(time_info->formatted_time, sizeof(time_info->formatted_time),
                "%Y-%m-%d %H:%M:%S", &time_info->timeinfo);
    } else {
        strcpy(time_info->formatted_time, "时间未同步");
    }

    time_info->sync_status = s_sync_status;
    time_info->last_sync_time = s_last_sync_time;

    return ESP_OK;
}

/**
 * @brief 获取Unix时间戳（智能混合模式）
 */
time_t time_manager_get_timestamp(void)
{
    time_t now = 0;
    time(&now);

    // 如果时间有效（已同步），返回真实Unix时间戳
    if (now > 1577836800) { // 大于2020-01-01 00:00:00 UTC
        return now;
    }

    // 时间未同步，返回0表示使用相对时间
    return 0;
}

/**
 * @brief 获取智能时间戳（混合模式）
 * @param use_relative 如果为true，在NTP未同步时使用相对时间
 * @return 时间戳（Unix时间戳或相对时间）
 */
time_t time_manager_get_smart_timestamp(bool use_relative)
{
    time_t unix_time = time_manager_get_timestamp();

    if (unix_time > 0) {
        // NTP时间可用，返回Unix时间戳
        return unix_time;
    }

    if (use_relative) {
        // NTP不可用但允许相对时间，返回系统启动后的秒数
        return xTaskGetTickCount() / configTICK_RATE_HZ;
    }

    // 不允许相对时间，返回0
    return 0;
}

/**
 * @brief 获取格式化时间字符串
 */
esp_err_t time_manager_get_formatted_time(char* buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    time_info_t time_info;
    esp_err_t ret = time_manager_get_time(&time_info);
    if (ret != ESP_OK) {
        return ret;
    }

    strncpy(buffer, time_info.formatted_time, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    return ESP_OK;
}

/**
 * @brief 检查时间是否有效
 */
bool time_manager_is_time_valid(void)
{
    time_t now = 0;
    time(&now);
    return (now > 1577836800); // 2020-01-01 00:00:00 UTC
}

/**
 * @brief 获取时间同步状态
 */
time_sync_status_t time_manager_get_sync_status(void)
{
    return s_sync_status;
}

/**
 * @brief 设置时区
 */
esp_err_t time_manager_set_timezone(const char* timezone)
{
    if (timezone == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "🌍 Setting timezone to: %s", timezone);
    setenv("TZ", timezone, 1);
    tzset();

    return ESP_OK;
}

/**
 * @brief 强制重新同步时间
 */
esp_err_t time_manager_force_sync(void)
{
    ESP_LOGI(TAG, "🔄 Forcing time resynchronization...");

    // 停止当前同步
    time_manager_stop_sync();

    // 重新启动同步
    return time_manager_start_sync();
}

/**
 * @brief 获取上次同步时间
 */
uint32_t time_manager_get_last_sync_time(void)
{
    return s_last_sync_time;
}
