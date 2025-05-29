@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
REM ========================================================
REM ESP32 Build Only Script - Compile without flashing
REM ========================================================

title ESP32 Build Tool
color 0B

REM Configuration variables
set PROJECT_NAME=esp32controlboard
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.4.1
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env

REM Switch to project directory
cd /d %~dp0

cls
echo ========================================================
echo            ESP32 Project Build Tool
echo ========================================================
echo.
echo Project: %PROJECT_NAME%
echo Target: Build project
echo.
echo Starting build process...
echo ========================================

REM Check if ESP-IDF path exists
if not exist "%IDF_PATH%" (
    echo [ERROR] ESP-IDF path not found: %IDF_PATH%
    echo Please check ESP-IDF installation.
    pause
    exit /b 1
)

REM Check if Python environment exists
if not exist "%IDF_PYTHON_ENV_PATH%" (
    echo [ERROR] Python environment not found: %IDF_PYTHON_ENV_PATH%
    echo Please check ESP-IDF Python environment installation.
    pause
    exit /b 1
)

echo [INFO] Setting up ESP-IDF environment...
REM Set ESP-IDF environment
call "%IDF_PATH%\export.bat"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to set ESP-IDF environment
    pause
    exit /b 1
)

echo [SUCCESS] ESP-IDF environment configured successfully
echo.

echo [INFO] Building ESP32 project...
REM Build the project
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build

REM Check if build was successful by checking for output files
if exist "build\esp32controlboard.bin" (
    echo.
    echo ========================================
    echo [SUCCESS] BUILD COMPLETED SUCCESSFULLY!
    echo ========================================
    echo.
    echo Build Information:
    if exist "build\esp32controlboard.bin" (
        for %%F in ("build\esp32controlboard.bin") do echo   Application: %%~nxF ^(%%~zF bytes^)
    ) else (
        echo   [WARNING] Application binary not found: build\esp32controlboard.bin
    )
    if exist "build\bootloader\bootloader.bin" (
        for %%F in ("build\bootloader\bootloader.bin") do echo   Bootloader: %%~nxF ^(%%~zF bytes^)
    ) else (
        echo   [WARNING] Bootloader binary not found: build\bootloader\bootloader.bin
    )
    if exist "build\partition_table\partition-table.bin" (
        for %%F in ("build\partition_table\partition-table.bin") do echo   Partition Table: %%~nxF ^(%%~zF bytes^)
    ) else (
        echo   [WARNING] Partition table not found: build\partition_table\partition-table.bin
    )
    echo   Build completed: %date% %time%
    echo.
    echo Output Files:
    if exist "build\esp32controlboard.bin" (
        echo   [OK] build\esp32controlboard.bin ^(main application^)
    )
    if exist "build\bootloader\bootloader.bin" (
        echo   [OK] build\bootloader\bootloader.bin ^(bootloader^)
    )
    if exist "build\partition_table\partition-table.bin" (
        echo   [OK] build\partition_table\partition-table.bin ^(partition table^)
    )
    echo.
    echo Next Steps:
    echo   1. Flash to ESP32: flash_com10.bat
    echo   2. Or use: idf.py -p COM10 flash monitor
    echo   3. For OTA: Use web interface after initial flash
    echo.
    echo Flash Configuration:
    echo   - Flash Size: 16MB
    echo   - Partition: Custom OTA partitions (16MB optimized)
    echo   - Target: ESP32
    goto :end
) else (
    echo.
    echo ========================================
    echo [ERROR] BUILD FAILED!
    echo ========================================
    echo.
    echo Please check the error messages above and fix the issues.
    echo.
    echo Common Solutions:
    echo   1. Check ESP-IDF environment installation
    echo   2. Verify all source files syntax
    echo   3. Check CMakeLists.txt configuration
    echo   4. Clean build directory: Remove-Item -Recurse -Force build
    echo   5. Check component dependencies in main\CMakeLists.txt
    echo   6. Verify Flash size configuration ^(should be 16MB^)
    echo.
    echo Troubleshooting Steps:
    echo   - Check build logs in: build\log\
    echo   - Verify sdkconfig Flash settings
    echo   - Ensure all required components are available
)

:end
echo.
pause
