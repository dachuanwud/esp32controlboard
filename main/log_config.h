#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 日志配置函数
 * 设置各个模块的日志级别
 */
void configure_logging(void);

/**
 * 启用详细调试日志
 * 用于开发和调试阶段
 */
void enable_debug_logging(void);

/**
 * 启用生产环境日志
 * 减少日志输出，提高性能
 */
void enable_production_logging(void);

/**
 * 打印系统信息
 * 显示设备基本信息和配置
 */
void print_system_info(void);

/**
 * 打印网络状态信息
 */
void print_network_status(void);

/**
 * 打印云服务状态信息
 */
void print_cloud_status(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_CONFIG_H
