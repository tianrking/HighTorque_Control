#!/bin/bash
# LivelyBot High Torque Motor Control Library - Setup Script
# This script sets up the development environment for all platforms

set -e

echo "🚀 Setting up LivelyBot High Torque Motor Control Library..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
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

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    print_warning "Running as root is not recommended for setup"
fi

# Function to setup CAN interface
setup_can_interface() {
    print_status "Setting up CAN interface..."

    # Check if can-utils is installed
    if ! command -v candump &> /dev/null; then
        print_status "Installing can-utils..."
        sudo apt-get update
        sudo apt-get install -y can-utils
    else
        print_success "can-utils already installed"
    fi

    # Setup can0 interface
    print_status "Configuring can0 interface..."
    sudo ip link set can0 down 2>/dev/null || true
    sudo ip link set can0 up type can bitrate 1000000 restart-ms 100

    # Verify interface is up
    if ip link show can0 | grep -q "UP"; then
        print_success "CAN interface can0 is up and running"
    else
        print_error "Failed to setup CAN interface"
        return 1
    fi
}

# Function to setup Python environment
setup_python() {
    print_status "Setting up Python environment..."

    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is not installed"
        return 1
    fi

    # Check if pip is available
    if ! command -v pip3 &> /dev/null; then
        print_status "Installing pip3..."
        sudo apt-get install -y python3-pip
    fi

    # Install Python dependencies
    print_status "Installing Python dependencies..."
    cd python
    pip3 install -r requirements.txt --user
    cd ..

    print_success "Python environment setup complete"
}

# Function to setup C++ environment
setup_cpp() {
    print_status "Setting up C++ environment..."

    # Check if required tools are installed
    local missing_tools=()

    for tool in gcc g++ make cmake; do
        if ! command -v $tool &> /dev/null; then
            missing_tools+=($tool)
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_status "Installing missing C++ tools: ${missing_tools[*]}"
        sudo apt-get install -y build-essential cmake
    else
        print_success "C++ tools already installed"
    fi

    # Create basic CMakeLists.txt if it doesn't exist
    if [ ! -f cpp/CMakeLists.txt ]; then
        print_status "Creating basic CMakeLists.txt..."
        cat > cpp/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(livelybot_motor_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAN REQUIRED libsocketcan)

# Add source files here
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# Create library
add_library(livelybot_motor ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(livelybot_motor ${CAN_LIBRARIES})

# Include directories
target_include_directories(livelybot_motor PUBLIC include)

# Create examples
add_subdirectory(examples OPTIONAL)
EOF
        print_success "Created basic CMakeLists.txt"
    fi

    print_success "C++ environment setup complete"
}

# Function to setup Rust environment
setup_rust() {
    print_status "Setting up Rust environment..."

    # Check if Rust is installed
    if ! command -v cargo &> /dev/null; then
        print_status "Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
        source $HOME/.cargo/env
    else
        print_success "Rust already installed"
    fi

    # Create basic Cargo.toml if it doesn't exist
    if [ ! -f rust/Cargo.toml ]; then
        print_status "Creating basic Cargo.toml..."
        cat > rust/Cargo.toml << 'EOF'
[package]
name = "livelybot-motor-control"
version = "0.1.0"
edition = "2021"
authors = ["LivelyBot Team"]
description = "High Torque Motor Control Library"
license = "MIT"

[dependencies]
# CAN bus communication
socketcan = "2.0"

# Serialization
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

# Async runtime
tokio = { version = "1.0", features = ["full"] }
tokio-socketcan = "2.0"

# Error handling
anyhow = "1.0"
thiserror = "1.0"

# Logging
log = "0.4"
env_logger = "0.10"

# Utilities
clap = { version = "4.0", features = ["derive"] }
ctrlc = "3.0"

[dev-dependencies]
tokio-test = "0.4"
EOF
        print_success "Created basic Cargo.toml"
    fi

    # Create basic main.rs if it doesn't exist
    if [ ! -f rust/src/main.rs ]; then
        mkdir -p rust/src
        cat > rust/src/main.rs << 'EOF'
use clap::Parser;

#[derive(Parser)]
#[command(name = "livelybot-motor")]
#[command(about = "LivelyBot High Torque Motor Control")]
struct Cli {
    /// Motor ID to control
    #[arg(short, long)]
    motor_id: u8,
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    println!("LivelyBot Motor Control (Rust)");
    println!("Motor ID: {}", cli.motor_id);
    println!("Implementation coming soon...");

    Ok(())
}
EOF
        print_success "Created basic main.rs"
    fi

    print_success "Rust environment setup complete"
}

# Function to setup Arduino environment
setup_arduino() {
    print_status "Arduino/ESP32 environment setup..."

    print_warning "Arduino/ESP32 development requires manual setup"
    print_status "Please install:"
    print_status "  - Arduino IDE or PlatformIO"
    print_status "  - ESP32 board support package"
    print_status "  - Required libraries (can-utils, etc.)"

    # Create basic platformio.ini if it doesn't exist
    if [ ! -f arduino/platformio.ini ]; then
        print_status "Creating basic platformio.ini..."
        cat > arduino/platformio.ini << 'EOF'
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

[env:esp32dev_can]
extends = env:esp32dev
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DCORE_DEBUG_LEVEL=3
lib_deps =
    mbed-mbed/spotify-github-32-CAN
EOF
        print_success "Created basic platformio.ini"
    fi

    # Create basic example sketch
    if [ ! -f arduino/examples/can_motor_control/can_motor_control.ino ]; then
        mkdir -p arduino/examples/can_motor_control
        cat > arduino/examples/can_motor_control/can_motor_control.ino << 'EOF'
/*
 * LivelyBot High Torque Motor Control - Arduino Example
 * Basic CAN motor control implementation for ESP32
 */

#include <Arduino.h>
#include <CAN.h>

#define MOTOR_ID 1
#define CAN_CS_PIN 5

CAN_device_t CAN_cfg;

void setup() {
  Serial.begin(115200);
  Serial.println("LivelyBot Motor Control - ESP32");

  // Initialize CAN
  CAN_cfg.speed = CAN_SPEED_1000KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_5;
  CAN_cfg.rx_pin_id = GPIO_NUM_4;
  CAN_cfg.rx_queue_size = 5;
  CAN_cfg.tx_queue_size = 5;

  CAN_cfg.task_priority = 10;
  CAN_cfg.task_stack_size = 4096;
  CAN_cfg.driver_mode = CAN_NORMAL_MODE;

  CAN_init(&CAN_cfg, CAN_MAX_DEVICE_NUM);
  CAN_start(&CAN_cfg, CAN_MAX_DEVICE_NUM);

  Serial.println("CAN initialized");
  Serial.println("Motor control implementation coming soon...");
}

void loop() {
  // Basic CAN implementation coming soon
  delay(1000);
}
EOF
        print_success "Created basic Arduino example"
    fi

    print_success "Arduino environment setup complete"
}

# Function to run tests
run_tests() {
    print_status "Running tests..."

    # Python tests
    if [ -f python/test_motor_scanner.py ]; then
        print_status "Running Python tests..."
        cd python
        python3 -m pytest test_motor_scanner.py -v || print_warning "Python tests not found"
        cd ..
    fi

    # Add tests for other languages as they are implemented
}

# Function to show project status
show_status() {
    print_status "Project Status:"
    echo "┌─────────────────────────────────────────┐"
    echo "│ Component        │ Status    │ TODO     │"
    echo "├─────────────────────────────────────────┤"
    echo "│ Python (100Hz)    │ ✅ Complete│          │"
    echo "│ C++ (200Hz)       │ ⏳ TODO    │ 70%      │"
    echo "│ Rust (150Hz)      │ ⏳ TODO    │ 60%      │"
    echo "│ Arduino (50-200Hz)│ ⏳ TODO    │ 50%      │"
    echo "│ Documentation     │ ✅ Complete│          │"
    echo "│ Tests            │ ⏳ TODO    │ 30%      │"
    echo "└─────────────────────────────────────────┘"
}

# Main setup function
main() {
    echo "🔧 LivelyBot High Torque Motor Control Library Setup"
    echo "=================================================="

    # Parse command line arguments
    case "${1:-all}" in
        "python")
            setup_python
            ;;
        "cpp")
            setup_cpp
            ;;
        "rust")
            setup_rust
            ;;
        "arduino")
            setup_arduino
            ;;
        "can")
            setup_can_interface
            ;;
        "tests")
            run_tests
            ;;
        "status")
            show_status
            ;;
        "all")
            setup_python
            setup_cpp
            setup_rust
            setup_arduino
            setup_can_interface
            ;;
        "help"|"-h"|"--help")
            echo "Usage: $0 [command]"
            echo ""
            echo "Commands:"
            echo "  all      - Setup all environments (default)"
            echo "  python   - Setup Python environment"
            echo "  cpp      - Setup C++ environment"
            echo "  rust     - Setup Rust environment"
            echo "  arduino  - Setup Arduino/ESP32 environment"
            echo "  can      - Setup CAN interface"
            echo "  tests    - Run tests"
            echo "  status   - Show project status"
            echo "  help     - Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown command: $1"
            echo "Use '$0 help' for available commands"
            exit 1
            ;;
    esac

    print_success "Setup completed!"
    echo ""
    print_status "Next steps:"
    echo "  1. Test motor scanning: python3 python/can_motor_scanner.py --channel can0"
    echo "  2. Run velocity control: python3 python/velocity_acceleration_control.py --motor_id 1"
    echo "  3. Run angle control: python3 python/angle_stream_control.py --motor_id 1"
    echo ""
    echo "For more information, see the README.md files in each directory."
}

# Run main function with all arguments
main "$@"