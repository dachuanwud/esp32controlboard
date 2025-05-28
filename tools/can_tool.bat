@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ESP32控制板CAN工具启动脚本
:: 版本: 1.0.0

echo.
echo ===============================================================
echo 🚗 ESP32控制板CAN工具 v1.0.0
echo ===============================================================
echo.

:: 检查Python是否安装
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ 错误: 未找到Python
    echo 💡 请先安装Python 3.7或更高版本
    echo    下载地址: https://www.python.org/downloads/
    pause
    exit /b 1
)

:: 显示Python版本
for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
echo ✅ Python版本: %PYTHON_VERSION%

:: 检查是否在tools目录
if not exist "can_tool.py" (
    echo ❌ 错误: 请在tools目录下运行此脚本
    echo 💡 当前目录应包含can_tool.py文件
    pause
    exit /b 1
)

:: 检查依赖是否安装
echo.
echo 🔍 检查Python依赖包...
python -c "import can" >nul 2>&1
if errorlevel 1 (
    echo ⚠️ 未安装python-can包，正在自动安装...
    echo.
    pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple/
    if errorlevel 1 (
        echo ❌ 依赖包安装失败
        echo 💡 请手动运行: pip install -r requirements.txt
        pause
        exit /b 1
    )
    echo ✅ 依赖包安装完成
) else (
    echo ✅ 依赖包检查通过
)

:: 显示菜单
echo.
echo 🎯 请选择运行模式:
echo    1. 交互式模式 (推荐)
echo    2. 检测模式 (仅检测CAN接口)
echo    3. 监控模式 (实时监控CAN总线)
echo    4. 电机测试模式 (自动电机测试)
echo    5. 查看帮助信息
echo    6. 退出
echo.

set /p choice="请输入选择 (1-6): "

if "%choice%"=="1" (
    echo.
    echo 🚀 启动交互式模式...
    python can_tool.py
) else if "%choice%"=="2" (
    echo.
    echo 🔍 启动检测模式...
    python can_tool.py --detect
) else if "%choice%"=="3" (
    echo.
    echo 👁️ 启动监控模式...
    echo 💡 按Ctrl+C停止监控
    python can_tool.py --monitor
) else if "%choice%"=="4" (
    echo.
    echo 🚗 启动电机测试模式...
    python can_tool.py --test-motor
) else if "%choice%"=="5" (
    echo.
    echo 📖 显示帮助信息...
    python can_tool.py --help
) else if "%choice%"=="6" (
    echo.
    echo 👋 退出程序
    exit /b 0
) else (
    echo.
    echo ❌ 无效选择，请重新运行脚本
)

echo.
pause
