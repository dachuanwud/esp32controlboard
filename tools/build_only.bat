@echo off
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

REM Set ESP-IDF environment
call "%IDF_PATH%\export.bat"

REM Build the project
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build
set BUILD_RESULT=%ERRORLEVEL%

if %BUILD_RESULT% EQU 0 (
    echo ========================================
    echo BUILD SUCCESS!
    echo ========================================
    echo.
    echo Build Information:
    if exist "build\%PROJECT_NAME%.bin" (
        for %%F in ("build\%PROJECT_NAME%.bin") do echo   Application size: %%~zF bytes
    )
    if exist "build\bootloader\bootloader.bin" (
        for %%F in ("build\bootloader\bootloader.bin") do echo   Bootloader size: %%~zF bytes
    )
    echo   Build time: %date% %time%
    echo.
    echo Output files:
    echo   - build\%PROJECT_NAME%.bin (main application)
    echo   - build\bootloader\bootloader.bin (bootloader)
    echo   - build\partition_table\partition-table.bin (partition table)
    echo.
    echo Next steps:
    echo   - Use flash_com10.bat to flash to ESP32
    echo   - Or use quick.bat for build+flash+monitor
) else (
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
    echo.
    echo Please check the error messages above and fix the issues.
    echo.
    echo Common solutions:
    echo 1. Check ESP-IDF environment installation
    echo 2. Verify all source files syntax
    echo 3. Check CMakeLists.txt configuration
    echo 4. Clean build directory and retry
)

echo.
pause
