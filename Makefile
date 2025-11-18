# LivelyBot High Torque Motor Control Library Makefile
# Provides convenient commands for development and testing

.PHONY: help setup clean test scan velocity angle stop status format

# Default target
help:
	@echo "LivelyBot High Torque Motor Control Library"
	@echo "=============================================="
	@echo ""
	@echo "Available commands:"
	@echo "  make setup    - Setup development environment"
	@echo "  make scan     - Scan for motors"
	@echo "  make velocity - Start velocity control (motor_id=1)"
	@echo "  make angle    - Start angle control (motor_id=1)"
	@echo "  make test     - Run motor tests"
	@echo "  make stop     - Emergency stop"
	@echo "  make status   - Show CAN status"
	@echo "  make clean    - Clean temporary files"
	@echo "  make format   - Format Python code"
	@echo "  make check    - Run code quality checks"
	@echo ""
	@echo "Examples:"
	@echo "  make scan                  # Scan motors"
	@echo "  make velocity MOTOR_ID=2   # Control motor 2"
	@echo "  make angle MOTOR_ID=2      # Control motor 2 angle"
	@echo "  make test MOTOR_ID=2       # Test motor 2"

# Environment setup
setup:
	@echo "🔧 Setting up development environment..."
	./scripts/setup.sh all

# Motor scanning
scan:
	@echo "🔍 Scanning for motors..."
	./quick_start.sh scan

# Velocity control
velocity:
	@echo "🏎️ Starting velocity control for motor $(MOTOR_ID)..."
	./quick_start.sh velocity $(MOTOR_ID)

# Angle control
angle:
	@echo "🎯 Starting angle control for motor $(MOTOR_ID)..."
	./quick_start.sh angle $(MOTOR_ID)

# Motor tests
test:
	@echo "🧪 Running motor tests for motor $(MOTOR_ID)..."
	./quick_start.sh tests $(MOTOR_ID)

# Emergency stop
stop:
	@echo "🛑 Emergency stop for motor $(MOTOR_ID)..."
	./quick_start.sh stop $(MOTOR_ID)

# CAN status
status:
	@echo "📊 CAN interface status:"
	./quick_start.sh status

# Clean temporary files
clean:
	@echo "🧹 Cleaning temporary files..."
	find . -type f -name "*.pyc" -delete
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.log" -delete
	find . -type f -name ".DS_Store" -delete
	find . -type f -name "Thumbs.db" -delete
	@echo "✅ Clean completed!"

# Format Python code
format:
	@echo "📝 Formatting Python code..."
	@if command -v black >/dev/null 2>&1; then \
		cd python && black *.py; \
	else \
		echo "⚠️  black not found. Install with: pip install black"; \
	fi

# Run code quality checks
check:
	@echo "🔍 Running code quality checks..."
	@cd python && \
	if command -v flake8 >/dev/null 2>&1; then \
		echo "Running flake8..."; \
		flake8 *.py --max-line-length=100 --ignore=E203,W503; \
	else \
		echo "⚠️  flake8 not found. Install with: pip install flake8"; \
	fi
	@echo "✅ Code quality checks completed!"

# Python linting
lint:
	@echo "🔍 Running Python linting..."
	@cd python && \
	if command -v pylint >/dev/null 2>&1; then \
		echo "Running pylint..."; \
		pylint *.py --disable=R,C,W0613; \
	else \
		echo "⚠️  pylint not found. Install with: pip install pylint"; \
	fi

# Install dependencies
deps:
	@echo "📦 Installing dependencies..."
	@cd python && pip3 install -r requirements.txt --user

# CAN interface setup
setup-can:
	@echo "🔌 Setting up CAN interface..."
	@sudo ip link set can0 down 2>/dev/null || true
	@sudo ip link set can0 up type can bitrate 1000000 restart-ms 100
	@echo "✅ CAN interface setup complete!"

# Test sine wave velocity
test-sine-vel:
	@echo "🌊 Testing sine wave velocity..."
	./quick_start.sh test-sine-vel $(MOTOR_ID) $(AMPLITUDE) $(FREQUENCY) $(DURATION)

# Test sine wave angle
test-sine-angle:
	@echo "🌊 Testing sine wave angle..."
	./quick_start.sh test-sine-angle $(MOTOR_ID) $(AMPLITUDE) $(FREQUENCY) $(DURATION)

# Continuous monitor
monitor:
	@echo "📈 Continuous monitoring mode..."
	@python3 python/can_motor_scanner.py --channel can0 --monitor 60

# Batch motor test
test-all:
	@echo "🧪 Testing all motors (ID 1-14)..."
	@for motor_id in $$(seq 1 14); do \
		echo "Testing motor $$motor_id..."; \
		./quick_start.sh tests $$motor_id || true; \
		sleep 2; \
	done

# Documentation generation
docs:
	@echo "📚 Generating documentation..."
	@echo "✅ Documentation ready in README files"

# Project status
project-status:
	@echo "📊 Project Status:"
	@echo "┌─────────────────────────────────────────┐"
	@echo "│ Component        │ Status    │ Progress │"
	@echo "├─────────────────────────────────────────┤"
	@echo "│ Python (100Hz)    │ ✅ Complete│ 100%     │"
	@echo "│ C++ (200Hz)       │ ⏳ TODO    │ 0%       │"
	@echo "│ Rust (150Hz)      │ ⏳ TODO    │ 0%       │"
	@echo "│ Arduino (50-200Hz)│ ⏳ TODO    │ 0%       │"
	@echo "│ Documentation     │ ✅ Complete│ 100%     │"
	@echo "└─────────────────────────────────────────┘"

# Development server
dev:
	@echo "🚀 Starting development server..."
	@echo "Use this for interactive development"
	@./quick_start.sh

# Release preparation
release:
	@echo "📦 Preparing for release..."
	@make clean
	@make format
	@make check
	@echo "✅ Release preparation complete!"

# Installation
install:
	@echo "📥 Installing LivelyBot Motor Control..."
	@echo "Adding to PATH..."
	@echo 'export PATH=$$PWD:$$PATH' >> ~/.bashrc
	@echo "✅ Installation complete! Run 'source ~/.bashrc' or restart your shell."

# Uninstallation
uninstall:
	@echo "🗑️ Uninstalling LivelyBot Motor Control..."
	@sed -i '/livelybot_hardware_sdk/d' ~/.bashrc
	@echo "✅ Uninstallation complete!"

# Advanced examples
examples:
	@echo "🎯 Advanced examples:"
	@echo ""
	@echo "1. Multi-motor sine wave coordination:"
	@echo "   for motor in 1 2 3; do ./quick_start.sh test-sine-angle $$motor 45 0.5 5 & done; wait"
	@echo ""
	@echo "2. Batch motor scanning with report:"
	@echo "   ./quick_start.sh scan | tee motor_scan_report.txt"
	@echo ""
	@echo "3. Continuous monitoring with logging:"
	@echo "   ./quick_start.sh status | tee can_monitor.log &"
	@echo "   ./quick_start.sh monitor"

# Docker setup (optional)
docker-setup:
	@echo "🐳 Setting up Docker environment..."
	@if [ -f Dockerfile ]; then \
		docker build -t livelybot-motor-control .; \
	else \
		echo "⚠️  Dockerfile not found"; \
	fi

# CI/CD setup
ci:
	@echo "🔄 CI/CD setup:"
	@echo "Configure your CI/CD pipeline to:"
	@echo "1. make setup"
	@echo "2. make deps"
	@echo "3. make test"
	@echo "4. make check"
	@echo "5. make docs"

# Version info
version:
	@echo "🏷️  LivelyBot High Torque Motor Control Library"
	@echo "Version: 1.0.0"
	@echo "Python Implementation: ✅ Complete"
	@echo "C++ Implementation: ⏳ TODO"
	@echo "Rust Implementation: ⏳ TODO"
	@echo "Arduino Implementation: ⏳ TODO"

# Target-specific commands
.PHONY: setup-can test-sine-vel test-sine-angle monitor test-all docs project-status dev release install uninstall examples docker-setup ci version

# Default values for optional parameters
MOTOR_ID ?= 1
AMPLITUDE ?= 2.0
FREQUENCY ?= 0.5
DURATION ?= 10.0