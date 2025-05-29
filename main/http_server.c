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

// CORS头部
static const char* CORS_HEADERS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";

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
    httpd_resp_set_status(req, status_code == 200 ? "200 OK" : "400 Bad Request");

    // 发送响应
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
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

    device_info_t info;
    if (http_server_get_device_info(&info) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddStringToObject(data, "device_name", info.device_name);
    cJSON_AddStringToObject(data, "firmware_version", info.firmware_version);
    cJSON_AddStringToObject(data, "hardware_version", info.hardware_version);
    cJSON_AddStringToObject(data, "chip_model", info.chip_model);
    cJSON_AddNumberToObject(data, "flash_size", info.flash_size);
    cJSON_AddNumberToObject(data, "free_heap", info.free_heap);
    cJSON_AddNumberToObject(data, "uptime_seconds", info.uptime_seconds);
    cJSON_AddStringToObject(data, "mac_address", info.mac_address);

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
 * OTA上传处理函数
 */
static esp_err_t ota_upload_handler(httpd_req_t *req)
{
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

    // Wi-Fi状态API
    httpd_uri_t wifi_status_uri = {
        .uri = API_WIFI_STATUS,
        .method = HTTP_GET,
        .handler = wifi_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_status_uri);

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
    config.max_uri_handlers = 10;
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

    // 填充设备信息
    strcpy(info->device_name, "ESP32 Control Board");
    strcpy(info->firmware_version, "1.0.0-OTA");
    strcpy(info->hardware_version, "v1.0");
    snprintf(info->chip_model, sizeof(info->chip_model), "ESP32-%d", chip_info.revision);
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
        strcpy(status->wifi_ip, wifi_status.ip_address);
        status->wifi_rssi = wifi_status.rssi;
    }

    // 获取SBUS状态（通过回调函数）
    if (s_sbus_callback != NULL) {
        status->sbus_connected = s_sbus_callback(status->sbus_channels);
    }

    // 获取电机状态（通过回调函数）
    if (s_motor_callback != NULL) {
        status->can_connected = s_motor_callback(&status->motor_left_speed, &status->motor_right_speed);
    }

    // 设置时间戳 - 使用系统滴答计数转换为毫秒
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

    return ota_manager_get_progress(progress);
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