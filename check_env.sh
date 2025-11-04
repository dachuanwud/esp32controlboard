#!/bin/bash
# ========================================================
# ESP32 开发环境检查工具
# 快速验证开发环境是否配置正确
# ========================================================

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 检查计数
CHECKS_TOTAL=0
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARNING=0

print_header() {
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}          ESP32 开发环境检查工具${NC}"
    echo -e "${CYAN}========================================================${NC}"
    echo ""
}

check_item() {
    local name=$1
    local command=$2
    local required=$3  # true/false

    CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

    printf "%-40s" "检查 $name..."

    if eval "$command" >/dev/null 2>&1; then
        echo -e "${GREEN}✓ 通过${NC}"
        CHECKS_PASSED=$((CHECKS_PASSED + 1))
        return 0
    else
        if [ "$required" = "true" ]; then
            echo -e "${RED}✗ 失败${NC}"
            CHECKS_FAILED=$((CHECKS_FAILED + 1))
        else
            echo -e "${YELLOW}⚠ 警告${NC}"
            CHECKS_WARNING=$((CHECKS_WARNING + 1))
        fi
        return 1
    fi
}

check_version() {
    local name=$1
    local command=$2
    local required=$3

    CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

    printf "%-40s" "检查 $name..."

    if command -v ${command%% *} >/dev/null 2>&1; then
        local version=$(eval "$command" 2>&1 | head -n1)
        echo -e "${GREEN}✓${NC} $version"
        CHECKS_PASSED=$((CHECKS_PASSED + 1))
        return 0
    else
        if [ "$required" = "true" ]; then
            echo -e "${RED}✗ 未安装${NC}"
            CHECKS_FAILED=$((CHECKS_FAILED + 1))
        else
            echo -e "${YELLOW}⚠ 未安装${NC}"
            CHECKS_WARNING=$((CHECKS_WARNING + 1))
        fi
        return 1
    fi
}

print_section() {
    echo ""
    echo -e "${BLUE}━━━ $1 ━━━${NC}"
    echo ""
}

main() {
    clear
    print_header

    # 1. 基础工具检查
    print_section "1. 基础工具"
    check_version "Git" "git --version" true
    check_version "Python3" "python3 --version" true
    check_version "pip3" "pip3 --version" true
    check_version "CMake" "cmake --version" true

    # 2. ESP-IDF 环境
    print_section "2. ESP-IDF 环境"
    check_item "IDF_PATH 环境变量" "[ -n \"\$IDF_PATH\" ]" true
    check_item "ESP-IDF 目录存在" "[ -d \"\$IDF_PATH\" ]" true
    check_item "ESP-IDF export.sh" "[ -f \"\$IDF_PATH/export.sh\" ]" true
    check_version "idf.py 工具" "idf.py --version" true

    # 3. ESP32 工具链
    print_section "3. ESP32 工具链"
    check_version "xtensa-esp32-elf-gcc" "xtensa-esp32-elf-gcc --version" true
    check_item "esptool Python包" "python3 -c 'import esptool'" true
    check_item "ESP32 toolchain" "[ -d \"\$HOME/.espressif\" ]" true

    # 4. 构建工具
    print_section "4. 构建工具"
    check_version "Ninja" "ninja --version" false
    check_version "ccache" "ccache --version" false

    # 5. macOS 特定检查
    if [[ "$OSTYPE" == "darwin"* ]]; then
        print_section "5. macOS 特定"
        check_version "Homebrew" "brew --version" false
        check_item "USB串口设备" "ls /dev/cu.* 2>/dev/null | grep -q 'usbserial\|SLAB\|wchusbserial'" false
    fi

    # 6. 项目配置
    print_section "6. 项目配置"
    check_item "项目 CMakeLists.txt" "[ -f CMakeLists.txt ]" true
    check_item "sdkconfig 文件" "[ -f sdkconfig ]" true
    check_item "main 目录" "[ -d main ]" true
    check_item "build 脚本" "[ -f build_only.sh ]" false

    # 7. 可选工具
    print_section "7. 可选工具"
    check_version "Node.js" "node --version" false
    check_version "npm" "npm --version" false

    # 8. VS Code 配置
    print_section "8. VS Code 配置"
    check_item ".vscode 目录" "[ -d .vscode ]" false
    check_item "VS Code settings" "[ -f .vscode/settings.json ]" false
    check_item "VS Code tasks" "[ -f .vscode/tasks.json ]" false

    # 总结
    echo ""
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}                    检查结果总结${NC}"
    echo -e "${CYAN}========================================================${NC}"
    echo ""
    echo -e "总检查项: ${BLUE}$CHECKS_TOTAL${NC}"
    echo -e "通过:     ${GREEN}$CHECKS_PASSED${NC}"
    echo -e "失败:     ${RED}$CHECKS_FAILED${NC}"
    echo -e "警告:     ${YELLOW}$CHECKS_WARNING${NC}"
    echo ""

    if [ $CHECKS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✅ 开发环境配置完整，可以开始开发！${NC}"
        echo ""
        echo "下一步操作:"
        echo "  1. 编译项目: idf.py build"
        echo "  2. 烧录固件: idf.py -p PORT flash"
        echo "  3. 监控串口: idf.py -p PORT monitor"
        exit 0
    else
        echo -e "${RED}❌ 开发环境存在问题，请修复后再继续${NC}"
        echo ""
        echo "修复建议:"
        if ! command -v idf.py >/dev/null 2>&1; then
            echo "  • 运行环境配置脚本: ./setup_env_macos.sh"
            echo "  • 或激活ESP-IDF: source \$IDF_PATH/export.sh"
        fi
        if [ -z "$IDF_PATH" ]; then
            echo "  • 设置 IDF_PATH: export IDF_PATH=~/esp/esp-idf"
        fi
        echo "  • 查看文档: docs/01-开发指南/环境搭建指南.md"
        exit 1
    fi
}

main "$@"
