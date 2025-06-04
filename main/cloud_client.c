#include "cloud_client.h"
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

// HTTP响应缓冲区
static char s_response_buffer[2048];
static int s_response_len = 0;

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
 * 状态上报任务
 */
static void status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态上报任务已启动");
    
    while (s_client_running) {
        if (wifi_manager_is_connected()) {
            // 构建状态数据
            cJSON *status_json = cJSON_CreateObject();
            cJSON *data_json = cJSON_CreateObject();
            
            // 添加设备基本信息
            cJSON_AddStringToObject(data_json, "firmware_version", "1.0.0");
            cJSON_AddStringToObject(data_json, "local_ip", wifi_manager_get_ip_address());
            cJSON_AddNumberToObject(data_json, "free_heap", esp_get_free_heap_size());
            cJSON_AddNumberToObject(data_json, "uptime", xTaskGetTickCount() / configTICK_RATE_HZ);
            
            cJSON_AddStringToObject(status_json, "deviceId", s_device_info.device_id);
            cJSON_AddStringToObject(status_json, "status", "online");
            cJSON_AddItemToObject(status_json, "data", data_json);
            
            char *json_string = cJSON_Print(status_json);
            if (json_string) {
                char url[256];
                snprintf(url, sizeof(url), "%s/device-status", CLOUD_SERVER_URL);
                
                esp_err_t err = send_http_post(url, json_string);
                if (err == ESP_OK) {
                    s_client_connected = true;
                    ESP_LOGD(TAG, "状态上报成功");
                } else {
                    s_client_connected = false;
                    ESP_LOGW(TAG, "状态上报失败");
                }
                
                free(json_string);
            }
            
            cJSON_Delete(status_json);
        } else {
            s_client_connected = false;
            ESP_LOGW(TAG, "Wi-Fi未连接，跳过状态上报");
        }
        
        vTaskDelay(pdMS_TO_TICKS(DEVICE_STATUS_INTERVAL_MS));
    }
    
    ESP_LOGI(TAG, "状态上报任务已停止");
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
    
    // 生成设备ID
    generate_device_id(s_device_info.device_id, sizeof(s_device_info.device_id));
    
    // 设置默认设备信息
    snprintf(s_device_info.device_name, sizeof(s_device_info.device_name), "ESP32控制板-%s", 
             s_device_info.device_id + 6); // 跳过"esp32-"前缀
    strcpy(s_device_info.device_type, "ESP32");
    s_device_info.status = CLOUD_STATUS_OFFLINE;
    
    ESP_LOGI(TAG, "✅ 云客户端初始化完成，设备ID: %s", s_device_info.device_id);
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
    s_client_running = true;
    s_client_connected = false;
    
    // 创建状态上报任务
    BaseType_t ret = xTaskCreate(status_task, "cloud_status", 4096, NULL, 5, &s_status_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建状态上报任务失败");
        s_client_running = false;
        return ESP_FAIL;
    }
    
    // 创建指令轮询任务
    ret = xTaskCreate(command_task, "cloud_command", 4096, NULL, 5, &s_command_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建指令轮询任务失败");
        s_client_running = false;
        if (s_status_task_handle) {
            vTaskDelete(s_status_task_handle);
            s_status_task_handle = NULL;
        }
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ 云客户端启动成功");
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
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📡 注册设备到云服务器...");

    // 更新设备信息
    if (device_id) {
        strncpy(s_device_info.device_id, device_id, sizeof(s_device_info.device_id) - 1);
    }
    if (device_name) {
        strncpy(s_device_info.device_name, device_name, sizeof(s_device_info.device_name) - 1);
    }
    if (local_ip) {
        strncpy(s_device_info.local_ip, local_ip, sizeof(s_device_info.local_ip) - 1);
    } else {
        strncpy(s_device_info.local_ip, wifi_manager_get_ip_address(), sizeof(s_device_info.local_ip) - 1);
    }

    // 构建注册数据
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "deviceId", s_device_info.device_id);
    cJSON_AddStringToObject(json, "deviceName", s_device_info.device_name);
    cJSON_AddStringToObject(json, "localIP", s_device_info.local_ip);
    cJSON_AddStringToObject(json, "deviceType", s_device_info.device_type);

    char *json_string = cJSON_Print(json);
    esp_err_t err = ESP_FAIL;

    if (json_string) {
        char url[256];
        snprintf(url, sizeof(url), "%s/register-device", CLOUD_SERVER_URL);

        err = send_http_post(url, json_string);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✅ 设备注册成功");
            s_device_info.status = CLOUD_STATUS_ONLINE;
        } else {
            ESP_LOGE(TAG, "❌ 设备注册失败");
            s_device_info.status = CLOUD_STATUS_ERROR;
        }

        free(json_string);
    }

    cJSON_Delete(json);
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
