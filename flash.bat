@echo off
setlocal enabledelayedexpansion
title ESP32 Control Board Flash
color 0B
cls
echo ========================================================
echo            ESP32 Control Board Flash Tool
echo ========================================================
echo.

REM Setup ESP-IDF environment variables
echo Setting up ESP-IDF environment...
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.4.1
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV_PATH%\Scripts;C:\Users\Administrator\.espressif\tools\ninja\1.12.1;C:\Users\Administrator\.espressif\tools\cmake\3.30.2\bin;C:\Users\Administrator\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;%PATH%

REM Go to project directory
cd /d %~dp0

REM Get available COM ports
echo Detecting available COM ports...
echo.
powershell -Command "[System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { Write-Host $_ }" > comports.txt
set /a count=0
for /f "tokens=*" %%a in (comports.txt) do (
    set /a count+=1
    set port[!count!]=%%a
    echo !count!: %%a
)
del comports.txt

REM If no COM ports found
if %count% EQU 0 (
    echo No COM ports detected!
    echo Please connect your ESP32 device and try again.
    echo.
    echo Press any key to exit...
    pause > nul
    exit
)

REM Ask user to select a COM port
echo.
set /p choice=Enter the number of the COM port to use (1-%count%):

REM Validate choice
if %choice% LSS 1 (
    echo Invalid choice!
    echo Press any key to exit...
    pause > nul
    exit
)
if %choice% GTR %count% (
    echo Invalid choice!
    echo Press any key to exit...
    pause > nul
    exit
)

REM Get the selected COM port
set selected_port=!port[%choice%]!

echo.
echo Selected COM port: %selected_port%
echo.
echo Flashing to %selected_port%...
echo This may take a few minutes, please wait...
echo.

REM Flash the project
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" -p %selected_port% flash

if %ERRORLEVEL% EQU 0 (
    color 0A
    echo.
    echo ========================================================
    echo                 FLASH SUCCESSFUL!
    echo ========================================================
    echo.
    echo To monitor the output, run:
    echo idf.py -p %selected_port% monitor
    echo.
    echo Press any key to exit...
) else (
    color 0C
    echo.
    echo ========================================================
    echo                  FLASH FAILED!
    echo ========================================================
    echo.
    echo Press any key to exit...
)

pause > nul
