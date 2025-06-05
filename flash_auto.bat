@echo off
title ESP32 Auto Flash Tool
cls

echo ========================================================
echo           ESP32 Auto Flash Tool
echo ========================================================
echo.

cd /d E:\lishechuan\bot\esp32controlboard

echo Setting ESP-IDF environment...
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV_PATH%\Scripts;%PATH%

echo Checking build files...
if not exist "build\esp32controlboard.bin" (
    echo ERROR: Build file not found!
    echo Please run build_only.bat first.
    pause
    exit /b 1
)

echo Build files OK
echo.

echo Detecting available COM ports...
echo Available ports:
for /f "tokens=1" %%i in ('"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m serial.tools.list_ports') do (
    echo   %%i
    set "FIRST_PORT=%%i"
)

if not defined FIRST_PORT (
    echo ERROR: No COM ports found!
    echo Please check ESP32 connection.
    pause
    exit /b 1
)

echo.
echo Auto-detected port: %FIRST_PORT%
echo.
set /p port_choice=Use this port? (y/n) or enter custom port (e.g., COM9): 

if /i "%port_choice%"=="y" (
    set "TARGET_PORT=%FIRST_PORT%"
) else if /i "%port_choice%"=="n" (
    echo Please connect ESP32 and try again.
    pause
    exit /b 1
) else (
    set "TARGET_PORT=%port_choice%"
)

echo.
echo Target: ESP32
echo Port: %TARGET_PORT%
echo Baud: 460800
echo.
echo Starting flash process...
echo ========================================

"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m esptool --chip esp32 -p %TARGET_PORT% -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size 16MB 0x1000 build\bootloader\bootloader.bin 0x20000 build\esp32controlboard.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin

echo ========================================

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: Flash completed successfully!
    echo ESP32 should restart and run the new program.
    echo.
    echo Start serial monitor? (y/n)
    set /p monitor_choice=Choice:
    if /i "%monitor_choice%"=="y" (
        echo Starting monitor on %TARGET_PORT%...
        "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m serial.tools.miniterm %TARGET_PORT% 115200
    )
) else (
    echo.
    echo ERROR: Flash failed!
    echo Please check ESP32 connection to %TARGET_PORT%.
)

echo.
pause
