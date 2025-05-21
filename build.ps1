# ESP32 Control Board Build Script

Write-Host "Building ESP32 Control Board project..." -ForegroundColor Green

# 设置ESP-IDF环境变量
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.4.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.4_py3.11_env"
$env:PATH = "$env:IDF_PYTHON_ENV_PATH\Scripts;C:\Users\Administrator\.espressif\tools\ninja\1.12.1;C:\Users\Administrator\.espressif\tools\cmake\3.30.2\bin;C:\Users\Administrator\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;$env:PATH"

# 修复CMake gdbinit错误
$GDBINIT_CMAKE = "$env:IDF_PATH\tools\cmake\gdbinit.cmake"
$GDBINIT_CMAKE_BAK = "$env:IDF_PATH\tools\cmake\gdbinit.cmake.bak"
if (-not (Test-Path $GDBINIT_CMAKE_BAK)) {
    Copy-Item $GDBINIT_CMAKE $GDBINIT_CMAKE_BAK
    (Get-Content $GDBINIT_CMAKE) -replace 'file\(TO_CMAKE_PATH \$ENV{ESP_ROM_ELF_DIR} ESP_ROM_ELF_DIR\)', 'file(TO_CMAKE_PATH "$ENV{ESP_ROM_ELF_DIR}" ESP_ROM_ELF_DIR)' | Set-Content $GDBINIT_CMAKE
}

# 编译项目
Write-Host "Running idf.py build..." -ForegroundColor Yellow
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" build

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
} else {
    Write-Host "Build failed!" -ForegroundColor Red
}

Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
