# 🔍 ESP32 连接诊断报告

生成时间: $(date)

---

## ❌ 诊断结果：ESP32 设备未检测到

### 检查项目

| 检查项 | 状态 | 说明 |
|--------|------|------|
| USB 串口设备 | ❌ 未检测到 | 系统未识别任何串口设备 |
| CH340 驱动 | ❌ 未安装 | 内核扩展未加载 |
| 驱动文件 | ❌ 不存在 | /Library/Extensions/ 中无驱动 |
| ESP32 连接 | ❌ 未连接 | 没有看到设备在 USB 总线上 |

---

## 🔧 问题分析

### 根本原因
1. **CH340 驱动未安装** - 这是主要问题
2. **ESP32 可能未连接** - 或者连接后系统无法识别

### 当前状态
- 检测到的设备：仅蓝牙和 WiFi 调试端口
- USB 总线：只看到 USB Hub，无串口设备
- 驱动状态：完全未安装

---

## ✅ 解决方案

### 步骤 1: 安装 CH340 驱动

**选择以下任一方法：**

#### 方法 A: 手动下载安装（推荐，最简单）

```bash
# 1. 下载驱动
curl -L -o ~/Downloads/CH34x_Install_V1.4.pkg \
  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver/raw/master/CH34x_Install_V1.4.pkg

# 2. 打开 Downloads 文件夹
open ~/Downloads
```

然后：
1. 双击 `CH34x_Install_V1.4.pkg`
2. 按照安装向导操作
3. 输入管理员密码
4. **重要**: 安装后打开「系统设置 → 隐私与安全性」，点击「允许」

#### 方法 B: 使用安装脚本

```bash
cd /Users/lishechuan/Downloads/esp32controlboard
./install_ch340_driver.sh
# 选择选项 1 或 2
```

---

### 步骤 2: 允许系统扩展（关键步骤！）

安装驱动后，macOS 会阻止它运行。**必须手动允许**：

1. 打开 **系统设置** (⚙️)
2. 点击 **隐私与安全性**
3. 向下滚动到底部
4. 找到提示：「系统软件来自开发者 "Jiangsu Qinheng Co., Ltd." 已被阻止」
5. 点击 **允许** 按钮
6. 可能需要输入密码

**如果没有看到提示**：
- 重启 Mac
- 或在终端运行：`sudo kextload /Library/Extensions/usbserial.kext`

---

### 步骤 3: 连接 ESP32 并验证

1. **拔掉 ESP32**（如果已连接）
2. **等待 5 秒**
3. **重新插入 ESP32**
4. **检查设备**：

```bash
# 查看串口设备
ls -l /dev/cu.*

# 查看 USB 设备
system_profiler SPUSBDataType | grep -A 10 -i "ch340\|serial"

# 运行检测工具
cd /Users/lishechuan/Downloads/esp32controlboard
./detect_esp32.sh
```

成功后会看到 `/dev/cu.usbserial-*` 或 `/dev/cu.wchusbserial*`

---

## 🔍 额外检查项

### 检查 USB 线是否支持数据传输

**症状**: 驱动已安装但仍检测不到
**原因**: 使用了仅充电的 USB 线

**解决**:
- 更换为数据线（通常较粗）
- 或使用 ESP32 开发板附带的原装线

### 检查 ESP32 是否正常

1. **电源指示灯**: ESP32 连接后应该有 LED 亮起
2. **尝试其他 USB 口**: 换个 Mac 的 USB 端口
3. **直连**: 如果使用 USB Hub，尝试直接连接到 Mac

---

## 📋 完整安装流程（复制执行）

**在终端中按顺序执行：**

```bash
# 1. 下载驱动
echo "正在下载 CH340 驱动..."
curl -L -o ~/Downloads/CH34x_Install_V1.4.pkg \
  https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver/raw/master/CH34x_Install_V1.4.pkg

# 2. 打开安装包
echo "请双击打开的 pkg 文件进行安装"
open ~/Downloads/CH34x_Install_V1.4.pkg

# 3. 等待安装完成后执行
read -p "驱动安装完成后按 Enter 继续..."

# 4. 提醒允许系统扩展
echo ""
echo "=================================="
echo "重要！请立即执行以下步骤："
echo "1. 打开: 系统设置 → 隐私与安全性"
echo "2. 找到底部的「系统扩展被阻止」提示"
echo "3. 点击「允许」按钮"
echo "=================================="
read -p "完成后按 Enter 继续..."

# 5. 检查驱动状态
echo ""
echo "检查驱动状态..."
kextstat | grep -i ch34 && echo "✅ 驱动已加载" || echo "❌ 驱动未加载，请重启 Mac"

# 6. 提示连接设备
echo ""
echo "现在请："
echo "1. 拔掉 ESP32 设备"
echo "2. 等待 3 秒"
echo "3. 重新插入 ESP32"
read -p "插入后按 Enter 继续..."

# 7. 检测设备
echo ""
echo "检测设备..."
ls -l /dev/cu.* | grep -E "usbserial|wchusbserial" && echo "✅ ESP32 已检测到！" || echo "❌ 仍未检测到，可能需要重启 Mac"

# 8. 运行检测工具
cd /Users/lishechuan/Downloads/esp32controlboard
source ~/esp/esp-idf/export.sh
./detect_esp32.sh
```

---

## 🎯 预期结果

安装成功后，你应该看到：

```bash
$ ls -l /dev/cu.*
crw-rw-rw-  1 root  wheel  ... /dev/cu.Bluetooth-Incoming-Port
crw-rw-rw-  1 root  wheel  ... /dev/cu.usbserial-0001          ← 这是 ESP32！
crw-rw-rw-  1 root  wheel  ... /dev/cu.wlan-debug
```

然后运行 `./detect_esp32.sh` 会自动识别并提供烧录选项。

---

## 🆘 如果还是不行

### 最后的排查步骤：

1. **完全重启 Mac**
   ```bash
   sudo reboot
   ```

2. **检查 ESP32 本身**
   - 尝试在其他电脑上测试
   - 检查是否有物理损坏
   - 确认使用正确的 USB 端口（ESP32 通常有 USB 和串口两个口）

3. **联系我**
   - 告诉我重启后的检测结果
   - 提供 ESP32 开发板的型号
   - 截图系统设置中的「隐私与安全性」页面

---

## 📞 需要帮助？

如果按照上述步骤操作后仍有问题，请提供：
- [ ] 驱动安装的截图
- [ ] 系统设置 → 隐私与安全性的截图
- [ ] `ls -l /dev/cu.*` 的输出
- [ ] ESP32 开发板的型号和照片

---

生成此报告的命令：
```bash
cat /Users/lishechuan/Downloads/esp32controlboard/diagnostic_report.md
```
