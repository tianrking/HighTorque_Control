#!/bin/bash
# LivelyBot High Torque Motor Control Library - Quick Start Script
# This script provides quick access to common tasks

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

# Function to check if CAN interface is available
check_can_interface() {
    if ! ip link show can0 &> /dev/null; then
        print_warning "CAN interface can0 not found"
        print_info "Please run: sudo ip link set can0 up type can bitrate 1000000"
        return 1
    fi

    if ! ip link show can0 | grep -q "UP"; then
        print_warning "CAN interface can0 is not up"
        print_info "Please run: sudo ip link set can0 up type can bitrate 1000000"
        return 1
    fi

    return 0
}

# Function to scan motors
scan_motors() {
    print_info "Scanning for motors on CAN bus..."
    if ! check_can_interface; then
        return 1
    fi

    python3 python/can_motor_scanner.py --channel can0
}

# Function to start interactive velocity control
velocity_control() {
    local motor_id=${1:-1}
    print_info "Starting velocity control for motor $motor_id..."

    if ! check_can_interface; then
        return 1
    fi

    python3 python/velocity_acceleration_control.py --motor_id $motor_id --mode interactive
}

# Function to start interactive angle control
angle_control() {
    local motor_id=${1:-1}
    print_info "Starting angle control for motor $motor_id..."

    if ! check_can_interface; then
        return 1
    fi

    python3 python/angle_stream_control.py --motor_id $motor_id --mode interactive
}

# Function to test sine wave velocity
test_sine_velocity() {
    local motor_id=${1:-1}
    local amplitude=${2:-2.0}
    local frequency=${3:-0.5}
    local duration=${4:-10.0}

    print_info "Testing sine wave velocity control..."
    print_info "Motor: $motor_id, Amplitude: ${amplitude} rad/s, Frequency: ${frequency} Hz, Duration: ${duration}s"

    if ! check_can_interface; then
        return 1
    fi

    python3 python/velocity_acceleration_control.py --motor_id $motor_id \
        --mode sine --amplitude $amplitude --frequency $frequency --duration $duration
}

# Function to test sine wave angle
test_sine_angle() {
    local motor_id=${1:-1}
    local amplitude=${2:-90.0}
    local frequency=${3:-0.2}
    local duration=${4:-10.0}

    print_info "Testing sine wave angle control..."
    print_info "Motor: $motor_id, Amplitude: ${amplitude}°, Frequency: ${frequency} Hz, Duration: ${duration}s"

    if ! check_can_interface; then
        return 1
    fi

    python3 python/angle_stream_control.py --motor_id $motor_id \
        --mode sine --amplitude $amplitude --frequency $frequency --duration $duration
}

# Function to show CAN status
show_can_status() {
    print_info "CAN Interface Status:"

    echo "Interface Details:"
    ip -details link show can0 2>/dev/null || print_error "CAN interface not found"

    echo ""
    echo "SocketCAN Statistics:"
    cat /proc/net/can 2>/dev/null || print_error "CAN statistics not available"

    echo ""
    echo "Recent CAN Traffic (10 seconds):"
    timeout 10s candump can0 -ta 2>/dev/null || print_warning "No CAN traffic detected"
}

# Function to run motor tests
run_motor_tests() {
    local motor_id=${1:-1}
    print_info "Running comprehensive motor tests for motor $motor_id..."

    if ! check_can_interface; then
        return 1
    fi

    # Test 1: Motor scanning
    print_info "Test 1: Scanning motors..."
    python3 python/can_motor_scanner.py --channel can0 --test $motor_id

    # Test 2: Velocity response
    print_info "Test 2: Velocity response test..."
    python3 python/velocity_acceleration_control.py --motor_id $motor_id \
        --mode step --velocities "0,1,0,-1,0" --step_duration 2.0

    # Test 3: Angle response
    print_info "Test 3: Angle response test..."
    python3 python/angle_stream_control.py --motor_id $motor_id \
        --mode test --test_angles "0,45,90,45,0"

    print_success "Motor tests completed!"
}

# Function to emergency stop
emergency_stop() {
    local motor_id=${1:-1}
    print_warning "EMERGENCY STOP - Stopping all motors!"

    if ! check_can_interface; then
        return 1
    fi

    # Send stop command to all motors
    python3 -c "
import can
import time
import struct

try:
    bus = can.interface.Bus('can0', interface='socketcan')

    # Send stop command to motors 1-14
    for motor_id in range(1, 15):
        arbitration_id = 0x0000 | motor_id
        data = [0x01, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50]
        msg = can.Message(arbitration_id=arbitration_id, data=data, is_extended_id=True, is_fd=False)
        bus.send(msg)
        time.sleep(0.01)

    bus.shutdown()
    print('Emergency stop completed!')
except Exception as e:
    print(f'Error during emergency stop: {e}')
"

    print_success "Emergency stop completed!"
}

# Function to show help
show_help() {
    echo "LivelyBot High Torque Motor Control - Quick Start Script"
    echo ""
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  scan                            Scan for motors on CAN bus"
    echo "  velocity [motor_id]             Start interactive velocity control"
    echo "  angle [motor_id]                Start interactive angle control"
    echo "  test-sine-vel [motor_id] [amp] [freq] [dur]"
    echo "                                   Test sine wave velocity control"
    echo "  test-sine-angle [motor_id] [amp] [freq] [dur]"
    echo "                                   Test sine wave angle control"
    echo "  status                          Show CAN interface status"
    echo "  tests [motor_id]                 Run comprehensive motor tests"
    echo "  stop [motor_id]                  Emergency stop"
    echo "  help                            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 scan                          # Scan for motors"
    echo "  $0 velocity 1                    # Control motor 1 velocity"
    echo "  $0 angle 1                       # Control motor 1 angle"
    echo "  $0 test-sine-vel 1 2.0 0.5 10   # Test sine velocity on motor 1"
    echo "  $0 tests 1                       # Run tests on motor 1"
    echo "  $0 stop                          # Emergency stop all motors"
    echo ""
    echo "Default motor_id is 1"
    echo "Default sine velocity test: 2.0 rad/s, 0.5 Hz, 10s"
    echo "Default sine angle test: 90.0°, 0.2 Hz, 10s"
}

# Main function
main() {
    case "${1:-help}" in
        "scan")
            scan_motors
            ;;
        "velocity")
            velocity_control "$2"
            ;;
        "angle")
            angle_control "$2"
            ;;
        "test-sine-vel")
            test_sine_velocity "$2" "$3" "$4" "$5"
            ;;
        "test-sine-angle")
            test_sine_angle "$2" "$3" "$4" "$5"
            ;;
        "status")
            show_can_status
            ;;
        "tests")
            run_motor_tests "$2"
            ;;
        "stop")
            emergency_stop "$2"
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            print_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"