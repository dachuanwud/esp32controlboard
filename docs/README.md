# 📚 ESP32控制板项目文档中心

欢迎来到ESP32控制板项目的文档中心！这里集中存放了所有与项目相关的技术文档、系统流程图、模块说明和开发指南。

## 📁 文档目录结构

```
docs/
├── README.md                    # 📖 文档中心说明（本文件）
├── architecture/                # 🏗️ 系统架构文档
│   ├── system-overview.md       # 系统总体架构
│   ├── freertos-design.md       # FreeRTOS架构设计
│   └── hardware-interface.md    # 硬件接口设计
├── modules/                     # 🔧 模块说明文档
│   ├── sbus-module.md          # SBUS模块详解
│   ├── can-module.md           # CAN模块详解
│   ├── uart-module.md          # UART模块详解
│   └── gpio-module.md          # GPIO模块详解
├── development/                 # 💻 开发指南文档
│   ├── setup-guide.md          # 开发环境搭建
│   ├── build-flash.md          # 编译烧录指南
│   ├── debugging.md            # 调试指南
│   └── coding-standards.md     # 编码规范
├── protocols/                   # 📡 通信协议文档
│   ├── sbus-protocol.md        # SBUS协议详解
│   ├── can-protocol.md         # CAN协议详解
│   └── data-flow.md            # 数据流向分析
├── hardware/                    # ⚡ 硬件相关文档
│   ├── schematic.md            # 硬件原理图说明
│   ├── pin-mapping.md          # 引脚映射表
│   └── component-list.md       # 元器件清单
└── troubleshooting/            # 🔍 故障排除文档
    ├── common-issues.md        # 常见问题解决
    ├── debug-tips.md           # 调试技巧
    └── faq.md                  # 常见问答
```

## 📋 文档分类说明

### 🏗️ 系统架构 (architecture/)
- **系统总体架构**: 整个ESP32控制板系统的架构设计和组件关系
- **FreeRTOS架构设计**: FreeRTOS任务调度、队列机制、内存管理等详解
- **硬件接口设计**: 各硬件模块的接口设计和通信方式

### 🔧 模块说明 (modules/)
- **SBUS模块**: SBUS信号接收、解析、数据处理流程
- **CAN模块**: CAN总线通信配置、数据收发机制
- **UART模块**: 串口通信配置、数据传输协议
- **GPIO模块**: 通用输入输出引脚配置和控制

### 💻 开发指南 (development/)
- **开发环境搭建**: ESP-IDF环境配置、工具链安装
- **编译烧录指南**: 项目编译、固件烧录操作步骤
- **调试指南**: 代码调试、日志分析、性能优化
- **编码规范**: C语言编码标准、注释规范、文件组织

### 📡 通信协议 (protocols/)
- **SBUS协议详解**: SBUS通信协议规范、数据格式、解析方法
- **CAN协议详解**: CAN总线协议标准、帧格式、错误处理
- **数据流向分析**: 系统内部数据流转路径和处理逻辑

### ⚡ 硬件相关 (hardware/)
- **硬件原理图说明**: 电路设计原理、信号连接关系
- **引脚映射表**: ESP32引脚分配、功能定义
- **元器件清单**: 硬件BOM表、器件规格参数

### 🔍 故障排除 (troubleshooting/)
- **常见问题解决**: 开发过程中遇到的典型问题及解决方案
- **调试技巧**: 高效调试方法、工具使用技巧
- **常见问答**: 项目相关的FAQ集合

## 📝 文档编写规范

### ✅ 文档格式要求
- 使用Markdown格式编写
- 文件名使用小写字母和连字符（kebab-case）
- 中文文档优先，必要时提供英文版本
- 适当使用emoji增强可读性

### 📊 内容组织原则
- 结构清晰，层次分明
- 包含详细的代码示例
- 提供流程图和架构图
- 及时更新，保持文档与代码同步

### 🔗 文档链接规范
- 使用相对路径引用其他文档
- 提供清晰的导航链接
- 建立文档间的交叉引用

## 🚀 快速开始

1. **新手入门**: 先阅读 `development/setup-guide.md` 搭建开发环境
2. **系统了解**: 查看 `architecture/system-overview.md` 了解整体架构
3. **模块学习**: 根据需要阅读 `modules/` 下的具体模块文档
4. **问题解决**: 遇到问题时查阅 `troubleshooting/` 下的相关文档

## 📞 文档维护

- 📅 **更新频率**: 随代码变更及时更新
- 👥 **维护责任**: 开发团队共同维护
- 📋 **版本控制**: 与代码版本保持同步
- 🔄 **审核流程**: 重要文档变更需要代码审核

---

💡 **提示**: 建议将此文档文件夹加入书签，方便快速访问项目相关文档！
