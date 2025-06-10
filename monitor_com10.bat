@echo off
echo ========================================================
echo           ESP32 Serial Monitor Tool
echo ========================================================
echo.
echo Project: esp32controlboard
echo Target: Monitor COM10 output
echo.
echo Starting serial monitor...
echo ========================================

REM 激活ESP-IDF环境
echo [INFO] Setting up ESP-IDF environment...
call C:\Espressif\frameworks\esp-idf-v5.4.1\export.bat

REM 启动串口监控
echo [INFO] Starting serial monitor on COM10...
echo ========================================
idf.py -p COM10 monitor

echo.
echo ========================================
echo [INFO] Serial monitor session ended
echo ========================================
pause
