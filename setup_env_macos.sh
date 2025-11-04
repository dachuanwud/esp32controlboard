#!/bin/bash
# ========================================================
# ESP32 macOS 开发环境自动配置脚本
# 用于快速配置 macOS 上的 ESP-IDF 开发环境
# ========================================================

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 配置变量
ESP_IDF_VERSION="v5.4.1"
ESP_IDF_PATH="${HOME}/esp/esp-idf"
PROJECT_NAME="esp32controlboard"

# 打印函数
print_header() {
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}          ESP32 macOS 开发环境配置工具${NC}"
    echo -e "${CYAN}========================================================${NC}"
    echo ""
}

print_info() {
    echo -e "${BLUE}[信息]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[成功]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[警告]${NC} $1"
}

print_error() {
    echo -e "${RED}[错误]${NC} $1"
}

print_step() {
    echo ""
    echo -e "${CYAN}----------------------------------------${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}----------------------------------------${NC}"
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 检查 Homebrew
check_homebrew() {
    print_step "步骤 1: 检查 Homebrew"

    if command_exists brew; then
        print_success "Homebrew 已安装: $(brew --version | head -n1)"
        return 0
    else
        print_warning "Homebrew 未安装"
        echo ""
        echo "是否安装 Homebrew? (y/n)"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            print_info "正在安装 Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

            # 配置 Homebrew 环境变量
            if [[ $(uname -m) == "arm64" ]]; then
                echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
                eval "$(/opt/homebrew/bin/brew shellenv)"
            fi

            print_success "Homebrew 安装完成"
            return 0
        else
            print_error "需要 Homebrew 来安装依赖包"
            return 1
        fi
    fi
}

# 安装依赖包
install_dependencies() {
    print_step "步骤 2: 安装系统依赖"

    local packages=(
        "cmake"
        "ninja"
        "ccache"
        "dfu-util"
        "python@3.11"
        "git"
        "wget"
    )

    print_info "需要安装以下包: ${packages[*]}"

    for package in "${packages[@]}"; do
        if brew list "$package" &>/dev/null; then
            print_success "$package 已安装"
        else
            print_info "正在安装 $package..."
            brew install "$package"
        fi
    done

    print_success "所有依赖包已安装"
}

# 配置 Python 环境
setup_python() {
    print_step "步骤 3: 配置 Python 环境"

    if command_exists python3; then
        local python_version=$(python3 --version)
        print_success "Python 已安装: $python_version"
    else
        print_error "Python3 未找到，请安装 Python 3.7+"
        return 1
    fi

    # 升级 pip
    print_info "升级 pip..."
    python3 -m pip install --upgrade pip

    # 安装必要的 Python 包
    print_info "安装 Python 依赖包..."
    python3 -m pip install --user pyserial

    print_success "Python 环境配置完成"
}

# 克隆或更新 ESP-IDF
setup_esp_idf() {
    print_step "步骤 4: 配置 ESP-IDF"

    if [ -d "$ESP_IDF_PATH" ]; then
        print_warning "ESP-IDF 目录已存在: $ESP_IDF_PATH"
        echo "是否更新到 $ESP_IDF_VERSION? (y/n)"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            print_info "更新 ESP-IDF..."
            cd "$ESP_IDF_PATH"
            git fetch
            git checkout "$ESP_IDF_VERSION"
            git submodule update --init --recursive
            print_success "ESP-IDF 更新完成"
        fi
    else
        print_info "克隆 ESP-IDF 仓库..."
        mkdir -p "${HOME}/esp"
        cd "${HOME}/esp"
        git clone --recursive https://github.com/espressif/esp-idf.git
        cd esp-idf
        git checkout "$ESP_IDF_VERSION"
        git submodule update --init --recursive
        print_success "ESP-IDF 克隆完成"
    fi
}

# 安装 ESP-IDF 工具链
install_esp_tools() {
    print_step "步骤 5: 安装 ESP-IDF 工具链"

    cd "$ESP_IDF_PATH"

    print_info "安装 ESP32 工具链..."
    ./install.sh esp32

    print_success "ESP-IDF 工具链安装完成"
}

# 配置环境变量
setup_environment() {
    print_step "步骤 6: 配置环境变量"

    local shell_config=""

    # 检测 Shell 类型
    if [ -n "$ZSH_VERSION" ]; then
        shell_config="$HOME/.zshrc"
    elif [ -n "$BASH_VERSION" ]; then
        shell_config="$HOME/.bashrc"
    else
        shell_config="$HOME/.profile"
    fi

    print_info "使用配置文件: $shell_config"

    # 检查是否已配置
    if grep -q "ESP-IDF" "$shell_config" 2>/dev/null; then
        print_warning "环境变量已配置"
    else
        print_info "添加 ESP-IDF 环境变量..."

        cat >> "$shell_config" << 'EOF'

# ====================================================================
# ESP-IDF 开发环境配置
# ====================================================================
export IDF_PATH="$HOME/esp/esp-idf"

# ESP-IDF 环境激活函数
get_idf() {
    if [ -f "$IDF_PATH/export.sh" ]; then
        source "$IDF_PATH/export.sh"
        echo "ESP-IDF 环境已激活"
    else
        echo "错误: ESP-IDF export.sh 未找到"
    fi
}

# 自动补全（可选）
# get_idf

# 别名配置
alias idf='idf.py'
alias idf-build='idf.py build'
alias idf-flash='idf.py flash'
alias idf-monitor='idf.py monitor'
alias idf-clean='idf.py fullclean'

EOF

        print_success "环境变量配置完成"
        print_info "请运行以下命令使配置生效:"
        print_info "  source $shell_config"
    fi
}

# 验证安装
verify_installation() {
    print_step "步骤 7: 验证安装"

    # 激活 ESP-IDF 环境
    source "$ESP_IDF_PATH/export.sh"

    # 检查工具
    print_info "检查必要工具..."

    local tools_ok=true

    # 检查 idf.py
    if command_exists idf.py; then
        print_success "idf.py: $(idf.py --version)"
    else
        print_error "idf.py 未找到"
        tools_ok=false
    fi

    # 检查编译器
    if command_exists xtensa-esp32-elf-gcc; then
        print_success "xtensa-esp32-elf-gcc: $(xtensa-esp32-elf-gcc --version | head -n1)"
    else
        print_error "xtensa-esp32-elf-gcc 未找到"
        tools_ok=false
    fi

    # 检查 Python 包
    if python3 -c "import esptool" 2>/dev/null; then
        print_success "esptool: 已安装"
    else
        print_error "esptool 未安装"
        tools_ok=false
    fi

    if [ "$tools_ok" = true ]; then
        print_success "所有工具验证通过！"
        return 0
    else
        print_error "部分工具验证失败"
        return 1
    fi
}

# 配置 USB 驱动（macOS）
setup_usb_drivers() {
    print_step "步骤 8: 配置 USB 驱动"

    print_info "检查 USB 串口驱动..."

    # 检查是否有 USB 设备
    if ls /dev/cu.* 2>/dev/null | grep -q "usbserial\|SLAB\|wchusbserial"; then
        print_success "USB 串口设备已识别:"
        ls /dev/cu.* | grep -E "usbserial|SLAB|wchusbserial" || true
    else
        print_warning "未检测到 USB 串口设备"
        echo ""
        echo "如果您使用的是 CH340/CH341 芯片，请从以下网址下载驱动:"
        echo "  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver"
        echo ""
        echo "如果您使用的是 CP210x 芯片，请从以下网址下载驱动:"
        echo "  https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    fi
}

# 测试编译
test_build() {
    print_step "步骤 9: 测试项目编译"

    # 检查是否在项目目录
    if [ -f "CMakeLists.txt" ]; then
        print_info "检测到项目文件，开始测试编译..."

        # 激活环境
        source "$ESP_IDF_PATH/export.sh"

        # 尝试配置项目
        print_info "配置项目..."
        if idf.py set-target esp32; then
            print_success "项目配置成功"

            # 询问是否立即编译
            echo ""
            echo "是否立即编译项目? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                print_info "开始编译项目..."
                if idf.py build; then
                    print_success "项目编译成功！"
                else
                    print_error "项目编译失败，请检查错误信息"
                fi
            fi
        else
            print_warning "项目配置失败"
        fi
    else
        print_info "未检测到项目文件，跳过编译测试"
    fi
}

# 显示后续步骤
show_next_steps() {
    print_step "配置完成！"

    echo ""
    echo -e "${GREEN}✅ ESP32 开发环境配置成功！${NC}"
    echo ""
    echo -e "${CYAN}后续步骤:${NC}"
    echo ""
    echo "1. 激活 ESP-IDF 环境:"
    echo -e "   ${BLUE}source ~/esp/esp-idf/export.sh${NC}"
    echo ""
    echo "2. 或者使用快捷命令:"
    echo -e "   ${BLUE}get_idf${NC}"
    echo ""
    echo "3. 编译项目:"
    echo -e "   ${BLUE}idf.py build${NC}"
    echo ""
    echo "4. 烧录到设备 (替换端口名称):"
    echo -e "   ${BLUE}idf.py -p /dev/cu.usbserial-0001 flash${NC}"
    echo ""
    echo "5. 监控串口输出:"
    echo -e "   ${BLUE}idf.py -p /dev/cu.usbserial-0001 monitor${NC}"
    echo ""
    echo "6. 一键烧录并监控:"
    echo -e "   ${BLUE}idf.py -p /dev/cu.usbserial-0001 flash monitor${NC}"
    echo ""
    echo -e "${CYAN}VS Code 用户:${NC}"
    echo "1. 安装推荐的扩展 (ESP-IDF Extension)"
    echo "2. 按 Cmd+Shift+P，输入 'ESP-IDF: Configure'"
    echo "3. 使用集成的编译、烧录功能"
    echo ""
    echo -e "${CYAN}文档资源:${NC}"
    echo "- 项目文档: docs/README.md"
    echo "- 环境搭建: docs/01-开发指南/环境搭建指南.md"
    echo "- 编译烧录: docs/01-开发指南/编译烧录指南.md"
    echo ""
    echo -e "${GREEN}🎉 祝您开发愉快！${NC}"
    echo ""
}

# 主函数
main() {
    clear
    print_header

    # 确认开始
    echo "此脚本将配置 ESP32 开发环境，包括:"
    echo "  - Homebrew 和依赖包"
    echo "  - Python 环境"
    echo "  - ESP-IDF $ESP_IDF_VERSION"
    echo "  - ESP32 工具链"
    echo "  - 环境变量配置"
    echo ""
    echo "是否继续? (y/n)"
    read -r response
    if [[ ! "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
        print_info "已取消"
        exit 0
    fi

    # 执行配置步骤
    check_homebrew || exit 1
    install_dependencies || exit 1
    setup_python || exit 1
    setup_esp_idf || exit 1
    install_esp_tools || exit 1
    setup_environment || exit 1
    verify_installation || exit 1
    setup_usb_drivers
    test_build
    show_next_steps
}

# 运行主函数
main "$@"
