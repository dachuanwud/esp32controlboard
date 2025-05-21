@echo off
title ESP32 Control Board Build
color 0A
cls
echo ========================================================
echo            ESP32 Control Board Auto Build
echo ========================================================
echo.

REM Setup ESP-IDF environment variables
echo Setting up ESP-IDF environment...
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.4.1
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV_PATH%\Scripts;C:\Users\Administrator\.espressif\tools\ninja\1.12.1;C:\Users\Administrator\.espressif\tools\cmake\3.30.2\bin;C:\Users\Administrator\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;%PATH%
set ESP_ROM_ELF_DIR=C:\Users\Administrator\.espressif\tools\esp-rom-elfs\20240201

REM Create constraints file
echo Creating constraints file...
if not exist "C:\Users\Administrator\.espressif" mkdir "C:\Users\Administrator\.espressif"
copy "C:\Espressif\espidf.constraints.v5.4.txt" "C:\Users\Administrator\.espressif\espidf.constraints.v5.4.txt" >nul

REM Fix Git ownership issues
echo Fixing Git ownership issues...
git config --global --add safe.directory C:/Espressif/frameworks/esp-idf-v5.4.1 >nul 2>&1
git config --global --add safe.directory C:/Espressif/frameworks/esp-idf-v5.4.1/components/openthread/openthread >nul 2>&1

REM Fix CMake gdbinit error
echo Fixing CMake gdbinit error...
set GDBINIT_CMAKE=%IDF_PATH%\tools\cmake\gdbinit.cmake
set GDBINIT_CMAKE_BAK=%IDF_PATH%\tools\cmake\gdbinit.cmake.bak
if not exist "%GDBINIT_CMAKE_BAK%" (
    copy "%GDBINIT_CMAKE%" "%GDBINIT_CMAKE_BAK%" >nul
    powershell -Command "(Get-Content '%GDBINIT_CMAKE%') -replace 'file\(TO_CMAKE_PATH \$ENV{ESP_ROM_ELF_DIR} ESP_ROM_ELF_DIR\)', 'file(TO_CMAKE_PATH \"\$ENV{ESP_ROM_ELF_DIR}\" ESP_ROM_ELF_DIR)' | Set-Content '%GDBINIT_CMAKE%'" >nul 2>&1
)

REM Go to project directory
cd /d %~dp0

REM Clean and build project
echo.
echo Cleaning project...
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" fullclean >nul 2>&1

echo.
echo Building project...
echo This may take a few minutes, please wait...
echo.

REM Run the build command and capture output
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build >build_log.txt 2>&1

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    color 0A
    cls
    echo ========================================================
    echo                 BUILD SUCCESSFUL!
    echo ========================================================
    echo.
    echo Binary size:
    findstr /C:"esp32controlboard.bin binary size" build_log.txt
    echo.
    echo To flash the project, run:
    echo idf.py -p [PORT] flash
    echo.
    echo To monitor the output, run:
    echo idf.py -p [PORT] monitor
    echo.
    echo Build log saved to build_log.txt
    echo.
    echo Window will close in 5 seconds...
) else (
    color 0C
    cls
    echo ========================================================
    echo                  BUILD FAILED!
    echo ========================================================
    echo.
    echo Please check build_log.txt for details.
    echo.
    echo Window will close in 5 seconds...
)

REM Wait 5 seconds before closing
timeout /t 5 >nul

REM Delete log file if you don't want to keep it
REM del build_log.txt

REM Exit without pause
exit
