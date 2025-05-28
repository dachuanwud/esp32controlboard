@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ESP32控制板CAN工具安装脚本
:: 版本: 1.0.0

echo.
echo ===============================================================
echo 🛠️ ESP32控制板CAN工具安装脚本 v1.0.0
echo ===============================================================
echo.
echo 📋 此脚本将帮助您:
echo    • 检查Python环境
echo    • 安装必要的依赖包
echo    • 验证工具功能
echo    • 创建快捷方式
echo.

:: 检查Python是否安装
echo 🔍 检查Python环境...
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ 错误: 未找到Python
    echo.
    echo 💡 请先安装Python 3.7或更高版本:
    echo    1. 访问 https://www.python.org/downloads/
    echo    2. 下载并安装最新版本的Python
    echo    3. 安装时勾选 "Add Python to PATH"
    echo    4. 重新运行此脚本
    echo.
    pause
    exit /b 1
)

:: 显示Python版本
for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
echo ✅ Python版本: %PYTHON_VERSION%

:: 检查pip是否可用
echo.
echo 🔍 检查pip包管理器...
pip --version >nul 2>&1
if errorlevel 1 (
    echo ❌ 错误: pip不可用
    echo 💡 请重新安装Python并确保包含pip
    pause
    exit /b 1
)

for /f "tokens=2" %%i in ('pip --version 2^>^&1') do set PIP_VERSION=%%i
echo ✅ pip版本: %PIP_VERSION%

:: 检查是否在tools目录
echo.
echo 🔍 检查当前目录...
if not exist "requirements.txt" (
    echo ❌ 错误: 请在tools目录下运行此脚本
    echo 💡 当前目录应包含requirements.txt文件
    echo    请切换到ESP32控制板项目的tools目录
    pause
    exit /b 1
)
echo ✅ 目录检查通过

:: 升级pip（可选）
echo.
set /p upgrade_pip="🔧 是否升级pip到最新版本? (y/N): "
if /i "%upgrade_pip%"=="y" (
    echo 📦 正在升级pip...
    python -m pip install --upgrade pip -i https://pypi.tuna.tsinghua.edu.cn/simple/
    if errorlevel 1 (
        echo ⚠️ pip升级失败，但不影响后续安装
    ) else (
        echo ✅ pip升级完成
    )
)

:: 安装依赖包
echo.
echo 📦 正在安装Python依赖包...
echo    使用清华大学镜像源加速下载...

pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple/
if errorlevel 1 (
    echo.
    echo ❌ 依赖包安装失败
    echo.
    echo 💡 尝试解决方案:
    echo    1. 检查网络连接
    echo    2. 手动运行: pip install python-can pyserial
    echo    3. 或使用官方源: pip install -r requirements.txt
    echo.
    pause
    exit /b 1
)

echo ✅ 依赖包安装完成

:: 验证安装
echo.
echo 🧪 验证安装...
python -c "import can; print('✅ python-can 导入成功')" 2>nul
if errorlevel 1 (
    echo ❌ python-can验证失败
    pause
    exit /b 1
)

python -c "import serial; print('✅ pyserial 导入成功')" 2>nul
if errorlevel 1 (
    echo ❌ pyserial验证失败
    pause
    exit /b 1
)

echo ✅ 所有依赖包验证通过

:: 运行测试
echo.
set /p run_test="🧪 是否运行功能测试? (Y/n): "
if /i not "%run_test%"=="n" (
    echo.
    echo 🧪 正在运行功能测试...
    python test_can_tools.py
    if errorlevel 1 (
        echo ⚠️ 测试过程中出现问题，但不影响基本功能
    ) else (
        echo ✅ 功能测试通过
    )
)

:: 创建桌面快捷方式（可选）
echo.
set /p create_shortcut="🔗 是否创建桌面快捷方式? (Y/n): "
if /i not "%create_shortcut%"=="n" (
    set "desktop=%USERPROFILE%\Desktop"
    set "shortcut_path=!desktop!\ESP32 CAN工具.lnk"
    set "target_path=%CD%\can_tool.bat"
    
    :: 使用PowerShell创建快捷方式
    powershell -Command "$WshShell = New-Object -comObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('!shortcut_path!'); $Shortcut.TargetPath = '!target_path!'; $Shortcut.WorkingDirectory = '%CD%'; $Shortcut.Description = 'ESP32控制板CAN工具'; $Shortcut.Save()" >nul 2>&1
    
    if exist "!shortcut_path!" (
        echo ✅ 桌面快捷方式创建成功
    ) else (
        echo ⚠️ 快捷方式创建失败，请手动创建
    )
)

:: 安装完成
echo.
echo ===============================================================
echo 🎉 安装完成！
echo ===============================================================
echo.
echo 📋 安装摘要:
echo    ✅ Python环境: %PYTHON_VERSION%
echo    ✅ 依赖包: python-can, pyserial
echo    ✅ 工具脚本: 已就绪
echo.
echo 🚀 快速开始:
echo    1. 双击桌面快捷方式 "ESP32 CAN工具"
echo    2. 或运行: can_tool.bat
echo    3. 或运行: python quick_can_setup.py
echo.
echo 📖 详细说明: 请查看 README.md
echo.
echo 💡 提示: 使用前请确保CAN设备已正确连接！
echo ===============================================================

pause
