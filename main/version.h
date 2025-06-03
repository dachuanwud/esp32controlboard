#ifndef VERSION_H
#define VERSION_H

// ====================================================================
// ESP32控制板项目版本定义
// ====================================================================

// 主版本号 - 重大功能变更或不兼容的API变更
#define VERSION_MAJOR 1

// 次版本号 - 新功能添加，向后兼容
#define VERSION_MINOR 0

// 修订版本号 - Bug修复和小的改进
#define VERSION_PATCH 2

// 版本类型后缀 (可选)
// 可选值: "", "-alpha", "-beta", "-rc", "-release", "-ota"
#define VERSION_SUFFIX "-OTA"

// 构建时间戳 (编译时自动生成)
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// ====================================================================
// 自动生成的版本字符串
// ====================================================================

// 版本号字符串宏
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// 完整版本字符串 (例如: "1.0.0-OTA")
#define VERSION_STRING TOSTRING(VERSION_MAJOR) "." \
                      TOSTRING(VERSION_MINOR) "." \
                      TOSTRING(VERSION_PATCH) \
                      VERSION_SUFFIX

// 短版本字符串 (例如: "1.0.0")
#define VERSION_SHORT TOSTRING(VERSION_MAJOR) "." \
                     TOSTRING(VERSION_MINOR) "." \
                     TOSTRING(VERSION_PATCH)

// 构建信息字符串 (例如: "Built on Dec 30 2024 10:30:15")
#define BUILD_INFO "Built on " BUILD_DATE " " BUILD_TIME

// ====================================================================
// 版本比较宏
// ====================================================================

// 版本号数值表示 (用于版本比较)
#define VERSION_NUMBER ((VERSION_MAJOR * 10000) + (VERSION_MINOR * 100) + VERSION_PATCH)

// 版本比较宏
#define VERSION_AT_LEAST(major, minor, patch) \
    (VERSION_NUMBER >= ((major * 10000) + (minor * 100) + patch))

// ====================================================================
// 硬件版本信息
// ====================================================================

#define HARDWARE_VERSION "v1.0"
#define HARDWARE_REVISION "ESP32-Control-Board"

// ====================================================================
// 项目信息
// ====================================================================

#define PROJECT_NAME "ESP32 Control Board"
#define PROJECT_DESCRIPTION "ESP32控制板Web OTA系统"
#define PROJECT_AUTHOR "李社川"
#define PROJECT_ORGANIZATION "个人项目"

// ====================================================================
// 功能特性标识
// ====================================================================

#define FEATURE_OTA_ENABLED 1
#define FEATURE_WEB_SERVER_ENABLED 1
#define FEATURE_WIFI_ENABLED 1
#define FEATURE_SBUS_ENABLED 1
#define FEATURE_CAN_ENABLED 1

#endif /* VERSION_H */ 