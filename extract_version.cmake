# ====================================================================
# 版本号提取脚本 - extract_version.cmake
# 从 main/version.h 文件中提取版本信息并设置CMake变量
# ====================================================================

# 读取version.h文件内容
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/main/version.h" VERSION_FILE_CONTENT)

# 提取版本号各个部分
string(REGEX MATCH "#define VERSION_MAJOR ([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(VERSION_MAJOR ${CMAKE_MATCH_1})

string(REGEX MATCH "#define VERSION_MINOR ([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(VERSION_MINOR ${CMAKE_MATCH_1})

string(REGEX MATCH "#define VERSION_PATCH ([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(VERSION_PATCH ${CMAKE_MATCH_1})

# 提取版本后缀
string(REGEX MATCH "#define VERSION_SUFFIX \"([^\"]*)" _ ${VERSION_FILE_CONTENT})
set(VERSION_SUFFIX ${CMAKE_MATCH_1})

# 构建完整版本字符串
set(PROJECT_VER "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX}")

# 输出版本信息到控制台
message(STATUS "====================================")
message(STATUS "ESP32控制板项目构建信息")
message(STATUS "====================================")
message(STATUS "项目版本: ${PROJECT_VER}")
message(STATUS "主版本号: ${VERSION_MAJOR}")
message(STATUS "次版本号: ${VERSION_MINOR}")
message(STATUS "修订版本: ${VERSION_PATCH}")
message(STATUS "版本后缀: ${VERSION_SUFFIX}")
message(STATUS "====================================")

# 设置CMake项目版本变量
set(CMAKE_PROJECT_VERSION ${PROJECT_VER})
set(CMAKE_PROJECT_VERSION_MAJOR ${VERSION_MAJOR})
set(CMAKE_PROJECT_VERSION_MINOR ${VERSION_MINOR})
set(CMAKE_PROJECT_VERSION_PATCH ${VERSION_PATCH})

# 将版本信息设置为全局缓存变量，供ESP-IDF使用
set(PROJECT_VER ${PROJECT_VER} CACHE STRING "Project version from version.h" FORCE)
set(PROJECT_VERSION ${PROJECT_VER} CACHE STRING "Project version for ESP-IDF" FORCE)

# 确保版本信息传递给ESP-IDF应用描述符
set(CMAKE_PROJECT_VERSION ${PROJECT_VER} CACHE STRING "CMake project version" FORCE) 