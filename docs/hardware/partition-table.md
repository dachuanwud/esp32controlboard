# 📊 ESP32控制板分区表配置说明

本文档详细说明ESP32控制板项目的Flash分区表配置，包括16MB Flash的优化分区方案和OTA功能支持。

## 🎯 分区表概述

ESP32控制板使用16MB Flash存储，采用自定义分区表`partitions_16mb_ota.csv`，专为OTA功能和大容量存储优化设计。

## 📋 分区表详细配置

### 分区布局

| 分区名称 | 类型 | 子类型 | 偏移地址 | 大小 | 用途说明 |
|---------|------|--------|----------|------|----------|
| nvs | data | nvs | 0x9000 | 24KB | NVS存储(Wi-Fi配置、系统设置) |
| otadata | data | ota | 0xf000 | 8KB | OTA状态信息 |
| phy_init | data | phy | 0x11000 | 4KB | PHY初始化数据 |
| factory | app | factory | 0x20000 | 2MB | 主应用程序分区 |
| ota_0 | app | ota_0 | 0x220000 | 2MB | OTA分区0 |
| ota_1 | app | ota_1 | 0x420000 | 2MB | OTA分区1 |
| spiffs | data | spiffs | 0x620000 | 7MB | SPIFFS文件系统 |
| userdata | data | 0x40 | 0xd20000 | 1MB | 用户数据存储 |

### 地址空间分布

```
0x000000 ┌─────────────────┐
         │   Bootloader    │ 4KB (ESP-IDF管理)
0x001000 ├─────────────────┤
         │   Partition     │ 28KB (分区表)
0x009000 ├─────────────────┤
         │      NVS        │ 24KB
0x00F000 ├─────────────────┤
         │    OTA Data     │ 8KB
0x011000 ├─────────────────┤
         │   PHY Init      │ 4KB
0x020000 ├─────────────────┤
         │    Factory      │ 2MB (主程序)
0x220000 ├─────────────────┤
         │     OTA_0       │ 2MB
0x420000 ├─────────────────┤
         │     OTA_1       │ 2MB
0x620000 ├─────────────────┤
         │    SPIFFS       │ 7MB (Web文件、日志)
0xD20000 ├─────────────────┤
         │   User Data     │ 1MB (用户数据)
0x1000000└─────────────────┘ 16MB总容量
```

## 🔄 OTA功能支持

### 双分区机制

- **Factory分区**: 出厂固件，作为安全回退点
- **OTA_0分区**: 第一个OTA更新分区
- **OTA_1分区**: 第二个OTA更新分区

### OTA工作流程

1. **初始状态**: 运行Factory分区的固件
2. **首次OTA**: 更新写入OTA_0分区，重启后运行OTA_0
3. **后续OTA**: 在OTA_0和OTA_1之间交替更新
4. **回滚保护**: 更新失败时自动回滚到上一个有效分区

## 💾 存储分区说明

### NVS分区 (24KB)
- **用途**: 存储Wi-Fi配置、系统设置、用户偏好
- **特点**: 非易失性存储，断电保持数据
- **管理**: ESP-IDF NVS库自动管理

### SPIFFS分区 (7MB)
- **用途**: Web界面文件、日志文件、配置文件
- **特点**: 类似文件系统，支持文件操作
- **容量**: 足够存储完整的Web前端和大量日志

### 用户数据分区 (1MB)
- **用途**: 预留给用户自定义数据存储
- **特点**: 可用于存储传感器数据、配置文件等
- **扩展**: 可根据需要调整大小

## ⚙️ 配置文件修改

### 1. sdkconfig配置

```ini
# Flash大小配置
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"

# 分区表配置
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_16mb_ota.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_16mb_ota.csv"
```

### 2. 烧录脚本配置

```batch
# flash_com10.bat中的Flash大小参数
--flash_size 16MB
```

## 🔧 故障排除

### 常见分区错误

1. **分区超出Flash范围**
   - 检查分区表总大小是否超过16MB
   - 验证最后一个分区的结束地址

2. **分区重叠**
   - 确保相邻分区之间没有地址重叠
   - 检查偏移地址和大小计算

3. **OTA分区过小**
   - 确保OTA分区大小足够容纳固件
   - 当前配置每个OTA分区为2MB

### 分区表验证

```bash
# 编译时验证分区表
idf.py build

# 查看分区表信息
idf.py partition-table
```

## 📈 性能优化建议

### 1. 分区大小调整
- 根据实际固件大小调整OTA分区
- 如果固件小于1MB，可以减小OTA分区增加SPIFFS空间

### 2. 存储优化
- 使用SPIFFS压缩功能减少Web文件占用
- 定期清理日志文件避免SPIFFS满载

### 3. OTA优化
- 实现增量OTA减少更新时间
- 添加固件签名验证提高安全性

## 🔗 相关文档

- [ESP-IDF分区表官方文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [OTA系统设计文档](../ota-system.md)
- [编译烧录指南](../development/build-flash.md)
