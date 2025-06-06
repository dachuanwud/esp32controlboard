#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "http_server.h"

// OTA状态枚举
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_PREPARING,
    OTA_STATE_WRITING,
    OTA_STATE_VALIDATING,
    OTA_STATE_COMPLETED,
    OTA_STATE_FAILED
} ota_state_t;

// OTA配置结构体
typedef struct {
    uint32_t max_firmware_size;
    bool verify_signature;
    bool auto_rollback;
    uint32_t rollback_timeout_ms;
} ota_config_t;

// OTA进度回调函数类型
typedef void (*ota_progress_callback_t)(uint8_t progress_percent, const char* status_message);

/**
 * 初始化OTA管理器
 * @param config OTA配置参数
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_init(const ota_config_t* config);

/**
 * 开始OTA更新
 * @param firmware_size 固件大小
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_begin(uint32_t firmware_size);

/**
 * 写入固件数据
 * @param data 固件数据
 * @param size 数据大小
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_write(const void* data, size_t size);

/**
 * 完成OTA更新
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_end(void);

/**
 * 中止OTA更新
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_abort(void);

/**
 * 获取OTA进度
 * @param progress 输出进度信息
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_get_progress(ota_progress_t* progress);

/**
 * 获取当前运行的分区信息
 * @return 分区信息指针
 */
const esp_partition_t* ota_manager_get_running_partition(void);

/**
 * 获取下一个OTA分区信息
 * @return 分区信息指针
 */
const esp_partition_t* ota_manager_get_next_partition(void);

/**
 * 回滚到上一个固件版本
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_rollback(void);

/**
 * 验证当前固件
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_mark_valid(void);

/**
 * 检查是否需要回滚
 * @return true=需要回滚，false=不需要
 */
bool ota_manager_check_rollback_required(void);

/**
 * 获取固件版本信息
 * @param version_buffer 版本字符串缓冲区
 * @param buffer_size 缓冲区大小
 * @return ESP_OK=成功
 */
esp_err_t ota_manager_get_version(char* version_buffer, size_t buffer_size);

/**
 * 获取分区表信息
 * @param partition_info 分区信息数组
 * @param max_partitions 最大分区数量
 * @return 实际分区数量
 */
uint8_t ota_manager_get_partition_info(esp_partition_t* partition_info, uint8_t max_partitions);

/**
 * 设置OTA进度回调函数
 * @param callback 进度回调函数指针
 */
void ota_manager_set_progress_callback(ota_progress_callback_t callback);

#endif /* OTA_MANAGER_H */
