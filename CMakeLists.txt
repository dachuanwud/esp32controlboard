cmake_minimum_required(VERSION 3.5)

# ====================================================================
# ESP32控制板项目版本管理
# 版本号从 main/version.h 文件中自动提取
# ====================================================================

# 包含版本提取脚本
include(${CMAKE_CURRENT_SOURCE_DIR}/extract_version.cmake)

# 使用提取的版本号设置项目版本
set(PROJECT_VER ${PROJECT_VER})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32controlboard VERSION ${PROJECT_VER})
