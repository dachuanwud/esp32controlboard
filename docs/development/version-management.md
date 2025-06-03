# 📝 ESP32控制板版本管理系统

## 🎯 概述

ESP32控制板项目采用了基于代码的版本管理系统，版本号在C头文件中定义，然后自动传递给CMake构建系统和ESP-IDF应用描述符。

## 📁 文件结构

```
esp32controlboard/
├── main/version.h          # 版本定义头文件 (主要配置文件)
├── extract_version.cmake   # CMake版本提取脚本
├── CMakeLists.txt          # 项目CMake配置
└── docs/development/
    └── version-management.md # 本文档
```

## 🔧 版本号定义 (`main/version.h`)

### 基本版本号

```c
// 主版本号 - 重大功能变更或不兼容的API变更
#define VERSION_MAJOR 1

// 次版本号 - 新功能添加，向后兼容  
#define VERSION_MINOR 0

// 修订版本号 - Bug修复和小的改进
#define VERSION_PATCH 0

// 版本类型后缀
#define VERSION_SUFFIX "-OTA"
```

### 自动生成的版本字符串

```c
// 完整版本字符串: "1.0.0-OTA"
#define VERSION_STRING

// 短版本字符串: "1.0.0"  
#define VERSION_SHORT

// 构建信息: "Built on Dec 30 2024 10:30:15"
#define BUILD_INFO
```

### 项目信息

```c
#define PROJECT_NAME "ESP32 Control Board"
#define PROJECT_DESCRIPTION "ESP32控制板Web OTA系统"
#define PROJECT_AUTHOR "李社川"
#define PROJECT_ORGANIZATION "个人项目"
```

### 硬件信息

```c
#define HARDWARE_VERSION "v1.0"
#define HARDWARE_REVISION "ESP32-Control-Board"
```

### 功能特性标识

```c
#define FEATURE_OTA_ENABLED 1
#define FEATURE_WEB_SERVER_ENABLED 1
#define FEATURE_WIFI_ENABLED 1
#define FEATURE_SBUS_ENABLED 1
#define FEATURE_CAN_ENABLED 1
```

## 🔄 版本更新流程

### 1. 修改版本号

编辑 `main/version.h` 文件：

```c
// 示例：发布新的功能版本
#define VERSION_MAJOR 1
#define VERSION_MINOR 1  // 新功能版本
#define VERSION_PATCH 0
#define VERSION_SUFFIX "-OTA"
```

### 2. 编译项目

```bash
# 使用项目脚本编译
build_only.bat

# 或使用ESP-IDF命令
idf.py build
```

### 3. 查看版本信息

编译时会显示提取的版本信息：

```
====================================
ESP32控制板项目构建信息
====================================
项目版本: 1.1.0-OTA
主版本号: 1
次版本号: 1  
修订版本: 0
版本后缀: -OTA
====================================
```

### 4. 运行时版本输出

程序启动时会显示完整的版本信息：

```
🚀 ESP32 Control Board
====================================
📋 项目信息:
   📦 项目名称: ESP32 Control Board
   📝 项目描述: ESP32控制板Web OTA系统
   👤 项目作者: 李社川
   🏢 组织机构: 个人项目

🔢 版本信息:
   🚀 固件版本: 1.1.0-OTA
   🔨 硬件版本: v1.0
   📅 构建信息: Built on Dec 30 2024 10:30:15
   🔢 版本数值: 10100

⚡ 功能特性:
   📡 OTA更新: 启用
   🌐 Web服务器: 启用
   📶 Wi-Fi功能: 启用
   🎮 SBUS遥控: 启用
   🚗 CAN总线: 启用
====================================
```

## 🌐 Web界面显示

版本信息会自动显示在Web界面的设备信息页面：

- **固件版本**: 显示 `VERSION_STRING` (例如: 1.1.0-OTA)
- **硬件版本**: 显示 `HARDWARE_VERSION` (例如: v1.0)
- **设备名称**: 显示 `PROJECT_NAME`

## 📋 版本命名规范

### 语义化版本控制 (Semantic Versioning)

- **主版本号 (MAJOR)**: 不兼容的API修改
- **次版本号 (MINOR)**: 向后兼容的功能性新增
- **修订版本号 (PATCH)**: 向后兼容的问题修正

### 版本后缀说明

| 后缀 | 含义 | 使用场景 |
|------|------|----------|
| `-alpha` | 内部测试版本 | 开发阶段早期 |
| `-beta` | 公开测试版本 | 功能基本完成，需要测试 |
| `-rc` | 发布候选版本 | 准备正式发布前的最终测试 |
| `-OTA` | OTA发布版本 | 支持无线更新的正式版本 |
| `-release` | 正式发布版本 | 稳定的正式版本 |
| (空) | 稳定版本 | 完全稳定的发布版本 |

### 版本示例

```
1.0.0-alpha    # 第一个内部测试版本
1.0.0-beta     # 第一个公开测试版本  
1.0.0-rc       # 发布候选版本
1.0.0-OTA      # OTA正式版本
1.0.0          # 稳定正式版本
1.0.1          # 修复版本
1.1.0          # 新功能版本
2.0.0          # 重大更新版本
```

## 🔧 高级功能

### 版本比较

```c
// 检查版本是否至少为1.1.0
if (VERSION_AT_LEAST(1, 1, 0)) {
    // 新功能代码
}

// 获取数值版本号进行比较
uint32_t current_version = VERSION_NUMBER;  // 例如: 10100 (1.1.0)
```

### 条件编译

```c
#if FEATURE_OTA_ENABLED
    // OTA功能代码
#endif

#if VERSION_MAJOR >= 2
    // 仅在2.x版本中包含的代码
#endif
```

### 运行时版本检查

```c
#include "version.h"

void check_version() {
    ESP_LOGI("VERSION", "当前版本: %s", VERSION_STRING);
    ESP_LOGI("VERSION", "构建时间: %s", BUILD_INFO);
    
    if (VERSION_AT_LEAST(1, 1, 0)) {
        ESP_LOGI("VERSION", "支持新功能");
    }
}
```

## 🚀 发布流程

### 1. 开发版本发布

```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define VERSION_SUFFIX "-beta"
```

### 2. 候选版本发布

```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define VERSION_SUFFIX "-rc"
```

### 3. 正式版本发布

```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define VERSION_SUFFIX "-OTA"
```

### 4. 稳定版本发布

```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define VERSION_SUFFIX ""
```

## 🔍 故障排除

### 版本号未更新

1. **检查版本定义**: 确认 `main/version.h` 中的版本号已正确修改
2. **清理重建**: 执行 `idf.py clean` 然后重新编译
3. **检查缓存**: 删除 `build` 目录后重新编译

### CMake错误

1. **语法检查**: 确认 `extract_version.cmake` 脚本语法正确
2. **路径检查**: 确认 `main/version.h` 文件存在
3. **权限检查**: 确认文件读取权限正常

### Web界面版本显示错误

1. **检查HTTP服务器**: 确认 `http_server.c` 包含了版本头文件
2. **重新烧录**: 完整重新烧录固件
3. **清除缓存**: 清除浏览器缓存后重新访问

## 📚 相关文档

- [ESP-IDF应用版本管理](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/app_image_format.html)
- [CMake项目版本](https://cmake.org/cmake/help/latest/command/project.html#version)
- [语义化版本控制](https://semver.org/lang/zh-CN/)

---

💡 **提示**: 版本号管理是项目维护的重要组成部分，建议每次发布都要更新版本号并做好记录！ 