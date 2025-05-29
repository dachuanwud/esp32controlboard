# 📚 Git版本控制管理指南

## 🎯 概述

本文档提供ESP32控制板项目的Git版本控制最佳实践，包括文件管理、提交策略和清理指南。

## 📁 项目文件分类

### ✅ 应该提交的文件

#### 核心源代码
- `main/` - ESP32主程序源码
- `web_client/src/` - React前端源码
- `web_client/package.json` - 前端依赖配置
- `web_client/tsconfig.json` - TypeScript配置
- `web_client/vite.config.ts` - Vite构建配置

#### 项目配置
- `CMakeLists.txt` - ESP-IDF项目配置
- `partitions_16mb_ota.csv` - 分区表配置
- `sdkconfig.defaults` - ESP-IDF默认配置（如果存在）

#### 文档和脚本
- `docs/` - 项目文档
- `README.md` - 项目说明
- `build_only.bat` - 构建脚本
- `flash_com10.bat` - 烧录脚本

### ❌ 应该忽略的文件

#### 编译产物
- `build/` - ESP-IDF编译输出
- `web_client/dist/` - 前端构建输出
- `web_client/node_modules/` - Node.js依赖包

#### 临时文件
- `.vite/` - Vite缓存
- `.cursor/` - Cursor IDE配置
- 各种日志和临时文件

## 🔧 .gitignore 配置说明

我们的.gitignore文件包含以下主要部分：

1. **ESP-IDF相关** - 编译产物和临时文件
2. **Web Client相关** - Node.js依赖和构建产物
3. **开发工具** - IDE配置文件
4. **操作系统** - 系统生成的临时文件
5. **项目特定** - 备份文件和性能分析文件

## 📊 当前状态分析

### 文件统计
- **总计新增文件**: ~10,000+ 个
- **主要来源**: web_client/node_modules/ (~10,000个)
- **已被忽略**: node_modules和build目录
- **待提交**: 源码、配置、文档文件

### Git状态
```bash
# 修改的文件 (M)
- 源码文件更新
- 文档更新
- 脚本优化

# 删除的文件 (D)  
- 旧的工具脚本

# 新增的文件 (??)
- OTA系统相关模块
- 新的文档文件
- HTTP服务器模块
```

## 🚀 推荐的Git操作流程

### 1. 添加重要文件
```bash
# 添加新的源码文件
git add main/http_server.c main/http_server.h
git add main/ota_manager.c main/ota_manager.h
git add main/wifi_manager.c main/wifi_manager.h
git add main/time_manager.c main/time_manager.h

# 添加配置文件
git add partitions_16mb_ota.csv

# 添加文档
git add docs/api-specification.md
git add docs/deployment-guide.md
git add docs/ota-system.md
git add docs/hardware/partition-table.md
git add docs/modules/http-server-module.md
git add docs/modules/ota-manager-module.md
git add docs/modules/wifi-module.md
git add docs/troubleshooting/startup-issues.md

# 添加OTA说明
git add README-OTA.md
```

### 2. 提交更改
```bash
# 提交新功能
git commit -m "feat: 添加ESP32 Web OTA系统

- 新增HTTP服务器模块支持OTA上传
- 新增OTA管理器处理固件更新
- 新增WiFi管理器支持网络配置
- 新增时间管理器提供系统时间
- 更新分区表支持OTA双分区
- 完善项目文档和API规范"
```

### 3. 清理历史（如果需要）
如果之前错误提交了node_modules，可以使用：
```bash
# 从历史记录中移除大文件
git filter-branch --tree-filter 'rm -rf web_client/node_modules' HEAD

# 或使用git-filter-repo（推荐）
git filter-repo --path web_client/node_modules --invert-paths
```

## ⚠️ 注意事项

1. **不要提交敏感信息**
   - WiFi密码
   - API密钥
   - 调试信息

2. **保持提交原子性**
   - 每次提交只包含相关的更改
   - 使用清晰的提交信息

3. **定期清理**
   - 删除不需要的分支
   - 清理本地缓存

4. **备份重要数据**
   - 在执行清理操作前备份
   - 确保远程仓库是最新的

## 📝 提交信息规范

使用约定式提交格式：
```
<类型>: <描述>

[可选的正文]

[可选的脚注]
```

类型包括：
- `feat`: 新功能
- `fix`: 修复bug
- `docs`: 文档更新
- `style`: 代码格式化
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建过程或辅助工具的变动

## 🔍 检查命令

```bash
# 检查文件状态
git status

# 查看忽略的文件
git status --ignored

# 检查文件大小
git ls-files | xargs ls -la

# 查看提交历史
git log --oneline -10
```
