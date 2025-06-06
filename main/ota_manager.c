#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_app_format.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "OTA_MGR";

// OTA状态变量
static ota_state_t s_ota_state = OTA_STATE_IDLE;
static ota_progress_t s_ota_progress = {0};
static ota_config_t s_ota_config = {0};

// OTA操作句柄
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;
static const esp_partition_t *s_running_partition = NULL;

// 固件验证相关
static uint32_t s_firmware_size = 0;
static uint32_t s_written_size = 0;

// 进度回调函数
static ota_progress_callback_t s_progress_callback = NULL;

/**
 * 更新OTA进度信息
 */
static void update_progress(ota_state_t state, const char* message)
{
    s_ota_state = state;
    s_ota_progress.in_progress = (state != OTA_STATE_IDLE && state != OTA_STATE_COMPLETED && state != OTA_STATE_FAILED);

    if (s_firmware_size > 0) {
        s_ota_progress.progress_percent = (s_written_size * 100) / s_firmware_size;
    }

    s_ota_progress.total_size = s_firmware_size;
    s_ota_progress.written_size = s_written_size;

    if (message != NULL) {
        strncpy(s_ota_progress.status_message, message, sizeof(s_ota_progress.status_message) - 1);
        s_ota_progress.status_message[sizeof(s_ota_progress.status_message) - 1] = '\0';
    }

    s_ota_progress.success = (state == OTA_STATE_COMPLETED);

    ESP_LOGI(TAG, "📊 OTA Progress: %d%% (%" PRIu32 "/%" PRIu32 ") bytes - %s",
             s_ota_progress.progress_percent, s_written_size, s_firmware_size,
             s_ota_progress.status_message);

    // 向云端报告进度（如果有回调函数）
    if (s_progress_callback) {
        s_progress_callback(s_ota_progress.progress_percent, s_ota_progress.status_message);
    }
}

/**
 * 设置错误信息
 */
static void set_error(const char* error_message)
{
    s_ota_state = OTA_STATE_FAILED;
    s_ota_progress.in_progress = false;
    s_ota_progress.success = false;

    if (error_message != NULL) {
        strncpy(s_ota_progress.error_message, error_message, sizeof(s_ota_progress.error_message) - 1);
        s_ota_progress.error_message[sizeof(s_ota_progress.error_message) - 1] = '\0';
        strncpy(s_ota_progress.status_message, "Failed", sizeof(s_ota_progress.status_message) - 1);
    }

    ESP_LOGE(TAG, "❌ OTA Error: %s", s_ota_progress.error_message);
}

/**
 * 初始化OTA管理器
 */
esp_err_t ota_manager_init(const ota_config_t* config)
{
    ESP_LOGI(TAG, "Initializing OTA Manager...");

    if (config != NULL) {
        memcpy(&s_ota_config, config, sizeof(ota_config_t));
    } else {
        // 默认配置
        s_ota_config.max_firmware_size = 1024 * 1024; // 1MB
        s_ota_config.verify_signature = false;
        s_ota_config.auto_rollback = true;
        s_ota_config.rollback_timeout_ms = 30000; // 30秒
    }

    // 获取当前运行分区
    s_running_partition = esp_ota_get_running_partition();
    if (s_running_partition == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get running partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Running partition: %s (offset: 0x%08" PRIx32 ", size: %" PRIu32 ")",
             s_running_partition->label, (uint32_t)s_running_partition->address, (uint32_t)s_running_partition->size);

    // 初始化进度信息
    memset(&s_ota_progress, 0, sizeof(ota_progress_t));
    s_ota_state = OTA_STATE_IDLE;
    strcpy(s_ota_progress.status_message, "Ready");

    ESP_LOGI(TAG, "✅ OTA Manager initialized successfully");
    return ESP_OK;
}

/**
 * 开始OTA更新
 */
esp_err_t ota_manager_begin(uint32_t firmware_size)
{
    if (s_ota_state != OTA_STATE_IDLE) {
        set_error("OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (firmware_size == 0 || firmware_size > s_ota_config.max_firmware_size) {
        set_error("Invalid firmware size");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA update, firmware size: %" PRIu32 " bytes", (uint32_t)firmware_size);
    update_progress(OTA_STATE_PREPARING, "Preparing OTA update");

    // 获取下一个OTA分区
    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_update_partition == NULL) {
        set_error("Failed to get update partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Update partition: %s (offset: 0x%08" PRIx32 ", size: %" PRIu32 ")",
             s_update_partition->label, (uint32_t)s_update_partition->address, (uint32_t)s_update_partition->size);

    // 开始OTA操作
    esp_err_t ret = esp_ota_begin(s_update_partition, firmware_size, &s_ota_handle);
    if (ret != ESP_OK) {
        set_error("Failed to begin OTA update");
        return ret;
    }

    // 初始化状态
    s_firmware_size = firmware_size;
    s_written_size = 0;

    update_progress(OTA_STATE_WRITING, "Ready to receive firmware data");
    ESP_LOGI(TAG, "✅ OTA update started successfully");
    return ESP_OK;
}

/**
 * 写入固件数据
 */
esp_err_t ota_manager_write(const void* data, size_t size)
{
    if (s_ota_state != OTA_STATE_WRITING) {
        set_error("OTA not in writing state");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || size == 0) {
        set_error("Invalid data or size");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_written_size + size > s_firmware_size) {
        set_error("Data size exceeds firmware size");
        return ESP_ERR_INVALID_SIZE;
    }

    // 写入数据到OTA分区
    esp_err_t ret = esp_ota_write(s_ota_handle, data, size);
    if (ret != ESP_OK) {
        set_error("Failed to write OTA data");
        return ret;
    }

    s_written_size += size;

    // 更新进度（每写入64KB或完成时更新一次）
    if (s_written_size % (64 * 1024) == 0 || s_written_size == s_firmware_size) {
        char progress_msg[64];
        snprintf(progress_msg, sizeof(progress_msg), "Writing firmware: %u%%",
                (unsigned int)((s_written_size * 100) / s_firmware_size));
        update_progress(OTA_STATE_WRITING, progress_msg);
    }

    return ESP_OK;
}

/**
 * 完成OTA更新
 */
esp_err_t ota_manager_end(void)
{
    if (s_ota_state != OTA_STATE_WRITING) {
        set_error("OTA not in writing state");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_written_size != s_firmware_size) {
        set_error("Incomplete firmware data");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "🔍 Validating firmware...");
    update_progress(OTA_STATE_VALIDATING, "Validating firmware");

    // 结束OTA写入
    esp_err_t ret = esp_ota_end(s_ota_handle);
    if (ret != ESP_OK) {
        set_error("Failed to end OTA update");
        return ret;
    }

    // 设置启动分区
    ret = esp_ota_set_boot_partition(s_update_partition);
    if (ret != ESP_OK) {
        set_error("Failed to set boot partition");
        return ret;
    }

    update_progress(OTA_STATE_COMPLETED, "OTA update completed successfully");
    ESP_LOGI(TAG, "✅ OTA update completed successfully");
    ESP_LOGI(TAG, "🔄 System will restart to apply new firmware");

    return ESP_OK;
}

/**
 * 中止OTA更新
 */
esp_err_t ota_manager_abort(void)
{
    if (s_ota_state == OTA_STATE_IDLE) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "⚠️ Aborting OTA update...");

    if (s_ota_handle != 0) {
        esp_ota_abort(s_ota_handle);
        s_ota_handle = 0;
    }

    // 重置状态
    s_ota_state = OTA_STATE_IDLE;
    s_firmware_size = 0;
    s_written_size = 0;
    s_update_partition = NULL;

    memset(&s_ota_progress, 0, sizeof(ota_progress_t));
    strcpy(s_ota_progress.status_message, "Aborted");

    ESP_LOGI(TAG, "✅ OTA update aborted");
    return ESP_OK;
}

/**
 * 获取OTA进度
 */
esp_err_t ota_manager_get_progress(ota_progress_t* progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(progress, &s_ota_progress, sizeof(ota_progress_t));
    return ESP_OK;
}

/**
 * 获取当前运行的分区信息
 */
const esp_partition_t* ota_manager_get_running_partition(void)
{
    return s_running_partition;
}

/**
 * 获取下一个OTA分区信息
 */
const esp_partition_t* ota_manager_get_next_partition(void)
{
    return esp_ota_get_next_update_partition(NULL);
}

/**
 * 回滚到上一个固件版本
 */
esp_err_t ota_manager_rollback(void)
{
    ESP_LOGW(TAG, "🔄 Rolling back to previous firmware...");

    esp_err_t ret = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to rollback: %s", esp_err_to_name(ret));
        return ret;
    }

    // 这行代码不会执行，因为系统会重启
    return ESP_OK;
}

/**
 * 验证当前固件
 */
esp_err_t ota_manager_mark_valid(void)
{
    ESP_LOGI(TAG, "✅ Marking current firmware as valid");

    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to mark app valid: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ Current firmware marked as valid");
    return ESP_OK;
}

/**
 * 检查是否需要回滚
 */
bool ota_manager_check_rollback_required(void)
{
    esp_ota_img_states_t ota_state;
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }

    return false;
}

/**
 * 获取固件版本信息
 */
esp_err_t ota_manager_get_version(char* version_buffer, size_t buffer_size)
{
    if (version_buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_app_desc_t* app_desc = esp_app_get_description();
    if (app_desc == NULL) {
        return ESP_FAIL;
    }

    snprintf(version_buffer, buffer_size, "%s", app_desc->version);
    return ESP_OK;
}

/**
 * 获取分区表信息
 */
uint8_t ota_manager_get_partition_info(esp_partition_t* partition_info, uint8_t max_partitions)
{
    if (partition_info == NULL || max_partitions == 0) {
        return 0;
    }

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    uint8_t count = 0;

    while (it != NULL && count < max_partitions) {
        const esp_partition_t* partition = esp_partition_get(it);
        if (partition != NULL) {
            memcpy(&partition_info[count], partition, sizeof(esp_partition_t));
            count++;
        }
        it = esp_partition_next(it);
    }

    if (it != NULL) {
        esp_partition_iterator_release(it);
    }

    return count;
}

/**
 * 设置OTA进度回调函数
 */
void ota_manager_set_progress_callback(ota_progress_callback_t callback)
{
    s_progress_callback = callback;
    ESP_LOGI(TAG, "OTA进度回调函数已设置: %s", callback ? "已启用" : "已禁用");
}
