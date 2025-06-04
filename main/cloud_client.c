#include "cloud_client.h"
#include "data_integration.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "CLOUD_CLIENT";

// 全局变量
static device_info_t s_device_info = {0};
static bool s_client_running = false;
static bool s_client_connected = false;
static TaskHandle_t s_status_task_handle = NULL;
static TaskHandle_t s_command_task_handle = NULL;
static void (*s_command_callback)(const cloud_command_t* command) = NULL;
static void (*s_status_callback)(const device_status_data_t* status) = NULL;

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
    // 使用数据集成模块收集状态
    return data_integration_collect_status(status);
}

/**
 * 状态上报任务
 */
static void status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "📊 状态上报任务已启动");
    ESP_LOGI(TAG, "⏰ 上报间隔: %d秒", DEVICE_STATUS_INTERVAL_MS / 1000);

    device_status_data_t status_data;
    uint32_t report_count = 0;
    uint32_t success_count = 0;
    uint32_t error_count = 0;

    while (s_client_running) {
        if (wifi_manager_is_connected()) {
            ESP_LOGD(TAG, "🔄 开始第%d次状态收集...", report_count + 1);

            // 收集设备状态
            esp_err_t ret = collect_current_status(&status_data);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "📊 状态数据收集成功 - 堆内存: %d, 运行时间: %ds, WiFi: %s",
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

                    ESP_LOGI(TAG, "✅ 状态上报成功 [%d/%d] - 成功率: %.1f%%",
                             success_count, report_count + 1,
                             (float)success_count / (report_count + 1) * 100);
                } else {
                    s_client_connected = false;
                    s_device_info.network_status = NETWORK_ERROR;
                    error_count++;

                    ESP_LOGW(TAG, "⚠️ 状态上报失败 [%d/%d]: %s",
                             error_count, report_count + 1,
                             cloud_client_get_last_error());

                    // 尝试重连
                    if (s_retry_count < MAX_RETRY_ATTEMPTS) {
                        ESP_LOGI(TAG, "🔄 尝试重连 (第%d次)...", s_retry_count + 1);
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

        // 等待下次上报
        ESP_LOGD(TAG, "⏳ 等待%d秒后进行下次上报...", DEVICE_STATUS_INTERVAL_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(DEVICE_STATUS_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "📊 状态上报任务已停止");
    ESP_LOGI(TAG, "📈 统计信息 - 总计: %d, 成功: %d, 失败: %d",
             report_count, success_count, error_count);

    s_status_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * 指令轮询任务
 */
static void command_task(void *pvParameters)
{
    ESP_LOGI(TAG, "指令轮询任务已启动");
    
    while (s_client_running) {
        if (wifi_manager_is_connected() && s_client_connected) {
            // 通过状态上报接口获取指令（复用现有机制）
            // 指令会在状态上报的响应中返回
            ESP_LOGD(TAG, "指令轮询中...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(COMMAND_POLL_INTERVAL_MS));
    }
    
    ESP_LOGI(TAG, "指令轮询任务已停止");
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
    snprintf(s_device_info.device_name, sizeof(s_device_info.device_name), "ESP32控制板-%s",
             s_device_info.device_id + 6); // 跳过"esp32-"前缀
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

    // 创建状态上报任务
    ESP_LOGI(TAG, "📊 创建状态上报任务 (栈大小: 4096, 优先级: 5)");
    BaseType_t ret = xTaskCreate(status_task, "cloud_status", 4096, NULL, 5, &s_status_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建状态上报任务失败");
        s_client_running = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ 状态上报任务创建成功");

    // 创建指令轮询任务
    ESP_LOGI(TAG, "📊 创建指令轮询任务 (栈大小: 4096, 优先级: 5)");
    ret = xTaskCreate(command_task, "cloud_command", 4096, NULL, 5, &s_command_task_handle);
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
        } else {
            ESP_LOGE(TAG, "❌ 设备注册失败，HTTP错误");
            s_device_info.status = CLOUD_STATUS_ERROR;
            s_network_status = NETWORK_ERROR;
            set_last_error("设备注册HTTP请求失败");
        }

        free(json_string);
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
        free(json_string);
    }

    cJSON_Delete(json);
    return err;
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
            commands[count].id = (uint32_t)cJSON_GetNumberValue(id_obj);
            commands[count].command = CLOUD_CMD_UNKNOWN; // 需要解析command字符串

            if (data_obj) {
                char *data_str = cJSON_Print(data_obj);
                if (data_str) {
                    strncpy(commands[count].data, data_str, sizeof(commands[count].data) - 1);
                    free(data_str);
                }
            }

            if (timestamp_obj) {
                commands[count].timestamp = (uint32_t)cJSON_GetNumberValue(timestamp_obj);
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
const device_info_t* cloud_client_get_device_info(void)
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

    ESP_LOGD(TAG, "📊 状态数据摘要 - 堆内存: %d/%d, 运行时间: %ds, 任务数: %d",
             status_data->free_heap, status_data->total_heap,
             status_data->uptime_seconds, status_data->task_count);

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
        free(json_string);
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
    free(json_string);

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

    ESP_LOGI(TAG, "🔄 执行网络重连 (第%d次)", s_retry_count);

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
static esp_err_t register_device_enhanced(void)
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
        free(json_string);
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
    free(json_string);

    return ret;
}
