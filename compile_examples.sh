#!/bin/bash

# AFE4490 Arduino Library - Example Compilation Script
# This script compiles all examples for multiple Arduino platforms
# Usage: ./compile_examples.sh [platform]
# Example: ./compile_examples.sh all  (compiles for all platforms)
#          ./compile_examples.sh uno  (compiles for Arduino Uno only)

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
LIBRARY_PATH="."
EXAMPLES_DIR="examples"
LIBRARY_NAME="protocentral-afe4490-arduino"

# Supported platforms and their FQBNs
declare -A PLATFORMS=(
    ["uno"]="arduino:avr:uno"
    ["nano"]="arduino:avr:nano"
    ["mega"]="arduino:avr:mega"
    ["leonardo"]="arduino:avr:leonardo"
    ["esp32"]="esp32:esp32:esp32"
    ["esp32s3"]="esp32:esp32:esp32s3"
)

# Platform descriptions for better output
declare -A PLATFORM_NAMES=(
    ["uno"]="Arduino Uno"
    ["nano"]="Arduino Nano"
    ["mega"]="Arduino Mega"
    ["leonardo"]="Arduino Leonardo"
    ["esp32"]="ESP32 Dev Module"
    ["esp32s3"]="ESP32-S3 Dev Module"
)

# Statistics tracking
TOTAL_COMPILATIONS=0
SUCCESSFUL_COMPILATIONS=0
FAILED_COMPILATIONS=0
declare -a COMPILATION_RESULTS=()
declare -a FAILED_DETAILS=()

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
        "SUCCESS")
            echo -e "${GREEN}[SUCCESS]${NC} $message"
            ;;
        "ERROR")
            echo -e "${RED}[ERROR]${NC} $message"
            ;;
        "WARNING")
            echo -e "${YELLOW}[WARNING]${NC} $message"
            ;;
    esac
}

# Function to print section headers
print_header() {
    echo
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

# Function to check if arduino-cli is installed
check_arduino_cli() {
    if ! command -v arduino-cli &> /dev/null; then
        print_status "ERROR" "arduino-cli is not installed or not in PATH"
        print_status "INFO" "Please install arduino-cli from: https://arduino.github.io/arduino-cli/"
        exit 1
    fi
    
    local version=$(arduino-cli version | head -n1)
    print_status "INFO" "Found $version"
}

# Function to check if required cores are installed
check_cores() {
    print_status "INFO" "Checking installed Arduino cores..."
    
    local cores_output=$(arduino-cli core list)
    
    if [[ $cores_output == *"arduino:avr"* ]]; then
        print_status "SUCCESS" "Arduino AVR core is installed"
    else
        print_status "WARNING" "Arduino AVR core not found. Installing..."
        arduino-cli core install arduino:avr
    fi
    
    if [[ $cores_output == *"esp32:esp32"* ]]; then
        print_status "SUCCESS" "ESP32 core is installed"
    else
        print_status "WARNING" "ESP32 core not found. You may need to install it manually for ESP32 compilation"
        print_status "INFO" "To install ESP32 core: arduino-cli core install esp32:esp32"
    fi
}

# Function to find all example sketches
find_examples() {
    local examples=()
    while IFS= read -r -d '' file; do
        examples+=("$file")
    done < <(find "$EXAMPLES_DIR" -name "*.ino" -print0 2>/dev/null)
    
    if [ ${#examples[@]} -eq 0 ]; then
        print_status "ERROR" "No example sketches found in $EXAMPLES_DIR"
        exit 1
    fi
    
    print_status "INFO" "Found ${#examples[@]} example(s):"
    for example in "${examples[@]}"; do
        local example_name=$(basename "$example" .ino)
        print_status "INFO" "  - $example_name"
    done
    
    echo "${examples[@]}"
}

# Function to get memory usage from compilation output
extract_memory_usage() {
    local output="$1"
    local flash_usage=""
    local ram_usage=""
    
    # Extract flash usage
    if [[ $output =~ Sketch\ uses\ ([0-9]+)\ bytes\ \(([0-9]+)%\)\ of\ program\ storage ]]; then
        flash_usage="${BASH_REMATCH[1]} bytes (${BASH_REMATCH[2]}%)"
    fi
    
    # Extract RAM usage
    if [[ $output =~ Global\ variables\ use\ ([0-9]+)\ bytes\ \(([0-9]+)%\)\ of\ dynamic\ memory ]]; then
        ram_usage="${BASH_REMATCH[1]} bytes (${BASH_REMATCH[2]}%)"
    fi
    
    if [[ -n "$flash_usage" && -n "$ram_usage" ]]; then
        echo "Flash: $flash_usage, RAM: $ram_usage"
    elif [[ -n "$flash_usage" ]]; then
        echo "Flash: $flash_usage"
    else
        echo "Memory usage not available"
    fi
}

# Function to compile a single example for a single platform
compile_example() {
    local example_path="$1"
    local platform_key="$2"
    local platform_fqbn="${PLATFORMS[$platform_key]}"
    local platform_name="${PLATFORM_NAMES[$platform_key]}"
    
    local example_name=$(basename "$(dirname "$example_path")")
    
    print_status "INFO" "Compiling $example_name for $platform_name..."
    
    TOTAL_COMPILATIONS=$((TOTAL_COMPILATIONS + 1))
    
    # Compile the example
    local compile_output
    local compile_result=0
    
    compile_output=$(arduino-cli compile --fqbn "$platform_fqbn" "$example_path" --libraries "$LIBRARY_PATH" 2>&1) || compile_result=$?
    
    if [ $compile_result -eq 0 ]; then
        SUCCESSFUL_COMPILATIONS=$((SUCCESSFUL_COMPILATIONS + 1))
        local memory_usage=$(extract_memory_usage "$compile_output")
        print_status "SUCCESS" "$example_name ✓ [$platform_name] - $memory_usage"
        COMPILATION_RESULTS+=("✓ $example_name [$platform_name] - $memory_usage")
    else
        FAILED_COMPILATIONS=$((FAILED_COMPILATIONS + 1))
        print_status "ERROR" "$example_name ✗ [$platform_name] - Compilation failed"
        COMPILATION_RESULTS+=("✗ $example_name [$platform_name] - FAILED")
        FAILED_DETAILS+=("=== $example_name [$platform_name] ===" "$compile_output" "")
    fi
    
    return $compile_result
}

# Function to compile all examples for specified platforms
compile_examples() {
    local target_platforms=("$@")
    
    if [ ${#target_platforms[@]} -eq 0 ]; then
        print_status "ERROR" "No platforms specified"
        return 1
    fi
    
    # Find all examples
    local examples_array=($(find_examples))
    
    print_header "COMPILING EXAMPLES"
    
    # Compile each example for each platform
    local any_failed=0
    for example in "${examples_array[@]}"; do
        for platform in "${target_platforms[@]}"; do
            if [[ -z "${PLATFORMS[$platform]}" ]]; then
                print_status "WARNING" "Unknown platform: $platform (skipping)"
                continue
            fi
            
            compile_example "$example" "$platform" || any_failed=1
        done
        echo  # Add spacing between examples
    done
    
    return $any_failed
}

# Function to print compilation summary
print_summary() {
    print_header "COMPILATION SUMMARY"
    
    print_status "INFO" "Total compilations: $TOTAL_COMPILATIONS"
    print_status "SUCCESS" "Successful: $SUCCESSFUL_COMPILATIONS"
    
    if [ $FAILED_COMPILATIONS -gt 0 ]; then
        print_status "ERROR" "Failed: $FAILED_COMPILATIONS"
    else
        print_status "SUCCESS" "Failed: $FAILED_COMPILATIONS"
    fi
    
    echo
    print_status "INFO" "Detailed results:"
    for result in "${COMPILATION_RESULTS[@]}"; do
        if [[ $result == ✓* ]]; then
            echo -e "  ${GREEN}$result${NC}"
        else
            echo -e "  ${RED}$result${NC}"
        fi
    done
    
    # Print failed compilation details if any
    if [ ${#FAILED_DETAILS[@]} -gt 0 ]; then
        echo
        print_header "FAILURE DETAILS"
        for detail in "${FAILED_DETAILS[@]}"; do
            echo "$detail"
        done
    fi
    
    echo
    if [ $FAILED_COMPILATIONS -eq 0 ]; then
        print_status "SUCCESS" "All examples compiled successfully! 🎉"
        return 0
    else
        print_status "ERROR" "Some compilations failed. Please check the details above."
        return 1
    fi
}

# Function to show usage
show_usage() {
    echo "AFE4490 Arduino Library - Example Compilation Script"
    echo
    echo "Usage: $0 [platform1] [platform2] ..."
    echo "       $0 all"
    echo "       $0 avr"
    echo "       $0 esp"
    echo
    echo "Available platforms:"
    for platform in "${!PLATFORM_NAMES[@]}"; do
        printf "  %-10s - %s\n" "$platform" "${PLATFORM_NAMES[$platform]}"
    done
    echo
    echo "Platform groups:"
    echo "  all        - All supported platforms"
    echo "  avr        - All Arduino AVR platforms (uno, nano, mega, leonardo)"
    echo "  esp        - All ESP platforms (esp32, esp32s3)"
    echo
    echo "Examples:"
    echo "  $0 uno                    # Compile for Arduino Uno only"
    echo "  $0 uno esp32              # Compile for Arduino Uno and ESP32"
    echo "  $0 avr                    # Compile for all AVR platforms"
    echo "  $0 all                    # Compile for all platforms"
}

# Main function
main() {
    print_header "AFE4490 ARDUINO LIBRARY - EXAMPLE COMPILATION"
    
    # Check if help is requested
    if [[ "$1" == "-h" || "$1" == "--help" || "$1" == "help" ]]; then
        show_usage
        exit 0
    fi
    
    # Check prerequisites
    check_arduino_cli
    check_cores
    
    # Determine target platforms
    local target_platforms=()
    
    if [ $# -eq 0 ]; then
        print_status "INFO" "No platforms specified, using default: uno"
        target_platforms=("uno")
    else
        for arg in "$@"; do
            case "$arg" in
                "all")
                    target_platforms=(${!PLATFORMS[@]})
                    break
                    ;;
                "avr")
                    target_platforms+=("uno" "nano" "mega" "leonardo")
                    ;;
                "esp")
                    target_platforms+=("esp32" "esp32s3")
                    ;;
                *)
                    if [[ -n "${PLATFORMS[$arg]}" ]]; then
                        target_platforms+=("$arg")
                    else
                        print_status "ERROR" "Unknown platform or group: $arg"
                        show_usage
                        exit 1
                    fi
                    ;;
            esac
        done
    fi
    
    # Remove duplicates
    target_platforms=($(printf "%s\n" "${target_platforms[@]}" | sort -u))
    
    print_status "INFO" "Target platforms: ${target_platforms[*]}"
    
    # Change to library directory
    if [ ! -d "$EXAMPLES_DIR" ]; then
        print_status "ERROR" "Examples directory not found: $EXAMPLES_DIR"
        print_status "INFO" "Please run this script from the library root directory"
        exit 1
    fi
    
    # Compile examples
    local compilation_result=0
    compile_examples "${target_platforms[@]}" || compilation_result=$?
    
    # Print summary
    print_summary
    
    exit $compilation_result
}

# Run main function with all arguments
main "$@"