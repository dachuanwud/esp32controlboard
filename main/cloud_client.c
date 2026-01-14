#include "cloud_client.h"
#include "main.h"
#include "data_integration.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_mac.h"
#include "ota_manager.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "CLOUD_CLIENT";

// 函数声明
static void set_last_error(const char* error_msg);
static cloud_command_type_t parse_command_type(const char* command_str);
static esp_err_t handle_ota_command(const cJSON* data);
static esp_err_t download_and_install_firmware(const char* url, uint32_t expected_size);
static void ota_progress_callback(uint8_t progress_percent, const char* status_message);
static esp_http_client_handle_t create_http_client(const char* url);
static esp_err_t add_auth_headers(esp_http_client_handle_t client);
static esp_err_t fetch_pending_commands(void);

// 全局变量
static cloud_device_info_t s_device_info = {0};
static bool s_client_running = false;
static bool s_client_connected = false;
static TaskHandle_t s_status_task_handle = NULL;
static TaskHandle_t s_command_task_handle = NULL;
static void (*s_command_callback)(const cloud_command_t* command) = NULL;
static void (*s_status_callback)(const device_status_data_t* status) = NULL;

// OTA相关变量
static char s_current_command_id[64] = {0};

// HTTP响应缓冲区
static char s_response_buffer[MAX_HTTP_RESPONSE_SIZE];
static int s_response_len = 0;

// 错误处理和重连机制
static char s_last_error[256] = {0};
static uint32_t s_retry_count = 0;
static uint32_t s_last_retry_time = 0;
static network_status_t s_network_status = NETWORK_DISCONNECTED;

// 设备认证
static char s_device_key[64] = {0};
static bool s_auth_enabled = false;

/**
 * 内存监控和保护
 */
static void check_memory_usage(const char* context)
{
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();

    if (free_heap < 30 * 1024) { // 警告阈值30KB
        ESP_LOGW(TAG, "⚠️ [%s] 内存不足警告: 可用=%lu KB, 最小=%lu KB",
                 context, (unsigned long)free_heap / 1024, (unsigned long)min_free_heap / 1024);
    } else {
        ESP_LOGD(TAG, "💾 [%s] 内存状态: 可用=%lu KB, 最小=%lu KB",
                 context, (unsigned long)free_heap / 1024, (unsigned long)min_free_heap / 1024);
    }
}

/**
 * HTTP事件处理函数
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (s_response_len + evt->data_len < sizeof(s_response_buffer) - 1) {
                memcpy(s_response_buffer + s_response_len, evt->data, evt->data_len);
                s_response_len += evt->data_len;
                s_response_buffer[s_response_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/**
 * 发送HTTP POST请求
 */
static esp_err_t send_http_post(const char* url, const char* data)
{
    // 检查可用内存
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < 50 * 1024) { // 至少需要50KB可用内存
        ESP_LOGE(TAG, "❌ 可用内存不足 (%lu KB)，跳过HTTP请求", (unsigned long)free_heap / 1024);
        return ESP_ERR_NO_MEM;
    }

    s_response_len = 0;
    memset(s_response_buffer, 0, sizeof(s_response_buffer));
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-CloudClient/1.0");
    esp_http_client_set_header(client, "Connection", "close");
    
    esp_err_t err = esp_http_client_set_post_field(client, data, strlen(data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST data: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                status_code, esp_http_client_get_content_length(client));
        
        if (status_code >= 200 && status_code < 300) {
            err = ESP_OK;
        } else {
            ESP_LOGW(TAG, "HTTP POST failed with status %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

/**
 * 生成设备ID
 */
static void generate_device_id(char* device_id, size_t size)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_id, size, "esp32-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * 收集当前设备状态数据
 */
static esp_err_t collect_current_status(device_status_data_t* status)
{
    if (!status) {
        ESP_LOGE(TAG, "❌ 状态数据指针为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 检查可用内存
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < 30 * 1024) { // 至少需要30KB可用内存
        ESP_LOGW(TAG, "⚠️ 可用内存不足 (%lu KB)，跳过状态收集", (unsigned long)free_heap / 1024);
        return ESP_ERR_NO_MEM;
    }

    // 使用数据集成模块收集状态
    esp_err_t ret = data_integration_collect_status(status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 数据集成收集失败: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * 状态上报任务
 */
static void status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "📊 状态上报任务已启动");
    ESP_LOGI(TAG, "⏰ 上报间隔: %d秒", DEVICE_STATUS_INTERVAL_MS / 1000);

    // 等待系统完全初始化 - 防止启动时的内存访问冲突
    ESP_LOGI(TAG, "⏳ 等待系统稳定...");
    vTaskDelay(pdMS_TO_TICKS(5000)); // 等待5秒
    ESP_LOGI(TAG, "✅ 系统稳定，开始状态上报");

    device_status_data_t status_data;
    uint32_t report_count = 0;
    uint32_t success_count = 0;
    uint32_t error_count = 0;

    while (s_client_running) {
        if (wifi_manager_is_connected()) {
            ESP_LOGD(TAG, "🔄 开始第%" PRIu32 "次状态收集...", report_count + 1);

            // 监控内存使用情况
            check_memory_usage("状态上报");

            // 收集设备状态
            esp_err_t ret = collect_current_status(&status_data);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "📊 状态数据收集成功 - 堆内存: %" PRIu32 ", 运行时间: %" PRIu32 "s, WiFi: %s",
                         status_data.free_heap,
                         status_data.uptime_seconds,
                         status_data.wifi_connected ? "已连接" : "未连接");

                // 发送状态到Supabase
                ESP_LOGD(TAG, "📤 发送状态数据到Supabase...");
                ret = cloud_client_send_device_status(&status_data);
                if (ret == ESP_OK) {
                    s_client_connected = true;
                    s_device_info.last_seen = status_data.uptime_seconds;
                    s_device_info.network_status = NETWORK_CONNECTED;
                    s_retry_count = 0;
                    success_count++;

                    ESP_LOGI(TAG, "✅ 状态上报成功 [%" PRIu32 "/%" PRIu32 "] - 成功率: %.1f%%",
                             success_count, report_count + 1,
                             (float)success_count / (report_count + 1) * 100);
                } else {
                    s_client_connected = false;
                    s_device_info.network_status = NETWORK_ERROR;
                    error_count++;

                    ESP_LOGW(TAG, "⚠️ 状态上报失败 [%" PRIu32 "/%" PRIu32 "]: %s",
                             error_count, report_count + 1,
                             cloud_client_get_last_error());

                    // 尝试重连
                    if (s_retry_count < MAX_RETRY_ATTEMPTS) {
                        ESP_LOGI(TAG, "🔄 尝试重连 (第%" PRIu32 "次)...", s_retry_count + 1);
                        cloud_client_reconnect();
                    } else {
                        ESP_LOGE(TAG, "❌ 达到最大重试次数，暂停重连");
                    }
                }
            } else {
                ESP_LOGE(TAG, "❌ 状态数据收集失败");
                error_count++;
            }

            report_count++;
        } else {
            s_client_connected = false;
            s_device_info.network_status = NETWORK_DISCONNECTED;
            ESP_LOGW(TAG, "📡 Wi-Fi未连接，跳过状态上报");
        }

        // 等待下次上报 - 减少日志频率
        ESP_LOGV(TAG, "⏳ 等待%d秒后进行下次上报...", DEVICE_STATUS_INTERVAL_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(DEVICE_STATUS_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "📊 状态上报任务已停止");
    ESP_LOGI(TAG, "📈 统计信息 - 总计: %lu, 成功: %lu, 失败: %lu",
             (unsigned long)report_count, (unsigned long)success_count, (unsigned long)error_count);

    s_status_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * 主动获取待处理指令
 */
static esp_err_t fetch_pending_commands(void)
{
    if (!wifi_manager_is_connected()) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    ESP_LOGD(TAG, "🔍 主动获取待处理指令...");

    // 构建请求URL
    char url[256];
    snprintf(url, sizeof(url), "%s/api/device-commands/%s/pending",
             CLOUD_SERVER_URL, s_device_info.device_id);

    // 创建HTTP客户端
    esp_http_client_handle_t client = create_http_client(url);
    if (!client) {
        ESP_LOGE(TAG, "❌ 创建HTTP客户端失败");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    // 设置请求方法和头部
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    ret = add_auth_headers(client);

    if (ret == ESP_OK) {
        // 清空响应缓冲区
        s_response_len = 0;
        memset(s_response_buffer, 0, sizeof(s_response_buffer));

        ret = esp_http_client_perform(client);
        if (ret == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) {
                ESP_LOGD(TAG, "✅ 成功获取待处理指令");

                // 处理响应中的指令
                cloud_command_t commands[MAX_COMMANDS_PER_REQUEST];
                int command_count = cloud_client_get_commands(commands, MAX_COMMANDS_PER_REQUEST);

                if (command_count > 0) {
                    ESP_LOGI(TAG, "📤 获取到 %d 个待处理指令", command_count);

                    // 处理每个指令
                    for (int i = 0; i < command_count; i++) {
                        ESP_LOGI(TAG, "🔧 处理指令: %" PRIu32 ", 类型: %d", commands[i].id, commands[i].command);

                        if (s_command_callback) {
                            s_command_callback(&commands[i]);
                        }
                    }
                } else {
                    ESP_LOGD(TAG, "📭 没有待处理指令");
                }
            } else {
                ESP_LOGW(TAG, "⚠️ 获取指令失败，HTTP状态码: %d", status_code);
                ret = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "❌ HTTP请求失败: %s", esp_err_to_name(ret));
        }
    }

    esp_http_client_cleanup(client);
    return ret;
}

/**
 * 指令轮询任务
 */
static void command_task(void *pvParameters)
{
    ESP_LOGI(TAG, "📋 指令轮询任务已启动");
    ESP_LOGI(TAG, "⏰ 轮询间隔: %d秒", COMMAND_POLL_INTERVAL_MS / 1000);

    // 等待系统完全初始化 - 防止启动时的内存访问冲突
    ESP_LOGI(TAG, "⏳ 等待系统稳定...");
    vTaskDelay(pdMS_TO_TICKS(8000)); // 等待8秒，比状态任务晚一些
    ESP_LOGI(TAG, "✅ 系统稳定，开始指令轮询");

    uint32_t poll_count = 0;

    while (s_client_running) {
        if (wifi_manager_is_connected() && s_client_connected) {
            poll_count++;
            ESP_LOGV(TAG, "🔍 第%" PRIu32 "次指令轮询...", poll_count);

            // 主动获取待处理指令
            esp_err_t ret = fetch_pending_commands();
            if (ret != ESP_OK) {
                ESP_LOGV(TAG, "⚠️ 指令轮询失败: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGV(TAG, "📡 网络未连接，跳过指令轮询");
        }

        vTaskDelay(pdMS_TO_TICKS(COMMAND_POLL_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "📋 指令轮询任务已停止 (总计轮询%" PRIu32 "次)", poll_count);
    s_command_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * 初始化云客户端
 */
esp_err_t cloud_client_init(void)
{
    ESP_LOGI(TAG, "🌐 初始化云客户端...");
    ESP_LOGI(TAG, "📍 服务器地址: %s", CLOUD_SERVER_URL);
    ESP_LOGI(TAG, "📍 Supabase项目: %s", SUPABASE_PROJECT_URL);

    // 生成设备ID
    generate_device_id(s_device_info.device_id, sizeof(s_device_info.device_id));
    ESP_LOGI(TAG, "🆔 生成设备ID: %s", s_device_info.device_id);

    // 设置默认设备信息
    snprintf(s_device_info.device_name, sizeof(s_device_info.device_name), "ESP32控制板-%.8s",
             s_device_info.device_id + 6); // 跳过"esp32-"前缀，限制长度
    strcpy(s_device_info.device_type, "ESP32");
    strcpy(s_device_info.firmware_version, "2.1.0");
    strcpy(s_device_info.hardware_version, "v2.1");
    s_device_info.status = CLOUD_STATUS_OFFLINE;
    s_device_info.network_status = NETWORK_DISCONNECTED;
    s_device_info.retry_count = 0;

    ESP_LOGI(TAG, "📋 设备名称: %s", s_device_info.device_name);
    ESP_LOGI(TAG, "📋 设备类型: %s", s_device_info.device_type);
    ESP_LOGI(TAG, "📋 固件版本: %s", s_device_info.firmware_version);
    ESP_LOGI(TAG, "📋 硬件版本: %s", s_device_info.hardware_version);

    // 获取MAC地址
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_device_info.mac_address, sizeof(s_device_info.mac_address),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "📋 MAC地址: %s", s_device_info.mac_address);

    // 初始化错误信息
    memset(s_last_error, 0, sizeof(s_last_error));

    ESP_LOGI(TAG, "✅ 云客户端初始化完成");
    ESP_LOGI(TAG, "⚙️ 状态上报间隔: %d秒", DEVICE_STATUS_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "⚙️ 指令轮询间隔: %d秒", COMMAND_POLL_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "⚙️ 最大重试次数: %d", MAX_RETRY_ATTEMPTS);

    return ESP_OK;
}

/**
 * 启动云客户端
 */
esp_err_t cloud_client_start(void)
{
    if (s_client_running) {
        ESP_LOGW(TAG, "⚠️ 云客户端已在运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🚀 启动云客户端...");
    ESP_LOGI(TAG, "📊 创建后台任务...");

    s_client_running = true;
    s_client_connected = false;
    s_network_status = NETWORK_DISCONNECTED;
    s_retry_count = 0;

    // 创建状态上报任务 (优化栈大小：10240 -> 6144字节，JSON序列化不需要太多栈)
    ESP_LOGI(TAG, "📊 创建状态上报任务 (栈大小: 6144, 优先级: 5)");
    BaseType_t ret = xTaskCreate(status_task, "cloud_status", 6144, NULL, 5, &s_status_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建状态上报任务失败");
        s_client_running = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ 状态上报任务创建成功");

    // 创建指令轮询任务 (优化栈大小：16384 -> 8192字节，OTA下载可通过流式处理减少栈需求)
    ESP_LOGI(TAG, "📊 创建指令轮询任务 (栈大小: 8192, 优先级: 5)");
    ret = xTaskCreate(command_task, "cloud_command", 8192, NULL, 5, &s_command_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建指令轮询任务失败");
        s_client_running = false;
        if (s_status_task_handle) {
            ESP_LOGW(TAG, "🧹 清理状态上报任务");
            vTaskDelete(s_status_task_handle);
            s_status_task_handle = NULL;
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ 指令轮询任务创建成功");

    ESP_LOGI(TAG, "✅ 云客户端启动成功");
    ESP_LOGI(TAG, "🔄 后台任务已开始运行");

    return ESP_OK;
}

/**
 * 停止云客户端
 */
esp_err_t cloud_client_stop(void)
{
    if (!s_client_running) {
        ESP_LOGW(TAG, "⚠️ 云客户端未运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🛑 停止云客户端...");
    s_client_running = false;
    s_client_connected = false;

    // 等待任务结束
    int timeout = 50; // 5秒超时
    while ((s_status_task_handle || s_command_task_handle) && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }

    if (timeout == 0) {
        ESP_LOGW(TAG, "⚠️ 任务停止超时");
    }

    ESP_LOGI(TAG, "✅ 云客户端已停止");
    return ESP_OK;
}

/**
 * 注册设备到云服务器
 */
esp_err_t cloud_client_register_device(const char* device_id, const char* device_name, const char* local_ip)
{
    if (!wifi_manager_is_connected()) {
        ESP_LOGE(TAG, "❌ Wi-Fi未连接，无法注册设备");
        s_network_status = NETWORK_DISCONNECTED;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📡 开始注册设备到云服务器...");
    ESP_LOGI(TAG, "🆔 设备ID: %s", device_id ? device_id : s_device_info.device_id);
    ESP_LOGI(TAG, "📋 设备名称: %s", device_name ? device_name : s_device_info.device_name);
    ESP_LOGI(TAG, "🌐 本地IP: %s", local_ip ? local_ip : wifi_manager_get_ip_address());

    s_network_status = NETWORK_CONNECTING;

    // 更新设备信息
    if (device_id) {
        strncpy(s_device_info.device_id, device_id, sizeof(s_device_info.device_id) - 1);
        ESP_LOGD(TAG, "🔄 更新设备ID: %s", s_device_info.device_id);
    }
    if (device_name) {
        strncpy(s_device_info.device_name, device_name, sizeof(s_device_info.device_name) - 1);
        ESP_LOGD(TAG, "🔄 更新设备名称: %s", s_device_info.device_name);
    }
    if (local_ip) {
        strncpy(s_device_info.local_ip, local_ip, sizeof(s_device_info.local_ip) - 1);
    } else {
        const char* ip = wifi_manager_get_ip_address();
        if (ip) {
            strncpy(s_device_info.local_ip, ip, sizeof(s_device_info.local_ip) - 1);
        }
    }
    ESP_LOGD(TAG, "🔄 更新本地IP: %s", s_device_info.local_ip);

    // 构建注册数据
    ESP_LOGI(TAG, "📝 构建注册数据...");
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "❌ 创建JSON对象失败");
        s_network_status = NETWORK_ERROR;
        set_last_error("创建注册JSON对象失败");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "deviceName", s_device_info.device_name);
    cJSON_AddStringToObject(json, "localIP", s_device_info.local_ip);
    cJSON_AddStringToObject(json, "deviceType", s_device_info.device_type);
    cJSON_AddStringToObject(json, "firmwareVersion", s_device_info.firmware_version);
    cJSON_AddStringToObject(json, "hardwareVersion", s_device_info.hardware_version);
    cJSON_AddStringToObject(json, "macAddress", s_device_info.mac_address);

    char *json_string = cJSON_Print(json);
    esp_err_t err = ESP_FAIL;

    if (json_string) {
        char url[256];
        snprintf(url, sizeof(url), "%s/register-device", CLOUD_SERVER_URL);
        ESP_LOGI(TAG, "🌐 发送注册请求到: %s", url);
        ESP_LOGD(TAG, "📤 注册数据: %s", json_string);

        err = send_http_post(url, json_string);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✅ 设备注册成功");
            s_device_info.status = CLOUD_STATUS_ONLINE;
            s_network_status = NETWORK_CONNECTED;
            s_retry_count = 0;
            ESP_LOGI(TAG, "🎉 设备已成功注册到Supabase数据库");

            // 检查是否是OTA升级后的首次启动
            if (ota_manager_check_rollback_required()) {
                ESP_LOGI(TAG, "📤 检测到OTA升级后首次启动，发送固件版本更新通知");
                // 延迟发送，确保设备注册完成
                vTaskDelay(pdMS_TO_TICKS(1000));

                // 发送固件版本更新通知
                char version_msg[128];
                snprintf(version_msg, sizeof(version_msg), "固件已成功升级到版本 %s", s_device_info.firmware_version);
                cloud_client_send_status(CLOUD_STATUS_ONLINE, version_msg);
            }
        } else {
            ESP_LOGE(TAG, "❌ 设备注册失败，HTTP错误");
            s_device_info.status = CLOUD_STATUS_ERROR;
            s_network_status = NETWORK_ERROR;
            set_last_error("设备注册HTTP请求失败");
        }

        cJSON_free(json_string);
    } else {
        ESP_LOGE(TAG, "❌ 序列化注册JSON失败");
        s_network_status = NETWORK_ERROR;
        set_last_error("序列化注册JSON失败");
        err = ESP_ERR_NO_MEM;
    }

    cJSON_Delete(json);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "📊 设备注册完成，状态: 在线");
    } else {
        ESP_LOGW(TAG, "⚠️ 设备注册失败，将在后台重试");
    }

    return err;
}

/**
 * 向云服务器发送状态更新
 */
esp_err_t cloud_client_send_status(cloud_status_t status, const char* data)
{
    if (!wifi_manager_is_connected()) {
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);

    const char* status_str = "offline";
    switch (status) {
        case CLOUD_STATUS_ONLINE:
            status_str = "online";
            break;
        case CLOUD_STATUS_ERROR:
            status_str = "error";
            break;
        default:
            status_str = "offline";
            break;
    }
    cJSON_AddStringToObject(json, "status", status_str);

    if (data) {
        cJSON *data_obj = cJSON_Parse(data);
        if (data_obj) {
            cJSON_AddItemToObject(json, "data", data_obj);
        } else {
            cJSON_AddStringToObject(json, "data", data);
        }
    }

    char *json_string = cJSON_Print(json);
    esp_err_t err = ESP_FAIL;

    if (json_string) {
        char url[256];
        snprintf(url, sizeof(url), "%s/device-status", CLOUD_SERVER_URL);

        err = send_http_post(url, json_string);
        cJSON_free(json_string);
    }

    cJSON_Delete(json);
    return err;
}

/**
 * 解析指令类型字符串
 */
static cloud_command_type_t parse_command_type(const char* command_str)
{
    if (!command_str) {
        return CLOUD_CMD_UNKNOWN;
    }

    if (strcmp(command_str, "sbus_update") == 0) {
        return CLOUD_CMD_SBUS_UPDATE;
    } else if (strcmp(command_str, "motor_control") == 0) {
        return CLOUD_CMD_MOTOR_CONTROL;
    } else if (strcmp(command_str, "wifi_config") == 0) {
        return CLOUD_CMD_WIFI_CONFIG;
    } else if (strcmp(command_str, "ota_update") == 0) {
        return CLOUD_CMD_OTA_UPDATE;
    } else if (strcmp(command_str, "reboot") == 0) {
        return CLOUD_CMD_REBOOT;
    }

    return CLOUD_CMD_UNKNOWN;
}

/**
 * 处理OTA升级指令
 */
static esp_err_t handle_ota_command(const cJSON* data)
{
    ESP_LOGI(TAG, "🚀 开始处理OTA升级指令");

#if !ENABLE_CLOUD_OTA
    (void)data;
    ESP_LOGW(TAG, "⚠️ 云端OTA已禁用，拒绝执行指令");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    if (!data) {
        ESP_LOGE(TAG, "❌ OTA指令数据为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 解析OTA参数
    cJSON *firmware_url = cJSON_GetObjectItem(data, "firmware_url");
    cJSON *firmware_size = cJSON_GetObjectItem(data, "firmware_size");
    cJSON *firmware_version = cJSON_GetObjectItem(data, "firmware_version");
    cJSON *firmware_hash = cJSON_GetObjectItem(data, "firmware_hash");

    if (!firmware_url || !cJSON_IsString(firmware_url)) {
        ESP_LOGE(TAG, "❌ 固件URL无效或缺失");
        return ESP_ERR_INVALID_ARG;
    }

    const char* url = cJSON_GetStringValue(firmware_url);
    uint32_t size = firmware_size ? (uint32_t)cJSON_GetNumberValue(firmware_size) : 0;
    const char* version = firmware_version ? cJSON_GetStringValue(firmware_version) : "unknown";
    const char* hash = firmware_hash ? cJSON_GetStringValue(firmware_hash) : NULL;

    ESP_LOGI(TAG, "📦 OTA升级参数:");
    ESP_LOGI(TAG, "   📍 固件URL: %s", url);
    ESP_LOGI(TAG, "   📏 固件大小: %lu bytes (%.2f KB)", (unsigned long)size, (float)size / 1024.0);
    ESP_LOGI(TAG, "   🏷️ 固件版本: %s", version);
    if (hash) {
        ESP_LOGI(TAG, "   🔐 固件哈希: %.16s...", hash);
    }

    // 检查当前固件版本
    ESP_LOGI(TAG, "🔍 当前固件版本: %s", s_device_info.firmware_version);
    if (strcmp(version, s_device_info.firmware_version) == 0) {
        ESP_LOGW(TAG, "⚠️ 目标版本与当前版本相同，跳过升级");
        return ESP_OK;
    }

    // 检查可用内存
    uint32_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "💾 当前可用内存: %lu bytes (%.2f KB)", (unsigned long)free_heap, (float)free_heap / 1024.0);

    if (free_heap < 100 * 1024) { // 至少需要100KB可用内存
        ESP_LOGE(TAG, "❌ 可用内存不足，无法进行OTA升级");
        return ESP_ERR_NO_MEM;
    }

    // 发送开始升级状态
    cloud_client_send_command_feedback(s_current_command_id, "processing", "开始下载固件");

    // 下载并安装固件
    ESP_LOGI(TAG, "📥 开始下载并安装固件...");
    esp_err_t ret = download_and_install_firmware(url, size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA升级失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ OTA升级成功，准备重启应用新固件");

    // 发送成功状态（在重启前）
    cloud_client_send_command_feedback(s_current_command_id, "completed", "固件升级成功，即将重启");

    return ESP_OK;
}

/**
 * 从URL下载并安装固件
 */
static esp_err_t download_and_install_firmware(const char* url, uint32_t expected_size)
{
    ESP_LOGI(TAG, "📥 开始从URL下载固件: %s", url);

    // 创建HTTP客户端
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .buffer_size_tx = 1024
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "❌ 创建HTTP客户端失败");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    int content_length = 0;
    bool ota_started = false;

    // 发送HTTP请求
    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTP连接失败: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // 获取内容长度
    content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "❌ 无效的内容长度: %d", content_length);
        ret = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGI(TAG, "📏 固件大小: %d bytes", content_length);

    // 检查HTTP状态码
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "❌ HTTP错误状态码: %d", status_code);
        ret = ESP_FAIL;
        goto cleanup;
    }

    // 检查Content-Type
    char *content_type = NULL;
    esp_http_client_get_header(client, "Content-Type", &content_type);
    if (content_type && strstr(content_type, "application/json")) {
        ESP_LOGE(TAG, "❌ 服务器返回错误信息而非固件文件 (Content-Type: %s)", content_type);
        ret = ESP_FAIL;
        goto cleanup;
    }

    // 验证文件大小
    if (expected_size > 0 && (uint32_t)content_length != expected_size) {
        ESP_LOGW(TAG, "⚠️ 固件大小不匹配: 期望 %lu, 实际 %d",
                (unsigned long)expected_size, content_length);
        // 如果大小差异太大，可能是错误响应
        if (content_length < 1000) {
            ESP_LOGE(TAG, "❌ 固件文件太小，可能是错误响应");
            ret = ESP_FAIL;
            goto cleanup;
        }
    }

    // 开始OTA更新
    ret = ota_manager_begin((uint32_t)content_length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 开始OTA更新失败: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ota_started = true;

    // 下载并写入固件数据 (使用较小的缓冲区以减少内存使用)
    char *buffer = malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "❌ 分配下载缓冲区失败");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    int total_read = 0;
    while (total_read < content_length) {
        int data_read = esp_http_client_read(client, buffer, 2048);
        if (data_read < 0) {
            ESP_LOGE(TAG, "❌ 读取固件数据失败");
            ret = ESP_FAIL;
            break;
        }

        if (data_read == 0) {
            ESP_LOGW(TAG, "⚠️ 数据读取完成");
            break;
        }

        // 写入OTA数据
        ret = ota_manager_write(buffer, data_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ 写入OTA数据失败: %s", esp_err_to_name(ret));
            break;
        }

        total_read += data_read;

        // 每128KB打印一次进度，减少日志输出频率
        if (total_read % (128 * 1024) == 0 || total_read == content_length) {
            ESP_LOGI(TAG, "📥 下载进度: %d/%d bytes (%.1f%%)",
                    total_read, content_length,
                    (float)total_read * 100.0f / content_length);
        }
    }

    free(buffer);

    if (ret == ESP_OK && total_read == content_length) {
        // 完成OTA更新
        ret = ota_manager_end();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ 固件下载和安装成功");

            // 发送OTA完成状态到云端
            ESP_LOGI(TAG, "📤 发送OTA完成状态到云端");
            cloud_client_send_command_feedback(s_current_command_id, "completed", "OTA升级成功完成，即将重启");

            // 等待状态发送完成
            vTaskDelay(pdMS_TO_TICKS(2000));

            ESP_LOGI(TAG, "🔄 系统将在3秒后重启以应用新固件");

            // 延迟重启，让状态发送和日志输出完成
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "❌ 完成OTA更新失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ 固件下载不完整: %d/%d bytes", total_read, content_length);
        ret = ESP_FAIL;
    }

cleanup:
    if (ota_started && ret != ESP_OK) {
        ESP_LOGW(TAG, "🧹 中止OTA更新");
        ota_manager_abort();
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ret;
}

/**
 * 从云服务器获取指令
 */
int cloud_client_get_commands(cloud_command_t* commands, int max_commands)
{
    // 这个功能通过状态上报的响应来实现
    // 解析s_response_buffer中的指令
    if (s_response_len == 0 || !commands || max_commands <= 0) {
        return 0;
    }

    cJSON *json = cJSON_Parse(s_response_buffer);
    if (!json) {
        return 0;
    }

    cJSON *commands_array = cJSON_GetObjectItem(json, "commands");
    if (!cJSON_IsArray(commands_array)) {
        cJSON_Delete(json);
        return 0;
    }

    int count = 0;
    int array_size = cJSON_GetArraySize(commands_array);

    for (int i = 0; i < array_size && count < max_commands; i++) {
        cJSON *cmd_obj = cJSON_GetArrayItem(commands_array, i);
        if (!cmd_obj) continue;

        cJSON *id_obj = cJSON_GetObjectItem(cmd_obj, "id");
        cJSON *command_obj = cJSON_GetObjectItem(cmd_obj, "command");
        cJSON *data_obj = cJSON_GetObjectItem(cmd_obj, "data");
        cJSON *timestamp_obj = cJSON_GetObjectItem(cmd_obj, "timestamp");

        if (id_obj && command_obj) {
            // 处理指令ID（可能是字符串或数字）
            if (cJSON_IsString(id_obj)) {
                const char* id_str = cJSON_GetStringValue(id_obj);
                strncpy(s_current_command_id, id_str, sizeof(s_current_command_id) - 1);
                s_current_command_id[sizeof(s_current_command_id) - 1] = '\0';
                commands[count].id = 0; // 设置为0，因为我们使用字符串ID
            } else {
                commands[count].id = (uint32_t)cJSON_GetNumberValue(id_obj);
                snprintf(s_current_command_id, sizeof(s_current_command_id), "%lu", (unsigned long)commands[count].id);
            }

            // 解析指令类型
            const char* cmd_str = cJSON_GetStringValue(command_obj);
            commands[count].command = parse_command_type(cmd_str);

            // 处理指令数据
            if (data_obj) {
                char *data_str = cJSON_Print(data_obj);
                if (data_str) {
                    strncpy(commands[count].data, data_str, sizeof(commands[count].data) - 1);
                    commands[count].data[sizeof(commands[count].data) - 1] = '\0';
                    cJSON_free(data_str);
                }
            }

            if (timestamp_obj) {
                commands[count].timestamp = (uint32_t)cJSON_GetNumberValue(timestamp_obj);
            }

            // 立即处理OTA指令
            if (commands[count].command == CLOUD_CMD_OTA_UPDATE) {
#if !ENABLE_CLOUD_OTA
                ESP_LOGW(TAG, "⚠️ 云端OTA已禁用，忽略升级指令");
                cloud_client_send_command_feedback(s_current_command_id, "failed", "Cloud OTA disabled");
                continue;
#endif
                ESP_LOGI(TAG, "🚀 收到OTA升级指令，立即处理");
                ESP_LOGI(TAG, "📋 指令ID: %s", s_current_command_id);

                // 发送指令接收确认
                cloud_client_send_command_feedback(s_current_command_id, "received", "OTA指令已接收，开始处理");

                // 设置OTA进度回调
                ota_manager_set_progress_callback(ota_progress_callback);

                esp_err_t ota_ret = handle_ota_command(data_obj);
                if (ota_ret != ESP_OK) {
                    ESP_LOGE(TAG, "❌ OTA升级处理失败: %s", esp_err_to_name(ota_ret));

                    // 发送失败反馈
                    char error_msg[128];
                    snprintf(error_msg, sizeof(error_msg), "OTA升级失败: %s", esp_err_to_name(ota_ret));
                    cloud_client_send_command_feedback(s_current_command_id, "failed", error_msg);
                } else {
                    ESP_LOGI(TAG, "✅ OTA升级处理成功");
                    // 成功的反馈会在重启前发送
                }
                // OTA指令不返回给调用者，因为它会导致重启
                continue;
            }

            count++;
        }
    }

    cJSON_Delete(json);
    return count;
}

/**
 * 设置指令处理回调函数
 */
void cloud_client_set_command_callback(void (*callback)(const cloud_command_t* command))
{
    s_command_callback = callback;
}

/**
 * 检查云客户端是否已连接
 */
bool cloud_client_is_connected(void)
{
    return s_client_connected;
}

/**
 * 获取设备信息
 */
const cloud_device_info_t* cloud_client_get_device_info(void)
{
    return &s_device_info;
}

/**
 * 设置错误信息
 */
static void set_last_error(const char* error_msg)
{
    if (error_msg) {
        strncpy(s_last_error, error_msg, sizeof(s_last_error) - 1);
        s_last_error[sizeof(s_last_error) - 1] = '\0';
        ESP_LOGE(TAG, "❌ %s", error_msg);
    }
}

/**
 * 创建HTTP客户端配置（支持HTTPS）
 */
static esp_http_client_handle_t create_http_client(const char* url)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .buffer_size = MAX_HTTP_RESPONSE_SIZE,
        .buffer_size_tx = 2048,
        .event_handler = http_event_handler,
        .user_data = s_response_buffer,
        .skip_cert_common_name_check = true,  // 跳过证书验证（开发环境）
    };

    return esp_http_client_init(&config);
}

/**
 * 添加认证头部
 */
static esp_err_t add_auth_headers(esp_http_client_handle_t client)
{
    esp_err_t ret = ESP_OK;

    // 添加Content-Type
    ret = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (ret != ESP_OK) return ret;

    // 添加Supabase认证头部
    ret = esp_http_client_set_header(client, "apikey", SUPABASE_ANON_KEY);
    if (ret != ESP_OK) return ret;

    ret = esp_http_client_set_header(client, "Authorization", "Bearer " SUPABASE_ANON_KEY);
    if (ret != ESP_OK) return ret;

    // 如果启用了设备认证，添加自定义头部
    if (s_auth_enabled && strlen(s_device_key) > 0) {
        ret = esp_http_client_set_header(client, "X-Device-Key", s_device_key);
    }

    return ret;
}

/**
 * 发送完整设备状态到Supabase
 */
esp_err_t cloud_client_send_device_status(const device_status_data_t* status_data)
{
    if (!status_data) {
        set_last_error("状态数据为空");
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_manager_is_connected()) {
        set_last_error("Wi-Fi未连接");
        s_network_status = NETWORK_DISCONNECTED;
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    ESP_LOGD(TAG, "📤 开始发送设备状态到Supabase...");
    s_network_status = NETWORK_CONNECTING;

    // 创建JSON数据
    ESP_LOGD(TAG, "📝 构建状态JSON数据...");
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "❌ 创建状态JSON对象失败");
        set_last_error("创建JSON对象失败");
        s_network_status = NETWORK_ERROR;
        return ESP_ERR_NO_MEM;
    }

    // 添加设备ID
    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);

    // 添加状态数据
    cJSON_AddBoolToObject(json, "sbus_connected", status_data->sbus_connected);
    cJSON_AddBoolToObject(json, "can_connected", status_data->can_connected);
    cJSON_AddBoolToObject(json, "wifi_connected", status_data->wifi_connected);
    cJSON_AddStringToObject(json, "wifi_ip", status_data->wifi_ip);
    cJSON_AddNumberToObject(json, "wifi_rssi", status_data->wifi_rssi);
    cJSON_AddNumberToObject(json, "free_heap", status_data->free_heap);
    cJSON_AddNumberToObject(json, "total_heap", status_data->total_heap);
    cJSON_AddNumberToObject(json, "uptime_seconds", status_data->uptime_seconds);
    cJSON_AddNumberToObject(json, "task_count", status_data->task_count);
    cJSON_AddNumberToObject(json, "can_tx_count", status_data->can_tx_count);
    cJSON_AddNumberToObject(json, "can_rx_count", status_data->can_rx_count);
    cJSON_AddNumberToObject(json, "motor_left_speed", status_data->motor_left_speed);
    cJSON_AddNumberToObject(json, "motor_right_speed", status_data->motor_right_speed);
    cJSON_AddNumberToObject(json, "last_sbus_time", status_data->last_sbus_time);
    cJSON_AddNumberToObject(json, "last_cmd_time", status_data->last_cmd_time);

    ESP_LOGD(TAG, "📊 状态数据摘要 - 堆内存: %lu/%lu, 运行时间: %lus, 任务数: %d",
             (unsigned long)status_data->free_heap, (unsigned long)status_data->total_heap,
             (unsigned long)status_data->uptime_seconds, status_data->task_count);

    // 添加SBUS通道数组
    cJSON *channels = cJSON_CreateArray();
    if (channels) {
        for (int i = 0; i < 16; i++) {
            cJSON_AddItemToArray(channels, cJSON_CreateNumber(status_data->sbus_channels[i]));
        }
        cJSON_AddItemToObject(json, "sbus_channels", channels);
        ESP_LOGD(TAG, "📡 SBUS通道数据已添加 (16通道)");
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        ESP_LOGE(TAG, "❌ 序列化状态JSON失败");
        set_last_error("序列化JSON失败");
        s_network_status = NETWORK_ERROR;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "📏 JSON数据大小: %d字节", strlen(json_string));

    // 发送HTTP请求
    char url[256];
    snprintf(url, sizeof(url), "%s/device-status", CLOUD_SERVER_URL);
    ESP_LOGD(TAG, "🌐 发送POST请求到: %s", url);

    esp_http_client_handle_t client = create_http_client(url);
    if (!client) {
        cJSON_free(json_string);
        ESP_LOGE(TAG, "❌ 创建HTTP客户端失败");
        set_last_error("创建HTTP客户端失败");
        s_network_status = NETWORK_ERROR;
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    // 设置请求方法和头部
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    ret = add_auth_headers(client);

    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "🔐 HTTP头部设置成功");

        // 发送请求
        ret = esp_http_client_set_post_field(client, json_string, strlen(json_string));
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "📤 开始执行HTTP请求...");
            ret = esp_http_client_perform(client);

            if (ret == ESP_OK) {
                int status_code = esp_http_client_get_status_code(client);
                int content_length = esp_http_client_get_content_length(client);

                ESP_LOGD(TAG, "📥 HTTP响应 - 状态码: %d, 内容长度: %d", status_code, content_length);

                if (status_code == 200) {
                    s_network_status = NETWORK_CONNECTED;
                    s_retry_count = 0;
                    ESP_LOGD(TAG, "✅ 设备状态上报成功");

                    // 处理响应中的指令
                    cloud_command_t commands[MAX_COMMANDS_PER_REQUEST];
                    int command_count = cloud_client_get_commands(commands, MAX_COMMANDS_PER_REQUEST);

                    if (command_count > 0) {
                        ESP_LOGI(TAG, "📤 收到 %d 个指令，开始处理", command_count);

                        // 处理每个指令
                        for (int i = 0; i < command_count; i++) {
                            if (s_command_callback) {
                                s_command_callback(&commands[i]);
                            }
                        }
                    }

                    // 调用状态回调
                    if (s_status_callback) {
                        s_status_callback(status_data);
                    }
                } else {
                    s_network_status = NETWORK_ERROR;
                    snprintf(s_last_error, sizeof(s_last_error), "HTTP错误: %d", status_code);
                    ESP_LOGW(TAG, "⚠️ HTTP状态码错误: %d", status_code);
                    ret = ESP_FAIL;
                }
            } else {
                s_network_status = NETWORK_ERROR;
                ESP_LOGE(TAG, "❌ HTTP请求执行失败: %s", esp_err_to_name(ret));
                set_last_error("HTTP请求失败");
            }
        } else {
            ESP_LOGE(TAG, "❌ 设置POST数据失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ 设置HTTP头部失败: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    cJSON_free(json_string);

    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "🎉 状态上报流程完成");
    } else {
        ESP_LOGW(TAG, "⚠️ 状态上报流程失败");
    }

    return ret;
}

/**
 * 获取网络连接状态
 */
network_status_t cloud_client_get_network_status(void)
{
    return s_network_status;
}

/**
 * 设置设备认证信息
 */
esp_err_t cloud_client_set_auth(const char* device_key)
{
    if (!device_key) {
        s_auth_enabled = false;
        memset(s_device_key, 0, sizeof(s_device_key));
        return ESP_OK;
    }

    strncpy(s_device_key, device_key, sizeof(s_device_key) - 1);
    s_device_key[sizeof(s_device_key) - 1] = '\0';
    s_auth_enabled = true;

    ESP_LOGI(TAG, "🔐 设备认证已启用");
    return ESP_OK;
}

/**
 * 执行网络重连
 */
esp_err_t cloud_client_reconnect(void)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 检查重连间隔
    if (current_time - s_last_retry_time < RETRY_DELAY_MS) {
        return ESP_ERR_TIMEOUT;
    }

    s_last_retry_time = current_time;
    s_retry_count++;

    if (s_retry_count > MAX_RETRY_ATTEMPTS) {
        set_last_error("超过最大重连次数");
        s_network_status = NETWORK_ERROR;
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "🔄 执行网络重连 (第%" PRIu32 "次)", s_retry_count);

    // 重新注册设备
    esp_err_t ret = cloud_client_register_device(
        s_device_info.device_id,
        s_device_info.device_name,
        s_device_info.local_ip
    );

    if (ret == ESP_OK) {
        s_network_status = NETWORK_CONNECTED;
        s_retry_count = 0;
        ESP_LOGI(TAG, "✅ 网络重连成功");
    } else {
        s_network_status = NETWORK_ERROR;
        ESP_LOGW(TAG, "⚠️ 网络重连失败");
    }

    return ret;
}

/**
 * 获取最后错误信息
 */
const char* cloud_client_get_last_error(void)
{
    return s_last_error;
}

/**
 * 设置状态更新回调函数
 */
void cloud_client_set_status_callback(void (*callback)(const device_status_data_t* status))
{
    s_status_callback = callback;
}

/**
 * 增强的设备注册函数（支持更多设备信息）
 */
__attribute__((unused)) static esp_err_t register_device_enhanced(void)
{
    if (!wifi_manager_is_connected()) {
        set_last_error("Wi-Fi未连接");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    // 获取MAC地址
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_device_info.mac_address, sizeof(s_device_info.mac_address),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 设置固件和硬件版本
    strcpy(s_device_info.firmware_version, "2.1.0");
    strcpy(s_device_info.hardware_version, "v2.1");

    // 创建注册JSON数据
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        set_last_error("创建注册JSON失败");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "deviceName", s_device_info.device_name);
    cJSON_AddStringToObject(json, "localIP", s_device_info.local_ip);
    cJSON_AddStringToObject(json, "deviceType", s_device_info.device_type);
    cJSON_AddStringToObject(json, "firmwareVersion", s_device_info.firmware_version);
    cJSON_AddStringToObject(json, "hardwareVersion", s_device_info.hardware_version);
    cJSON_AddStringToObject(json, "macAddress", s_device_info.mac_address);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        set_last_error("序列化注册JSON失败");
        return ESP_ERR_NO_MEM;
    }

    // 发送注册请求
    esp_http_client_handle_t client = create_http_client(CLOUD_SERVER_URL "/register-device");
    if (!client) {
        cJSON_free(json_string);
        set_last_error("创建注册HTTP客户端失败");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    ret = add_auth_headers(client);

    if (ret == ESP_OK) {
        ret = esp_http_client_set_post_field(client, json_string, strlen(json_string));
        if (ret == ESP_OK) {
            ret = esp_http_client_perform(client);

            if (ret == ESP_OK) {
                int status_code = esp_http_client_get_status_code(client);
                if (status_code == 200) {
                    s_device_info.status = CLOUD_STATUS_ONLINE;
                    s_network_status = NETWORK_CONNECTED;
                    ESP_LOGI(TAG, "✅ 设备注册成功: %s", s_device_info.device_name);
                } else {
                    s_device_info.status = CLOUD_STATUS_ERROR;
                    s_network_status = NETWORK_ERROR;
                    snprintf(s_last_error, sizeof(s_last_error), "注册失败，HTTP状态: %d", status_code);
                    ret = ESP_FAIL;
                }
            } else {
                s_device_info.status = CLOUD_STATUS_ERROR;
                s_network_status = NETWORK_ERROR;
                set_last_error("注册HTTP请求失败");
            }
        }
    }

    esp_http_client_cleanup(client);
    cJSON_free(json_string);

    return ret;
}

/**
 * 注销设备从云服务器
 */
esp_err_t cloud_client_unregister_device(const char* reason)
{
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "⚠️ Wi-Fi未连接，无法注销设备");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    ESP_LOGI(TAG, "📤 开始注销设备: %s", s_device_info.device_id);
    ESP_LOGI(TAG, "📋 注销原因: %s", reason ? reason : "device_shutdown");

    // 创建注销JSON数据
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        set_last_error("创建注销JSON失败");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "reason", reason ? reason : "device_shutdown");

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        set_last_error("序列化注销JSON失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "📤 注销数据: %s", json_string);

    // 发送注销请求
    char url[256];
    snprintf(url, sizeof(url), "%s/unregister-device", CLOUD_SERVER_URL);

    esp_err_t ret = send_http_post(url, json_string);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 设备注销成功");
        s_device_info.status = CLOUD_STATUS_OFFLINE;
    } else {
        ESP_LOGE(TAG, "❌ 设备注销失败");
        set_last_error("设备注销HTTP请求失败");
    }

    cJSON_free(json_string);
    return ret;
}

/**
 * 优雅关闭云客户端
 */
esp_err_t cloud_client_graceful_shutdown(const char* reason)
{
    ESP_LOGI(TAG, "🛑 开始优雅关闭云客户端...");
    ESP_LOGI(TAG, "📋 关闭原因: %s", reason ? reason : "system_shutdown");

    esp_err_t ret = ESP_OK;

    // 如果客户端正在运行，先发送最后一次状态更新
    if (s_client_running && s_client_connected) {
        ESP_LOGI(TAG, "📊 发送最后一次状态更新...");

        // 收集当前状态
        device_status_data_t final_status = {0};
        final_status.wifi_connected = wifi_manager_is_connected();
        strncpy(final_status.wifi_ip, wifi_manager_get_ip_address(), sizeof(final_status.wifi_ip) - 1);
        final_status.wifi_rssi = wifi_manager_get_rssi();
        final_status.free_heap = esp_get_free_heap_size();
        final_status.total_heap = esp_get_minimum_free_heap_size();
        final_status.uptime_seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        final_status.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 发送最后状态
        cloud_client_send_device_status(&final_status);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 等待发送完成
    }

    // 注销设备
    if (s_client_connected) {
        ESP_LOGI(TAG, "📤 注销设备...");
        esp_err_t unregister_ret = cloud_client_unregister_device(reason);
        if (unregister_ret != ESP_OK) {
            ESP_LOGW(TAG, "⚠️ 设备注销失败，但继续关闭流程");
            ret = unregister_ret;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 等待注销完成
    }

    // 停止云客户端
    ESP_LOGI(TAG, "🛑 停止云客户端服务...");
    esp_err_t stop_ret = cloud_client_stop();
    if (stop_ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ 停止云客户端失败");
        ret = stop_ret;
    }

    ESP_LOGI(TAG, "✅ 云客户端优雅关闭完成");
    return ret;
}

/**
 * OTA进度回调函数
 */
static void ota_progress_callback(uint8_t progress_percent, const char* status_message)
{
    if (strlen(s_current_command_id) > 0) {
        cloud_client_send_ota_progress(s_current_command_id, progress_percent, status_message);
    }
}

/**
 * 发送指令执行状态反馈
 */
esp_err_t cloud_client_send_command_feedback(const char* command_id, const char* status, const char* message)
{
    if (!s_client_running || !wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "无法发送指令反馈：客户端未运行或网络未连接");
        return ESP_ERR_INVALID_STATE;
    }

    if (!command_id || !status) {
        ESP_LOGE(TAG, "指令ID和状态不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "📤 发送指令执行反馈: ID=%s, 状态=%s", command_id, status);

    // 创建JSON数据
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "创建JSON对象失败");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "commandId", command_id);
    cJSON_AddStringToObject(json, "status", status);
    if (message) {
        cJSON_AddStringToObject(json, "message", message);
    }
    cJSON_AddStringToObject(json, "timestamp", ""); // 服务器会设置时间戳

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        ESP_LOGE(TAG, "序列化指令反馈JSON失败");
        return ESP_ERR_NO_MEM;
    }

    // 发送HTTP POST请求
    char url[256];
    snprintf(url, sizeof(url), "%s/api/device-commands/feedback", CLOUD_SERVER_URL);

    esp_err_t err = send_http_post(url, json_string);
    cJSON_free(json_string);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ 指令反馈发送成功");
    } else {
        ESP_LOGW(TAG, "⚠️ 指令反馈发送失败: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * 发送OTA进度到云端
 */
esp_err_t cloud_client_send_ota_progress(const char* command_id, uint8_t progress, const char* message)
{
    if (!s_client_running) {
        ESP_LOGW(TAG, "云客户端未运行，跳过OTA进度上报");
        return ESP_ERR_INVALID_STATE;
    }

    // 检查WiFi连接状态而不是云客户端连接状态
    // 因为OTA过程中可能会有临时的连接问题
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi未连接，跳过OTA进度上报");
        return ESP_ERR_INVALID_STATE;
    }

    if (!command_id) {
        ESP_LOGE(TAG, "指令ID不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 创建JSON数据
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "创建JSON对象失败");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "commandId", command_id);
    cJSON_AddNumberToObject(json, "progress", progress);
    cJSON_AddStringToObject(json, "status", "in_progress");
    if (message) {
        cJSON_AddStringToObject(json, "message", message);
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_string) {
        ESP_LOGE(TAG, "序列化OTA进度JSON失败");
        return ESP_ERR_NO_MEM;
    }

    // 发送HTTP POST请求
    char url[256];
    snprintf(url, sizeof(url), "%s/api/firmware/ota-progress", CLOUD_SERVER_URL);

    ESP_LOGD(TAG, "📊 发送OTA进度: %d%% - %s", progress, message ? message : "");

    esp_err_t err = send_http_post(url, json_string);
    cJSON_free(json_string);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "发送OTA进度失败: %s", esp_err_to_name(err));
    }

    return err;
}
