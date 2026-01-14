#include "http_server.h"
#include "ota_manager.h"
#include "wifi_manager.h"
#include "main.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_app_desc.h"
#include "cJSON.h"
#include <string.h>
#include <inttypes.h>
#include "esp_mac.h"

static const char *TAG = "HTTP_SRV";

// HTTP服务器句柄
static httpd_handle_t s_server = NULL;

// 回调函数指针
static bool (*s_sbus_callback)(uint16_t* channels) = NULL;
static bool (*s_motor_callback)(int8_t* left, int8_t* right) = NULL;

/**
 * 发送JSON响应
 */
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json, int status_code)
{
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 设置响应头
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    
    // 设置状态码
    if (status_code == 200) {
        httpd_resp_set_status(req, "200 OK");
    } else if (status_code == 400) {
        httpd_resp_set_status(req, "400 Bad Request");
    } else if (status_code == 403) {
        httpd_resp_set_status(req, "403 Forbidden");
    } else if (status_code == 500) {
        httpd_resp_set_status(req, "500 Internal Server Error");
    } else {
        httpd_resp_set_status(req, "200 OK");  // 默认状态
    }

    // 发送响应
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    return ret;
}

static esp_err_t send_ota_disabled_response(httpd_req_t *req)
{
    ESP_LOGW(TAG, "⚠️ OTA endpoint disabled");
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(json, "status", "error");
    cJSON_AddStringToObject(json, "message", "OTA disabled");
    esp_err_t ret = send_json_response(req, json, 403);
    cJSON_Delete(json);
    return ret;
}

/**
 * 处理OPTIONS请求（CORS预检）
 */
static esp_err_t options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * 获取设备信息API处理函数
 */
static esp_err_t device_info_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "📱 Device info requested");

    device_info_t device_info;
    if (http_server_get_device_info(&device_info) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddStringToObject(data, "device_name", device_info.device_name);
    cJSON_AddStringToObject(data, "firmware_version", device_info.firmware_version);
    cJSON_AddStringToObject(data, "hardware_version", device_info.hardware_version);
    cJSON_AddStringToObject(data, "chip_model", device_info.chip_model);
    cJSON_AddNumberToObject(data, "flash_size", device_info.flash_size);
    cJSON_AddNumberToObject(data, "free_heap", device_info.free_heap);
    cJSON_AddNumberToObject(data, "uptime_seconds", device_info.uptime_seconds);
    cJSON_AddStringToObject(data, "mac_address", device_info.mac_address);

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * 运行时间处理函数 - 轻量级API
 */
static esp_err_t device_uptime_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "⏱️ Device uptime requested");

    uint32_t uptime_seconds = xTaskGetTickCount() / configTICK_RATE_HZ;

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddNumberToObject(data, "uptime_seconds", uptime_seconds);
    cJSON_AddNumberToObject(data, "timestamp", uptime_seconds); // 为兼容性保留

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * 获取设备状态API处理函数
 */
static esp_err_t device_status_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "📊 Device status requested");

    device_status_t status;
    if (http_server_get_device_status(&status) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddBoolToObject(data, "sbus_connected", status.sbus_connected);
    cJSON_AddBoolToObject(data, "can_connected", status.can_connected);
    cJSON_AddBoolToObject(data, "wifi_connected", status.wifi_connected);
    cJSON_AddStringToObject(data, "wifi_ip", status.wifi_ip);
    cJSON_AddNumberToObject(data, "wifi_rssi", status.wifi_rssi);
    cJSON_AddNumberToObject(data, "motor_left_speed", status.motor_left_speed);
    cJSON_AddNumberToObject(data, "motor_right_speed", status.motor_right_speed);
    cJSON_AddNumberToObject(data, "last_sbus_time", status.last_sbus_time);
    cJSON_AddNumberToObject(data, "last_cmd_time", status.last_cmd_time);

    // 添加SBUS通道数组
    cJSON *channels = cJSON_CreateArray();
    for (int i = 0; i < 16; i++) {
        cJSON_AddItemToArray(channels, cJSON_CreateNumber(status.sbus_channels[i]));
    }
    cJSON_AddItemToObject(data, "sbus_channels", channels);

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * 系统健康检查API处理函数
 */
static esp_err_t device_health_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "🩺 System health check requested");

    system_health_t health;
    if (http_server_get_system_health(&health) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddNumberToObject(data, "uptime_seconds", health.uptime_seconds);
    cJSON_AddNumberToObject(data, "free_heap", health.free_heap);
    cJSON_AddNumberToObject(data, "min_free_heap", health.min_free_heap);
    cJSON_AddNumberToObject(data, "cpu_usage_percent", health.cpu_usage_percent);
    cJSON_AddNumberToObject(data, "cpu_temperature", health.cpu_temperature);
    cJSON_AddBoolToObject(data, "watchdog_triggered", health.watchdog_triggered);
    cJSON_AddNumberToObject(data, "task_count", health.task_count);
    cJSON_AddBoolToObject(data, "wifi_healthy", health.wifi_healthy);
    cJSON_AddBoolToObject(data, "sbus_healthy", health.sbus_healthy);
    cJSON_AddBoolToObject(data, "motor_healthy", health.motor_healthy);

    // 计算整体健康评分
    int health_score = 100;
    if (!health.wifi_healthy) health_score -= 20;
    if (!health.sbus_healthy) health_score -= 30;
    if (!health.motor_healthy) health_score -= 30;
    if (health.free_heap < 50000) health_score -= 10;  // 内存不足
    if (health.cpu_usage_percent > 80) health_score -= 10;  // CPU使用率过高

    cJSON_AddNumberToObject(data, "health_score", health_score);
    cJSON_AddStringToObject(data, "health_status", 
                          health_score >= 80 ? "excellent" :
                          health_score >= 60 ? "good" :
                          health_score >= 40 ? "warning" : "critical");

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * OTA上传处理函数
 */
static esp_err_t ota_upload_handler(httpd_req_t *req)
{
#if !ENABLE_HTTP_OTA
    return send_ota_disabled_response(req);
#endif
    ESP_LOGI(TAG, "📦 OTA upload started, content length: %d", req->content_len);

    if (req->content_len <= 0) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "status", "error");
        cJSON_AddStringToObject(json, "message", "No content provided");
        esp_err_t ret = send_json_response(req, json, 400);
        cJSON_Delete(json);
        return ret;
    }

    // 开始OTA更新
    esp_err_t ota_ret = ota_manager_begin(req->content_len);
    if (ota_ret != ESP_OK) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "status", "error");
        cJSON_AddStringToObject(json, "message", "Failed to start OTA update");
        esp_err_t ret = send_json_response(req, json, 400);
        cJSON_Delete(json);
        return ret;
    }

    // 接收并写入固件数据
    char *buffer = malloc(HTTP_UPLOAD_CHUNK_SIZE);
    if (buffer == NULL) {
        ota_manager_abort();
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buffer,
                                     remaining > HTTP_UPLOAD_CHUNK_SIZE ?
                                     HTTP_UPLOAD_CHUNK_SIZE : remaining);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "❌ Failed to receive OTA data");
            break;
        }

        // 写入OTA数据
        ota_ret = ota_manager_write(buffer, recv_len);
        if (ota_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to write OTA data");
            break;
        }

        remaining -= recv_len;
        ESP_LOGI(TAG, "📥 OTA progress: %d/%d bytes",
                req->content_len - remaining, req->content_len);
    }

    free(buffer);

    // 完成OTA更新
    if (remaining == 0 && ota_ret == ESP_OK) {
        ota_ret = ota_manager_end();
        if (ota_ret == ESP_OK) {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "status", "success");
            cJSON_AddStringToObject(json, "message", "OTA update completed successfully");
            esp_err_t ret = send_json_response(req, json, 200);
            cJSON_Delete(json);

            ESP_LOGI(TAG, "✅ OTA update completed, restarting in 3 seconds...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
            return ret;
        }
    }

    // OTA失败
    ota_manager_abort();
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "error");
    cJSON_AddStringToObject(json, "message", "OTA update failed");
    esp_err_t ret = send_json_response(req, json, 400);
    cJSON_Delete(json);
    return ret;
}

/**
 * OTA进度查询处理函数
 */
static esp_err_t ota_progress_handler(httpd_req_t *req)
{
#if !ENABLE_HTTP_OTA
    return send_ota_disabled_response(req);
#endif
    ota_progress_t progress;
    if (http_server_get_ota_progress(&progress) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddBoolToObject(data, "in_progress", progress.in_progress);
    cJSON_AddNumberToObject(data, "total_size", progress.total_size);
    cJSON_AddNumberToObject(data, "written_size", progress.written_size);
    cJSON_AddNumberToObject(data, "progress_percent", progress.progress_percent);
    cJSON_AddStringToObject(data, "status_message", progress.status_message);
    cJSON_AddBoolToObject(data, "success", progress.success);
    if (strlen(progress.error_message) > 0) {
        cJSON_AddStringToObject(data, "error_message", progress.error_message);
    }

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * OTA开始处理函数
 */
static esp_err_t ota_start_handler(httpd_req_t *req)
{
#if !ENABLE_HTTP_OTA
    return send_ota_disabled_response(req);
#endif
    ESP_LOGI(TAG, "🚀 OTA start request received");

    // 解析请求体
    char *buffer = malloc(req->content_len + 1);
    if (buffer == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, buffer, req->content_len);
    if (ret <= 0) {
        free(buffer);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buffer[req->content_len] = '\0';

    // 解析JSON
    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (json == NULL) {
        cJSON *error_json = cJSON_CreateObject();
        cJSON_AddStringToObject(error_json, "status", "error");
        cJSON_AddStringToObject(error_json, "message", "Invalid JSON format");
        esp_err_t send_ret = send_json_response(req, error_json, 400);
        cJSON_Delete(error_json);
        return send_ret;
    }

    // 获取固件大小
    cJSON *size_item = cJSON_GetObjectItem(json, "firmware_size");
    if (!cJSON_IsNumber(size_item)) {
        cJSON_Delete(json);
        cJSON *error_json = cJSON_CreateObject();
        cJSON_AddStringToObject(error_json, "status", "error");
        cJSON_AddStringToObject(error_json, "message", "Missing or invalid firmware_size");
        esp_err_t send_ret = send_json_response(req, error_json, 400);
        cJSON_Delete(error_json);
        return send_ret;
    }

    uint32_t firmware_size = (uint32_t)cJSON_GetNumberValue(size_item);
    cJSON_Delete(json);

    // 开始OTA更新
    esp_err_t ota_ret = ota_manager_begin(firmware_size);

    cJSON *response_json = cJSON_CreateObject();
    if (ota_ret == ESP_OK) {
        cJSON_AddStringToObject(response_json, "status", "success");
        cJSON_AddStringToObject(response_json, "message", "OTA update started");
        ESP_LOGI(TAG, "✅ OTA update started, firmware size: %lu bytes", (unsigned long)firmware_size);
    } else {
        cJSON_AddStringToObject(response_json, "status", "error");
        cJSON_AddStringToObject(response_json, "message", "Failed to start OTA update");
        ESP_LOGE(TAG, "❌ Failed to start OTA update: %s", esp_err_to_name(ota_ret));
    }

    esp_err_t send_ret = send_json_response(req, response_json, ota_ret == ESP_OK ? 200 : 400);
    cJSON_Delete(response_json);
    return send_ret;
}

/**
 * OTA回滚处理函数
 */
static esp_err_t ota_rollback_handler(httpd_req_t *req)
{
#if !ENABLE_HTTP_OTA
    return send_ota_disabled_response(req);
#endif
    ESP_LOGI(TAG, "🔄 OTA rollback request received");

    // 执行回滚操作
    esp_err_t rollback_ret = ota_manager_rollback();

    cJSON *json = cJSON_CreateObject();
    if (rollback_ret == ESP_OK) {
        cJSON_AddStringToObject(json, "status", "success");
        cJSON_AddStringToObject(json, "message", "Rollback initiated, system will restart");
        ESP_LOGI(TAG, "✅ OTA rollback initiated");
    } else {
        cJSON_AddStringToObject(json, "status", "error");
        cJSON_AddStringToObject(json, "message", "Failed to initiate rollback");
        ESP_LOGE(TAG, "❌ Failed to initiate rollback: %s", esp_err_to_name(rollback_ret));
    }

    esp_err_t ret = send_json_response(req, json, rollback_ret == ESP_OK ? 200 : 400);
    cJSON_Delete(json);

    // 如果回滚成功，系统会重启，这里的代码不会执行
    return ret;
}

/**
 * OTA信息查询处理函数
 */
static esp_err_t ota_info_handler(httpd_req_t *req)
{
#if !ENABLE_HTTP_OTA
    return send_ota_disabled_response(req);
#endif
    ESP_LOGI(TAG, "📋 OTA info request received");

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    // 获取当前运行分区信息
    const esp_partition_t* running_partition = ota_manager_get_running_partition();
    if (running_partition) {
        cJSON_AddStringToObject(data, "running_partition", running_partition->label);
        cJSON_AddNumberToObject(data, "running_partition_size", running_partition->size);
        cJSON_AddNumberToObject(data, "running_partition_address", running_partition->address);
    }

    // 获取下一个OTA分区信息
    const esp_partition_t* next_partition = ota_manager_get_next_partition();
    if (next_partition) {
        cJSON_AddStringToObject(data, "next_partition", next_partition->label);
        cJSON_AddNumberToObject(data, "next_partition_size", next_partition->size);
        cJSON_AddNumberToObject(data, "next_partition_address", next_partition->address);
    }

    // 获取固件版本信息
    char version_buffer[64];
    if (ota_manager_get_version(version_buffer, sizeof(version_buffer)) == ESP_OK) {
        cJSON_AddStringToObject(data, "firmware_version", version_buffer);
    }

    // 检查是否需要回滚
    cJSON_AddBoolToObject(data, "rollback_required", ota_manager_check_rollback_required());

    // 获取分区表信息
    esp_partition_t partition_info[8];
    uint8_t partition_count = ota_manager_get_partition_info(partition_info, 8);
    cJSON *partitions = cJSON_CreateArray();
    for (uint8_t i = 0; i < partition_count; i++) {
        cJSON *partition = cJSON_CreateObject();
        cJSON_AddStringToObject(partition, "label", partition_info[i].label);
        cJSON_AddNumberToObject(partition, "type", partition_info[i].type);
        cJSON_AddNumberToObject(partition, "subtype", partition_info[i].subtype);
        cJSON_AddNumberToObject(partition, "address", partition_info[i].address);
        cJSON_AddNumberToObject(partition, "size", partition_info[i].size);
        cJSON_AddItemToArray(partitions, partition);
    }
    cJSON_AddItemToObject(data, "partitions", partitions);

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * Wi-Fi状态查询处理函数
 */
static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    wifi_status_t wifi_status;
    if (wifi_manager_get_status(&wifi_status) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    const char* state_str = "unknown";
    switch (wifi_status.state) {
        case WIFI_STATE_DISCONNECTED: state_str = "disconnected"; break;
        case WIFI_STATE_CONNECTING: state_str = "connecting"; break;
        case WIFI_STATE_CONNECTED: state_str = "connected"; break;
        case WIFI_STATE_FAILED: state_str = "failed"; break;
    }

    cJSON_AddStringToObject(data, "state", state_str);
    cJSON_AddStringToObject(data, "ip_address", wifi_status.ip_address);
    cJSON_AddNumberToObject(data, "rssi", wifi_status.rssi);
    cJSON_AddNumberToObject(data, "retry_count", wifi_status.retry_count);
    cJSON_AddNumberToObject(data, "connect_time", wifi_status.connect_time);

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * Wi-Fi连接处理函数
 */
static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "📡 Wi-Fi connect request");

    // 读取请求体
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "status", "error");
        cJSON_AddStringToObject(json, "message", "No content provided");
        esp_err_t resp_ret = send_json_response(req, json, 400);
        cJSON_Delete(json);
        return resp_ret;
    }
    content[ret] = '\0';

    // 解析JSON
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        cJSON *resp_json = cJSON_CreateObject();
        cJSON_AddStringToObject(resp_json, "status", "error");
        cJSON_AddStringToObject(resp_json, "message", "Invalid JSON");
        esp_err_t resp_ret = send_json_response(req, resp_json, 400);
        cJSON_Delete(resp_json);
        return resp_ret;
    }

    // 获取SSID和密码
    cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_item = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == NULL) {
        cJSON *resp_json = cJSON_CreateObject();
        cJSON_AddStringToObject(resp_json, "status", "error");
        cJSON_AddStringToObject(resp_json, "message", "SSID is required");
        esp_err_t resp_ret = send_json_response(req, resp_json, 400);
        cJSON_Delete(resp_json);
        cJSON_Delete(json);
        return resp_ret;
    }

    const char *ssid = ssid_item->valuestring;
    const char *password = cJSON_IsString(password_item) ? password_item->valuestring : "";

    // 尝试连接Wi-Fi
    esp_err_t wifi_ret = wifi_manager_connect(ssid, password);
    
    cJSON *resp_json = cJSON_CreateObject();
    if (wifi_ret == ESP_OK) {
        cJSON_AddStringToObject(resp_json, "status", "success");
        cJSON_AddStringToObject(resp_json, "message", "Connected to Wi-Fi");
    } else {
        cJSON_AddStringToObject(resp_json, "status", "error");
        cJSON_AddStringToObject(resp_json, "message", "Failed to connect to Wi-Fi");
    }

    esp_err_t resp_ret = send_json_response(req, resp_json, wifi_ret == ESP_OK ? 200 : 400);
    cJSON_Delete(resp_json);
    cJSON_Delete(json);
    return resp_ret;
}

/**
 * Wi-Fi扫描处理函数
 */
static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "📡 Wi-Fi scan request");

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();

    // 简化的Wi-Fi扫描结果（实际实现需要调用ESP32 Wi-Fi扫描API）
    // 这里提供一个示例结构，您可以根据需要实现完整的扫描功能
    cJSON *network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "ssid", "Example_Network");
    cJSON_AddNumberToObject(network, "rssi", -45);
    cJSON_AddStringToObject(network, "auth", "WPA2");
    cJSON_AddItemToArray(data, network);

    cJSON_AddStringToObject(json, "status", "success");
    cJSON_AddItemToObject(json, "data", data);
    cJSON_AddStringToObject(json, "message", "Wi-Fi scan completed");

    esp_err_t ret = send_json_response(req, json, 200);
    cJSON_Delete(json);
    return ret;
}

/**
 * 注册所有HTTP处理函数
 */
static esp_err_t register_handlers(httpd_handle_t server)
{
    // OPTIONS处理器（CORS预检）
    httpd_uri_t options_uri = {
        .uri = "/*",
        .method = HTTP_OPTIONS,
        .handler = options_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &options_uri);

    // 设备信息API
    httpd_uri_t device_info_uri = {
        .uri = API_DEVICE_INFO,
        .method = HTTP_GET,
        .handler = device_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_info_uri);

    // 设备状态API
    httpd_uri_t device_status_uri = {
        .uri = API_DEVICE_STATUS,
        .method = HTTP_GET,
        .handler = device_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_status_uri);

    // 系统健康检查API
    httpd_uri_t device_health_uri = {
        .uri = API_DEVICE_HEALTH,
        .method = HTTP_GET,
        .handler = device_health_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_health_uri);

    // 设备运行时间API（轻量级）
    httpd_uri_t device_uptime_uri = {
        .uri = API_DEVICE_UPTIME,
        .method = HTTP_GET,
        .handler = device_uptime_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_uptime_uri);

    // OTA上传API
    httpd_uri_t ota_upload_uri = {
        .uri = API_OTA_UPLOAD,
        .method = HTTP_POST,
        .handler = ota_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_upload_uri);

    // OTA进度API
    httpd_uri_t ota_progress_uri = {
        .uri = API_OTA_PROGRESS,
        .method = HTTP_GET,
        .handler = ota_progress_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_progress_uri);

    // OTA开始API
    httpd_uri_t ota_start_uri = {
        .uri = API_OTA_START,
        .method = HTTP_POST,
        .handler = ota_start_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_start_uri);

    // OTA回滚API
    httpd_uri_t ota_rollback_uri = {
        .uri = API_OTA_ROLLBACK,
        .method = HTTP_POST,
        .handler = ota_rollback_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_rollback_uri);

    // OTA信息API
    httpd_uri_t ota_info_uri = {
        .uri = API_OTA_INFO,
        .method = HTTP_GET,
        .handler = ota_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_info_uri);

    // Wi-Fi状态API
    httpd_uri_t wifi_status_uri = {
        .uri = API_WIFI_STATUS,
        .method = HTTP_GET,
        .handler = wifi_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_status_uri);

    // Wi-Fi连接API
    httpd_uri_t wifi_connect_uri = {
        .uri = API_WIFI_CONNECT,
        .method = HTTP_POST,
        .handler = wifi_connect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_connect_uri);

    // Wi-Fi扫描API
    httpd_uri_t wifi_scan_uri = {
        .uri = API_WIFI_SCAN,
        .method = HTTP_GET,
        .handler = wifi_scan_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_scan_uri);

    ESP_LOGI(TAG, "✅ All HTTP handlers registered");
    return ESP_OK;
}

/**
 * 初始化HTTP服务器
 */
esp_err_t http_server_init(void)
{
    ESP_LOGI(TAG, "🚀 Initializing HTTP Server...");
    return ESP_OK;
}

/**
 * 启动HTTP服务器
 */
esp_err_t http_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "⚠️ HTTP server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🌐 Starting HTTP Server on port %d...", HTTP_SERVER_PORT);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = 13;  // 增加到13个以支持所有OTA接口
    config.max_resp_headers = 8;
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册处理函数
    ret = register_handlers(s_server);
    if (ret != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "✅ HTTP Server started successfully on port %d", HTTP_SERVER_PORT);
    return ESP_OK;
}

/**
 * 停止HTTP服务器
 */
esp_err_t http_server_stop(void)
{
    if (s_server == NULL) {
        ESP_LOGW(TAG, "⚠️ HTTP server not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🛑 Stopping HTTP Server...");
    esp_err_t ret = httpd_stop(s_server);
    s_server = NULL;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP Server stopped");
    } else {
        ESP_LOGE(TAG, "❌ Failed to stop HTTP server: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * 检查HTTP服务器是否运行
 */
bool http_server_is_running(void)
{
    return (s_server != NULL);
}

/**
 * 获取设备信息
 */
esp_err_t http_server_get_device_info(device_info_t* info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    // 获取Flash大小
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    // 获取MAC地址
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // 填充设备信息 - 使用version.h中的定义
    strncpy(info->device_name, PROJECT_NAME, sizeof(info->device_name) - 1);
    info->device_name[sizeof(info->device_name) - 1] = '\0';
    
    // 优先使用version.h中定义的版本号，确保版本显示的一致性
    strncpy(info->firmware_version, VERSION_STRING, sizeof(info->firmware_version) - 1);
    info->firmware_version[sizeof(info->firmware_version) - 1] = '\0';
    
    // 如果version.h中的版本为空或无效，则使用ESP-IDF应用描述符版本作为后备
    if (strlen(info->firmware_version) == 0) {
        const esp_app_desc_t *app_desc = esp_app_get_description();
        if (app_desc && strlen(app_desc->version) > 0) {
            strncpy(info->firmware_version, app_desc->version, sizeof(info->firmware_version) - 1);
            info->firmware_version[sizeof(info->firmware_version) - 1] = '\0';
        } else {
            // 如果都获取不到，设置默认版本
            strncpy(info->firmware_version, "Unknown", sizeof(info->firmware_version) - 1);
            info->firmware_version[sizeof(info->firmware_version) - 1] = '\0';
        }
    }
    
    strncpy(info->hardware_version, HARDWARE_VERSION, sizeof(info->hardware_version) - 1);
    info->hardware_version[sizeof(info->hardware_version) - 1] = '\0';
    
    snprintf(info->chip_model, sizeof(info->chip_model), "ESP32-%d核心", chip_info.cores);
    info->flash_size = flash_size;
    info->free_heap = esp_get_free_heap_size();
    info->uptime_seconds = xTaskGetTickCount() / configTICK_RATE_HZ;
    snprintf(info->mac_address, sizeof(info->mac_address),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

/**
 * 获取设备状态
 */
esp_err_t http_server_get_device_status(device_status_t* status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(device_status_t));

    // 获取Wi-Fi状态
    wifi_status_t wifi_status;
    if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
        status->wifi_connected = (wifi_status.state == WIFI_STATE_CONNECTED);
        strncpy(status->wifi_ip, wifi_status.ip_address, sizeof(status->wifi_ip) - 1);
        status->wifi_ip[sizeof(status->wifi_ip) - 1] = '\0';  // 确保字符串结束
        status->wifi_rssi = wifi_status.rssi;
    }

    // 获取SBUS状态（通过回调函数）
    if (s_sbus_callback != NULL) {
        status->sbus_connected = s_sbus_callback(status->sbus_channels);
    }

    // 获取电机状态（通过回调函数）
    if (s_motor_callback != NULL) {
        // 注意：这里获取的是电机状态，不是CAN状态
        bool motor_active = s_motor_callback(&status->motor_left_speed, &status->motor_right_speed);
        // 电机状态不等于CAN状态，这里不设置can_connected
        (void)motor_active; // 避免未使用变量警告
    }

    // CAN状态检测 - 目前没有实际CAN硬件，设置为未连接
    // TODO: 当有实际CAN硬件时，在此处添加真实的CAN状态检测
    status->can_connected = false;

    // 设置时间戳 - 将系统滴答计数转换为毫秒
    status->last_sbus_time = g_last_sbus_update * portTICK_PERIOD_MS;
    status->last_cmd_time = g_last_motor_update * portTICK_PERIOD_MS;

    return ESP_OK;
}

/**
 * 获取OTA进度
 */
esp_err_t http_server_get_ota_progress(ota_progress_t* progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if !ENABLE_HTTP_OTA
    return ESP_ERR_NOT_SUPPORTED;
#endif

    return ota_manager_get_progress(progress);
}

/**
 * 获取系统健康状态
 */
esp_err_t http_server_get_system_health(system_health_t* health)
{
    if (health == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(health, 0, sizeof(system_health_t));

    // 获取系统运行时间
    health->uptime_seconds = xTaskGetTickCount() / configTICK_RATE_HZ;

    // 获取内存信息
    health->free_heap = esp_get_free_heap_size();
    health->min_free_heap = esp_get_minimum_free_heap_size();

    // 获取任务数量
    health->task_count = uxTaskGetNumberOfTasks();

    // 简化的CPU使用率估算（基于空闲任务统计）
    // 实际实现可能需要更复杂的CPU使用率监控
    health->cpu_usage_percent = 0;  // 暂时设为0，需要实际的CPU监控实现

    // 模拟CPU温度（ESP32没有内置温度传感器，需要外部传感器）
    health->cpu_temperature = 45.0f;  // 模拟温度

    // 看门狗状态
    health->watchdog_triggered = false;  // 简化实现

    // 检查各子系统健康状态
    
    // Wi-Fi健康检查
    health->wifi_healthy = wifi_manager_is_connected();

    // SBUS健康检查 - 检查最近是否有SBUS数据更新
    uint32_t current_time = xTaskGetTickCount();
    health->sbus_healthy = (current_time - g_last_sbus_update) < pdMS_TO_TICKS(10000);  // 10秒内有更新

    // 电机健康检查 - 检查最近是否有电机命令更新
    health->motor_healthy = (current_time - g_last_motor_update) < pdMS_TO_TICKS(10000);  // 10秒内有更新

    return ESP_OK;
}

/**
 * 设置SBUS状态回调函数
 */
void http_server_set_sbus_callback(bool (*callback)(uint16_t* channels))
{
    s_sbus_callback = callback;
}

/**
 * 设置电机状态回调函数
 */
void http_server_set_motor_callback(bool (*callback)(int8_t* left, int8_t* right))
{
    s_motor_callback = callback;
}
