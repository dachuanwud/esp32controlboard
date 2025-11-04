# 🚀 ESP32 macOS 快速安装指南

## 📋 系统要求检查

你的系统已具备以下条件：
- ✅ Git 2.49.0
- ✅ Python 3.13.2
- ✅ Homebrew 4.6.17
- ✅ CMake 3.31.6

## 🛠️ 安装 ESP-IDF（3 个步骤）

### 步骤 1: 安装依赖包

```bash
# 安装必需的构建工具
brew install cmake ninja ccache dfu-util

# 安装 Python 依赖
pip3 install --user pyserial
```

### 步骤 2: 下载 ESP-IDF

```bash
# 创建 ESP 目录
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF v5.4.1
git clone --recursive -b v5.4.1 https://github.com/espressif/esp-idf.git

# 或使用国内镜像（如果 GitHub 太慢）
# git clone --recursive -b v5.4.1 https://gitee.com/EspressifSystems/esp-idf.git
```

### 步骤 3: 安装 ESP32 工具链

```bash
# 进入 ESP-IDF 目录
cd ~/esp/esp-idf

# 安装 ESP32 工具链
./install.sh esp32

# 设置环境变量
source ./export.sh
```

## ⚙️ 配置环境变量（永久生效）

添加到你的 shell 配置文件：

```bash
# 对于 zsh (macOS 默认)
cat >> ~/.zshrc << 'EOF'

# ESP-IDF 环境配置
export IDF_PATH="$HOME/esp/esp-idf"
alias get_idf='source $IDF_PATH/export.sh'

EOF

# 使配置生效
source ~/.zshrc
```

## ✅ 验证安装

```bash
# 激活 ESP-IDF 环境
get_idf

# 验证工具
idf.py --version
xtensa-esp32-elf-gcc --version

# 或运行检查脚本
cd /Users/lishechuan/Downloads/esp32controlboard
./check_env.sh
```

## 🚀 编译项目

```bash
# 返回项目目录
cd /Users/lishechuan/Downloads/esp32controlboard

# 激活环境（如果还没激活）
get_idf

# 编译项目
idf.py build

# 查看可用串口
ls /dev/cu.*

# 烧录到设备（替换端口名）
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

## 🔍 USB 串口驱动

如果无法识别 ESP32 设备，根据芯片类型安装驱动：

### CH340/CH341 芯片
```bash
# 使用 Homebrew 安装
brew tap mengbo/ch340g-ch34g-ch34x-mac-os-x-driver https://github.com/mengbo/ch340g-ch34g-ch34x-mac-os-x-driver
brew install ch340g-ch34g-ch34x-mac-os-x-driver
```

或从 [GitHub](https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver) 手动下载

### CP210x 芯片
从 [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) 下载驱动

### FTDI 芯片
macOS 通常内置支持，无需额外驱动

## 💡 一键安装命令（复制粘贴）

```bash
# 安装所有依赖并设置环境
brew install cmake ninja ccache dfu-util && \
pip3 install --user pyserial && \
mkdir -p ~/esp && \
cd ~/esp && \
git clone --recursive -b v5.4.1 https://github.com/espressif/esp-idf.git && \
cd esp-idf && \
./install.sh esp32 && \
echo 'export IDF_PATH="$HOME/esp/esp-idf"' >> ~/.zshrc && \
echo 'alias get_idf="source \$IDF_PATH/export.sh"' >> ~/.zshrc && \
source ~/.zshrc && \
source ~/esp/esp-idf/export.sh && \
echo "✅ ESP-IDF 安装完成！"
```

## 🐛 常见问题

### 问题 1: Python 版本不兼容
```bash
# 安装 Python 3.11（ESP-IDF 推荐版本）
brew install python@3.11

# 设置为默认
export PATH="/opt/homebrew/opt/python@3.11/bin:$PATH"
```

### 问题 2: 权限错误
```bash
# 修复权限
sudo chown -R $(whoami) ~/esp
```

### 问题 3: 下载速度慢
```bash
# 使用国内镜像
export IDF_GITHUB_ASSETS="dl.espressif.com"
```

### 问题 4: M1/M2 Mac 特殊配置
```bash
# 确保 Homebrew 路径正确
eval "$(/opt/homebrew/bin/brew shellenv)"
```

## 📚 下一步

安装完成后：

1. **编译项目**: `idf.py build`
2. **查看文档**: `docs/01-开发指南/`
3. **使用 VS Code**: 安装 ESP-IDF 扩展
4. **运行测试**: `./check_env.sh`

## 🆘 获取帮助

- 📖 [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.1/)
- 💬 [ESP32 论坛](https://esp32.com/)
- 📁 [项目文档](docs/README.md)

---

**提示**: 如果遇到问题，先运行 `./check_env.sh` 查看具体缺失项！
