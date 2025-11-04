#!/bin/bash
# CH340 驱动快速安装脚本

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

clear
echo -e "${CYAN}========================================================${NC}"
echo -e "${CYAN}        CH340 驱动快速安装向导${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""

# 步骤 1: 下载驱动
echo -e "${BLUE}[步骤 1/4]${NC} 下载 CH340 驱动..."
curl -L -o ~/Downloads/CH34x_Install_V1.4.pkg \
  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver/raw/master/CH34x_Install_V1.4.pkg

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 下载完成${NC}"
else
    echo -e "${RED}✗ 下载失败，请检查网络连接${NC}"
    exit 1
fi

echo ""
echo -e "${BLUE}[步骤 2/4]${NC} 打开安装包..."
open ~/Downloads/CH34x_Install_V1.4.pkg
sleep 2

echo ""
echo -e "${CYAN}========================================================${NC}"
echo -e "${YELLOW}⚠️  重要提示！请仔细阅读：${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""
echo "现在应该已经打开了安装窗口，请："
echo ""
echo "  1️⃣  双击 pkg 文件（如果没有自动打开）"
echo "  2️⃣  点击「继续」按钮"
echo "  3️⃣  点击「安装」按钮"
echo "  4️⃣  输入你的 Mac 密码"
echo "  5️⃣  等待安装完成"
echo ""
read -p "安装完成后，按 Enter 继续..."

echo ""
echo -e "${BLUE}[步骤 3/4]${NC} 允许系统扩展..."
echo ""
echo -e "${CYAN}========================================================${NC}"
echo -e "${YELLOW}🔐 现在必须允许系统扩展（否则驱动不会工作）：${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""
echo "  1️⃣  打开「系统设置」(System Settings)"
echo "  2️⃣  点击「隐私与安全性」(Privacy & Security)"
echo "  3️⃣  向下滚动到页面底部"
echo "  4️⃣  找到：「系统软件来自开发者 Jiangsu Qinheng 已被阻止」"
echo "  5️⃣  点击「允许」按钮"
echo "  6️⃣  输入密码确认"
echo ""
echo -e "${RED}注意：如果没有看到提示，可能需要重启 Mac${NC}"
echo ""
read -p "完成后，按 Enter 继续..."

echo ""
echo -e "${BLUE}[步骤 4/4]${NC} 验证安装..."
echo ""

# 检查驱动
if kextstat | grep -i "ch34" >/dev/null 2>&1; then
    echo -e "${GREEN}✅ 驱动已成功加载！${NC}"
else
    echo -e "${YELLOW}⚠️  驱动未加载${NC}"
    echo ""
    echo "可能的原因："
    echo "  • 还没有在系统设置中点击「允许」"
    echo "  • 需要重启 Mac"
    echo ""
    echo "请先完成系统扩展允许，或重启 Mac 后再继续"
    echo ""
fi

echo ""
echo -e "${CYAN}========================================================${NC}"
echo -e "${GREEN}安装流程完成！${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""
echo "接下来："
echo ""
echo "  1. ${YELLOW}拔掉${NC} ESP32 设备（如果已连接）"
echo "  2. 等待 3 秒"
echo "  3. ${YELLOW}重新插入${NC} ESP32 设备"
echo "  4. 运行检测工具："
echo ""
echo "     ${CYAN}cd /Users/lishechuan/Downloads/esp32controlboard${NC}"
echo "     ${CYAN}./detect_esp32.sh${NC}"
echo ""
echo -e "${YELLOW}如果仍然检测不到设备，请重启 Mac 后再试${NC}"
echo ""
