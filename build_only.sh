#!/bin/bash
# ========================================================
# ESP32 Build Only Script - Compile without flashing
# Linux version of build_only.bat
# ========================================================

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration variables
PROJECT_NAME="esp32controlboard"
IDF_PATH_DEFAULT="$HOME/esp/esp-idf"
IDF_PYTHON_ENV_DEFAULT="$HOME/.espressif/python_env/idf5.4_py3.11_env"

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${CYAN}========================================================${NC}"
    echo -e "${CYAN}            ESP32 Project Build Tool${NC}"
    echo -e "${CYAN}========================================================${NC}"
    echo
    echo -e "${BLUE}Project:${NC} $PROJECT_NAME"
    echo -e "${BLUE}Target:${NC} Build project"
    echo
    echo -e "${BLUE}Starting build process...${NC}"
    echo -e "${CYAN}========================================${NC}"
}

# Function to check file size
get_file_size() {
    if [ -f "$1" ]; then
        stat -c%s "$1" 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

# Function to format file size
format_size() {
    local size=$1
    if [ $size -gt 1048576 ]; then
        echo "$(($size / 1048576)) MB"
    elif [ $size -gt 1024 ]; then
        echo "$(($size / 1024)) KB"
    else
        echo "$size bytes"
    fi
}

# Function to check ESP-IDF installation
check_esp_idf() {
    print_info "Checking ESP-IDF installation..."
    
    # Check if idf.py is in PATH
    if command -v idf.py >/dev/null 2>&1; then
        print_success "ESP-IDF found in PATH"
        return 0
    fi
    
    # Check common installation paths
    local possible_paths=(
        "$HOME/esp/esp-idf"
        "/opt/esp-idf"
        "/usr/local/esp-idf"
        "$IDF_PATH"
    )
    
    for path in "${possible_paths[@]}"; do
        if [ -d "$path" ] && [ -f "$path/export.sh" ]; then
            print_success "ESP-IDF found at: $path"
            export IDF_PATH="$path"
            return 0
        fi
    done
    
    print_error "ESP-IDF not found!"
    echo
    echo "Please install ESP-IDF first:"
    echo "1. Install prerequisites:"
    echo "   sudo apt-get update"
    echo "   sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0"
    echo
    echo "2. Clone ESP-IDF:"
    echo "   mkdir -p ~/esp"
    echo "   cd ~/esp"
    echo "   git clone --recursive https://github.com/espressif/esp-idf.git"
    echo
    echo "3. Install ESP-IDF:"
    echo "   cd ~/esp/esp-idf"
    echo "   ./install.sh esp32"
    echo
    echo "4. Set up environment (add to ~/.bashrc):"
    echo "   . ~/esp/esp-idf/export.sh"
    echo
    return 1
}

# Function to setup ESP-IDF environment
setup_esp_idf_env() {
    print_info "Setting up ESP-IDF environment..."
    
    if [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/export.sh" ]; then
        source "$IDF_PATH/export.sh"
        if [ $? -eq 0 ]; then
            print_success "ESP-IDF environment configured successfully"
            return 0
        else
            print_error "Failed to set ESP-IDF environment"
            return 1
        fi
    else
        print_error "ESP-IDF export.sh not found"
        return 1
    fi
}

# Function to build project
build_project() {
    print_info "Building ESP32 project..."
    
    # Clean previous build if requested
    if [ "$1" = "clean" ]; then
        print_info "Cleaning previous build..."
        rm -rf build/
    fi
    
    # Build the project
    idf.py build
    
    return $?
}

# Function to show build results
show_build_results() {
    local build_status=$1
    
    if [ $build_status -eq 0 ] && [ -f "build/${PROJECT_NAME}.bin" ]; then
        echo
        echo -e "${CYAN}========================================${NC}"
        print_success "BUILD COMPLETED SUCCESSFULLY!"
        echo -e "${CYAN}========================================${NC}"
        echo
        
        echo -e "${BLUE}Build Information:${NC}"
        
        # Check application binary
        if [ -f "build/${PROJECT_NAME}.bin" ]; then
            local app_size=$(get_file_size "build/${PROJECT_NAME}.bin")
            echo "   Application: ${PROJECT_NAME}.bin ($(format_size $app_size))"
        else
            print_warning "Application binary not found: build/${PROJECT_NAME}.bin"
        fi
        
        # Check bootloader
        if [ -f "build/bootloader/bootloader.bin" ]; then
            local boot_size=$(get_file_size "build/bootloader/bootloader.bin")
            echo "   Bootloader: bootloader.bin ($(format_size $boot_size))"
        else
            print_warning "Bootloader binary not found: build/bootloader/bootloader.bin"
        fi
        
        # Check partition table
        if [ -f "build/partition_table/partition-table.bin" ]; then
            local part_size=$(get_file_size "build/partition_table/partition-table.bin")
            echo "   Partition Table: partition-table.bin ($(format_size $part_size))"
        else
            print_warning "Partition table not found: build/partition_table/partition-table.bin"
        fi
        
        echo "   Build completed: $(date)"
        echo
        
        echo -e "${BLUE}Output Files:${NC}"
        [ -f "build/${PROJECT_NAME}.bin" ] && echo "   [OK] build/${PROJECT_NAME}.bin (main application)"
        [ -f "build/bootloader/bootloader.bin" ] && echo "   [OK] build/bootloader/bootloader.bin (bootloader)"
        [ -f "build/partition_table/partition-table.bin" ] && echo "   [OK] build/partition_table/partition-table.bin (partition table)"
        echo
        
        echo -e "${BLUE}Next Steps:${NC}"
        echo "   1. Flash to ESP32: ./flash.sh /dev/ttyUSB0"
        echo "   2. Or use: idf.py -p /dev/ttyUSB0 flash monitor"
        echo "   3. For OTA: Use web interface after initial flash"
        echo
        
        echo -e "${BLUE}Flash Configuration:${NC}"
        echo "   - Flash Size: 16MB"
        echo "   - Partition: Custom OTA partitions (16MB optimized)"
        echo "   - Target: ESP32"
        
    else
        echo
        echo -e "${CYAN}========================================${NC}"
        print_error "BUILD FAILED!"
        echo -e "${CYAN}========================================${NC}"
        echo
        
        echo "Please check the error messages above and fix the issues."
        echo
        echo -e "${BLUE}Common Solutions:${NC}"
        echo "   1. Check ESP-IDF environment installation"
        echo "   2. Verify all source files syntax"
        echo "   3. Check CMakeLists.txt configuration"
        echo "   4. Clean build directory: rm -rf build/"
        echo "   5. Check component dependencies in main/CMakeLists.txt"
        echo "   6. Verify Flash size configuration (should be 16MB)"
        echo
        
        echo -e "${BLUE}Troubleshooting Steps:${NC}"
        echo "   - Check build logs in: build/log/"
        echo "   - Verify sdkconfig Flash settings"
        echo "   - Ensure all required components are available"
        echo "   - Run: ./build_only.sh clean (to clean and rebuild)"
    fi
}

# Main execution
main() {
    # Change to script directory
    cd "$(dirname "$0")"
    
    # Clear screen and show header
    clear
    print_header
    
    # Check ESP-IDF installation
    if ! check_esp_idf; then
        exit 1
    fi
    
    # Setup ESP-IDF environment
    if ! setup_esp_idf_env; then
        exit 1
    fi
    
    echo
    
    # Build project
    build_project "$1"
    build_status=$?
    
    # Show results
    show_build_results $build_status
    
    echo
    echo "Press Enter to continue..."
    read
    
    exit $build_status
}

# Run main function with all arguments
main "$@"
