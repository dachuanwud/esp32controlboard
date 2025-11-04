#!/bin/bash
# ========================================================
# ESP32 设备检测和烧录工具
# ========================================================

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

print_header() {
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}          ESP32 设备检测和烧录工具${NC}"
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

# 检测串口设备
detect_ports() {
    print_info "正在检测串口设备..."
    echo ""

    # 列出所有串口设备
    local all_ports=$(ls /dev/cu.* 2>/dev/null)

    if [ -z "$all_ports" ]; then
        print_error "未检测到任何串口设备"
        return 1
    fi

    # 筛选可能的 ESP32 设备
    local esp_ports=$(echo "$all_ports" | grep -E "usbserial|SLAB|wchusbserial|usbmodem")

    if [ -z "$esp_ports" ]; then
        print_warning "未检测到 ESP32 设备"
        echo ""
        echo "检测到以下串口设备："
        ls -l /dev/cu.* 2>/dev/null
        echo ""
        echo -e "${YELLOW}可能的原因:${NC}"
        echo "  1. ESP32 设备未连接"
        echo "  2. USB 数据线是充电线（不支持数据传输）"
        echo "  3. 需要安装 USB 转串口驱动"
        echo "  4. USB 端口接触不良"
        echo ""
        return 1
    else
        print_success "检测到 ESP32 设备！"
        echo ""
        echo "可用的 ESP32 端口："
        for port in $esp_ports; do
            echo -e "  ${GREEN}✓${NC} $port"
        done
        echo ""
        return 0
    fi
}

# 安装驱动指南
show_driver_guide() {
    echo ""
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}            USB 转串口驱动安装指南${NC}"
    echo -e "${CYAN}========================================================${NC}"
    echo ""

    echo -e "${BLUE}常见 USB 转串口芯片及驱动:${NC}"
    echo ""

    echo "1. ${GREEN}CH340/CH341 芯片${NC} (最常见)"
    echo "   安装命令:"
    echo "   ${YELLOW}brew tap mengbo/ch340g-ch34g-ch34x-mac-os-x-driver https://github.com/mengbo/ch340g-ch34g-ch34x-mac-os-x-driver${NC}"
    echo "   ${YELLOW}brew install ch340g-ch34g-ch34x-mac-os-x-driver${NC}"
    echo ""

    echo "2. ${GREEN}CP210x 芯片${NC}"
    echo "   下载地址: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    echo ""

    echo "3. ${GREEN}FTDI 芯片${NC}"
    echo "   macOS 通常内置支持，无需额外驱动"
    echo ""

    echo -e "${YELLOW}安装后需要:${NC}"
    echo "  1. 重启电脑或重新插拔 USB 设备"
    echo "  2. 重新运行此脚本检测设备"
    echo ""
}

# 测试连接
test_connection() {
    local port=$1

    print_info "测试连接到 $port..."

    # 使用 esptool 检测芯片
    if command -v esptool.py >/dev/null 2>&1; then
        esptool.py --port "$port" chip_id

        if [ $? -eq 0 ]; then
            print_success "ESP32 连接成功！"
            return 0
        else
            print_error "连接失败"
            return 1
        fi
    else
        print_warning "esptool.py 未找到，请先激活 ESP-IDF 环境"
        echo "运行: source ~/esp/esp-idf/export.sh"
        return 1
    fi
}

# 烧录固件
flash_firmware() {
    local port=$1

    print_info "准备烧录固件到 $port..."
    echo ""

    # 检查编译输出
    if [ ! -f "build/esp32controlboard.bin" ]; then
        print_error "未找到编译后的固件文件"
        echo "请先运行: idf.py build"
        return 1
    fi

    print_info "固件文件检查通过"
    print_info "开始烧录..."
    echo ""

    # 烧录固件
    idf.py -p "$port" flash

    if [ $? -eq 0 ]; then
        print_success "固件烧录成功！"
        echo ""
        echo "是否启动串口监控? (y/n)"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            print_info "启动串口监控 (按 Ctrl+] 退出)..."
            idf.py -p "$port" monitor
        fi
        return 0
    else
        print_error "烧录失败"
        return 1
    fi
}

# 主菜单
main_menu() {
    while true; do
        echo ""
        echo -e "${CYAN}请选择操作:${NC}"
        echo "  1) 检测 ESP32 设备"
        echo "  2) 测试设备连接"
        echo "  3) 烧录固件"
        echo "  4) 烧录并监控"
        echo "  5) 查看驱动安装指南"
        echo "  6) 退出"
        echo ""
        echo -n "请输入选项 [1-6]: "
        read -r choice

        case $choice in
            1)
                detect_ports
                ;;
            2)
                # 检测端口
                local esp_ports=$(ls /dev/cu.* 2>/dev/null | grep -E "usbserial|SLAB|wchusbserial|usbmodem")
                if [ -z "$esp_ports" ]; then
                    print_error "未检测到 ESP32 设备"
                    detect_ports
                else
                    # 如果只有一个端口
                    local port_count=$(echo "$esp_ports" | wc -l | tr -d ' ')
                    if [ "$port_count" -eq 1 ]; then
                        test_connection "$esp_ports"
                    else
                        echo "检测到多个设备，请选择:"
                        select port in $esp_ports; do
                            if [ -n "$port" ]; then
                                test_connection "$port"
                                break
                            fi
                        done
                    fi
                fi
                ;;
            3)
                # 烧录固件
                local esp_ports=$(ls /dev/cu.* 2>/dev/null | grep -E "usbserial|SLAB|wchusbserial|usbmodem")
                if [ -z "$esp_ports" ]; then
                    print_error "未检测到 ESP32 设备"
                    detect_ports
                else
                    local port_count=$(echo "$esp_ports" | wc -l | tr -d ' ')
                    if [ "$port_count" -eq 1 ]; then
                        flash_firmware "$esp_ports"
                    else
                        echo "检测到多个设备，请选择:"
                        select port in $esp_ports; do
                            if [ -n "$port" ]; then
                                flash_firmware "$port"
                                break
                            fi
                        done
                    fi
                fi
                ;;
            4)
                # 烧录并监控
                local esp_ports=$(ls /dev/cu.* 2>/dev/null | grep -E "usbserial|SLAB|wchusbserial|usbmodem")
                if [ -z "$esp_ports" ]; then
                    print_error "未检测到 ESP32 设备"
                    detect_ports
                else
                    local port_count=$(echo "$esp_ports" | wc -l | tr -d ' ')
                    if [ "$port_count" -eq 1 ]; then
                        print_info "烧录并监控..."
                        idf.py -p "$esp_ports" flash monitor
                    else
                        echo "检测到多个设备，请选择:"
                        select port in $esp_ports; do
                            if [ -n "$port" ]; then
                                print_info "烧录并监控..."
                                idf.py -p "$port" flash monitor
                                break
                            fi
                        done
                    fi
                fi
                ;;
            5)
                show_driver_guide
                ;;
            6)
                print_info "退出"
                exit 0
                ;;
            *)
                print_error "无效选项"
                ;;
        esac
    done
}

# 主函数
main() {
    clear
    print_header

    # 检查是否在项目目录
    if [ ! -f "CMakeLists.txt" ]; then
        print_error "请在项目根目录运行此脚本"
        exit 1
    fi

    # 检查 ESP-IDF 环境
    if [ -z "$IDF_PATH" ]; then
        print_warning "ESP-IDF 环境未激活"
        echo ""
        echo "请运行以下命令激活环境:"
        echo "  ${YELLOW}source ~/esp/esp-idf/export.sh${NC}"
        echo ""
        echo "或使用快捷命令:"
        echo "  ${YELLOW}get_idf${NC}"
        echo ""
        exit 1
    fi

    print_success "ESP-IDF 环境已激活"

    # 首次自动检测
    if detect_ports; then
        # 检测到设备，询问是否立即烧录
        echo "是否立即烧录固件? (y/n)"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            local esp_ports=$(ls /dev/cu.* 2>/dev/null | grep -E "usbserial|SLAB|wchusbserial|usbmodem")
            local port_count=$(echo "$esp_ports" | wc -l | tr -d ' ')
            if [ "$port_count" -eq 1 ]; then
                flash_firmware "$esp_ports"
            else
                echo "检测到多个设备，请选择:"
                select port in $esp_ports; do
                    if [ -n "$port" ]; then
                        flash_firmware "$port"
                        break
                    fi
                done
            fi
        fi
    else
        show_driver_guide
    fi

    # 显示主菜单
    main_menu
}

# 运行主函数
main "$@"
