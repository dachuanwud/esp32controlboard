@echo off
title ESP32 Flash Tool - COM10
cls

echo ========================================================
echo           ESP32 Flash Tool - COM10
echo ========================================================
echo.

cd /d E:\lishechuan\bot\esp32controlboard

echo Setting ESP-IDF environment...
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV_PATH%\Scripts;%PATH%

echo Checking build files...
if not exist "build\esp32controlboard.bin" (
    echo ERROR: Build file not found!
    pause
    exit /b 1
)

echo Build files OK
echo.
echo Target: ESP32
echo Port: COM10
echo Baud: 460800
echo.
echo Starting flash process...
echo ========================================

"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m esptool --chip esp32 -p COM10 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size 16MB 0x1000 build\bootloader\bootloader.bin 0x20000 build\esp32controlboard.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin

echo ========================================

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: Flash completed successfully!
    echo ESP32 should restart and run the new program.
    echo.
    echo Start serial monitor? (y/n)
    set /p monitor_choice=Choice:
    if /i "%monitor_choice%"=="y" (
        echo Starting monitor on COM10...
        "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m serial.tools.miniterm COM10 115200
    )
) else (
    echo.
    echo ERROR: Flash failed!
    echo Please check ESP32 connection to COM10.
)

echo.
pause
