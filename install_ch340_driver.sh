#!/bin/bash
# ========================================================
# CH340 USB 转串口驱动安装脚本
# ========================================================

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}========================================================${NC}"
echo -e "${CYAN}        CH340/CH341 USB 转串口驱动安装工具${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""

echo -e "${BLUE}安装选项:${NC}"
echo ""
echo "  1) 使用 Homebrew 安装 (推荐)"
echo "  2) 下载 pkg 安装包手动安装"
echo "  3) 检查驱动是否已安装"
echo "  4) 卸载旧版本驱动"
echo "  5) 退出"
echo ""
echo -n "请选择 [1-5]: "
read -r choice

case $choice in
    1)
        echo ""
        echo -e "${YELLOW}[方法 1] 使用 Homebrew 安装${NC}"
        echo ""
        echo "正在添加驱动仓库..."
        brew tap mengbo/ch340g-ch34g-ch34x-mac-os-x-driver https://github.com/mengbo/ch340g-ch34g-ch34x-mac-os-x-driver

        echo ""
        echo -e "${YELLOW}准备安装驱动...${NC}"
        echo "这将需要您的管理员密码"
        echo ""

        # 使用 brew 安装
        brew install --cask mengbo/ch340g-ch34g-ch34x-mac-os-x-driver/wch-ch34x-usb-serial-driver

        echo ""
        echo -e "${GREEN}安装完成！${NC}"
        echo ""
        echo -e "${YELLOW}重要提示:${NC}"
        echo "1. 如果系统提示「系统扩展被阻止」，请按以下步骤操作："
        echo "   - 打开 ${CYAN}系统设置 → 隐私与安全性${NC}"
        echo "   - 找到「已阻止系统扩展」部分"
        echo "   - 点击 ${CYAN}允许${NC} 按钮"
        echo ""
        echo "2. 重新插拔 USB 设备"
        echo ""
        echo "3. 运行检测: ${CYAN}./detect_esp32.sh${NC}"
        ;;

    2)
        echo ""
        echo -e "${YELLOW}[方法 2] 手动下载安装${NC}"
        echo ""
        echo "步骤 1: 下载驱动"
        echo "  访问: ${CYAN}https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver${NC}"
        echo "  或直接下载: ${CYAN}https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver/raw/master/CH34x_Install_V1.4.pkg${NC}"
        echo ""
        echo "步骤 2: 双击 .pkg 文件安装"
        echo ""
        echo "步骤 3: 如果提示「无法验证开发者」:"
        echo "  - 打开 ${CYAN}系统设置 → 隐私与安全性${NC}"
        echo "  - 找到被阻止的安装包"
        echo "  - 点击 ${CYAN}仍要打开${NC}"
        echo ""
        echo "步骤 4: 完成安装后"
        echo "  - 打开 ${CYAN}系统设置 → 隐私与安全性${NC}"
        echo "  - 允许系统扩展"
        echo "  - 重新插拔 USB 设备"
        echo ""

        echo "是否现在下载驱动? (y/n)"
        read -r download
        if [[ "$download" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            echo ""
            echo "正在下载驱动..."
            curl -L -o ~/Downloads/CH34x_Install_V1.4.pkg \
                https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver/raw/master/CH34x_Install_V1.4.pkg

            if [ $? -eq 0 ]; then
                echo -e "${GREEN}下载完成！${NC}"
                echo "文件保存在: ${CYAN}~/Downloads/CH34x_Install_V1.4.pkg${NC}"
                echo ""
                echo "请双击该文件进行安装"
                open ~/Downloads
            else
                echo -e "${RED}下载失败${NC}"
            fi
        fi
        ;;

    3)
        echo ""
        echo -e "${YELLOW}检查驱动状态...${NC}"
        echo ""

        # 检查内核扩展
        if kextstat | grep -i "ch34" >/dev/null; then
            echo -e "${GREEN}✓ CH340 驱动已加载${NC}"
            kextstat | grep -i "ch34"
        else
            echo -e "${RED}✗ CH340 驱动未加载${NC}"
        fi

        echo ""

        # 检查驱动文件
        if [ -d "/Library/Extensions/usbserial.kext" ]; then
            echo -e "${GREEN}✓ 驱动文件已安装${NC}"
            echo "  位置: /Library/Extensions/usbserial.kext"
        else
            echo -e "${RED}✗ 驱动文件未找到${NC}"
        fi

        echo ""

        # 检查设备
        echo "当前检测到的串口设备:"
        ls -l /dev/cu.* 2>/dev/null || echo "  无设备"
        ;;

    4)
        echo ""
        echo -e "${YELLOW}卸载旧版本驱动${NC}"
        echo ""
        echo -e "${RED}警告: 这将删除 CH340 驱动${NC}"
        echo "确定要继续吗? (y/n)"
        read -r confirm

        if [[ "$confirm" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            echo ""
            echo "正在卸载..."

            # 卸载驱动
            if [ -d "/Library/Extensions/usbserial.kext" ]; then
                sudo rm -rf /Library/Extensions/usbserial.kext
                echo -e "${GREEN}✓ 驱动文件已删除${NC}"
            fi

            # 使用 Homebrew 卸载
            brew uninstall --cask wch-ch34x-usb-serial-driver 2>/dev/null

            echo ""
            echo -e "${GREEN}卸载完成${NC}"
            echo "请重启电脑以完全生效"
        fi
        ;;

    5)
        echo "退出"
        exit 0
        ;;

    *)
        echo -e "${RED}无效选项${NC}"
        exit 1
        ;;
esac

echo ""
echo -e "${CYAN}========================================================${NC}"
echo -e "${GREEN}操作完成！${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""
echo "下一步:"
echo "  1. 连接 ESP32 设备"
echo "  2. 运行检测: ${CYAN}cd /Users/lishechuan/Downloads/esp32controlboard && ./detect_esp32.sh${NC}"
echo ""
