@echo off
echo ========================================
echo ESP32 状态一致性修复验证脚本
echo ========================================
echo.

echo 🔧 步骤1: 编译ESP32项目...
call build_only.bat
if %ERRORLEVEL% neq 0 (
    echo ❌ 编译失败！
    pause
    exit /b 1
)
echo ✅ 编译成功！
echo.

echo 🔥 步骤2: 烧录到ESP32...
echo 请确认ESP32已连接到COM10端口
pause
call flash_com10.bat
if %ERRORLEVEL% neq 0 (
    echo ❌ 烧录失败！
    pause
    exit /b 1
)
echo ✅ 烧录成功！
echo.

echo 📊 步骤3: 验证修复效果
echo.
echo 请按以下步骤验证：
echo.
echo 1. 检查ESP32串口日志，应该看到：
echo    - "SBUS: ⚠️ No SBUS data for 5 seconds"
echo    - "DATA_INTEGRATION: 📊 状态摘要 - WiFi: ✅, SBUS: ❌, CAN: ❌"
echo.
echo 2. 访问云端监控界面：
echo    - URL: http://www.nagaflow.top/
echo    - 检查设备状态：WiFi=✅, SBUS=❌, CAN=❌
echo.
echo 3. 访问本地监控界面：
echo    - URL: http://192.168.6.109/
echo    - 检查设备状态：WiFi=✅, SBUS=❌, CAN=❌
echo.
echo 4. 对比两个界面，状态应该完全一致！
echo.
echo ✅ 如果两个界面显示一致，说明修复成功！
echo ❌ 如果仍有不一致，请检查代码或联系开发者。
echo.
pause
