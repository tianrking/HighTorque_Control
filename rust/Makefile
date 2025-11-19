# LivelyBot Motor Control - Rust Makefile
# 高性能 Rust 电机控制编译脚本

# Rust 环境变量
export CARGO_TARGET_DIR ?= target
export RUSTFLAGS ?= -C target-cpu=native

# 默认目标
.PHONY: all clean help install test release debug

all: release

# 发布模式编译 (优化)
release:
	@echo "🚀 编译 Rust 发布版本..."
	cargo build --release
	@echo "✅ 发布版本编译完成"
	@echo "可执行文件位置:"
	@echo "  - ./target/release/can_motor_scanner"
	@echo "  - ./target/release/velocity_acceleration_control"
	@echo "  - ./target/release/angle_stream_control"

# 开发模式编译 (快速)
debug:
	@echo "🔧 编译 Rust 开发版本..."
	cargo build
	@echo "✅ 开发版本编译完成"

# 检查代码
check:
	@echo "🔍 检查代码..."
	cargo check
	@echo "✅ 代码检查完成"

# 代码检查 (Clippy)
clippy:
	@echo "🔍 运行 Clippy..."
	cargo clippy -- -D warnings
	@echo "✅ Clippy 检查完成"

# 格式化代码
fmt:
	@echo "🎨 格式化代码..."
	cargo fmt
	@echo "✅ 代码格式化完成"

# 运行测试
test:
	@echo "🧪 运行测试..."
	cargo test
	@echo "✅ 测试完成"

# 清理编译文件
clean:
	@echo "🧹 清理编译文件..."
	cargo clean
	@echo "✅ 清理完成"

# 安装到系统目录
install: release
	@echo "📦 安装到系统目录..."
	sudo cp target/release/can_motor_scanner /usr/local/bin/
	sudo cp target/release/velocity_acceleration_control /usr/local/bin/
	sudo cp target/release/angle_stream_control /usr/local/bin/
	@echo "✅ 安装完成！现在可以在任何目录运行这些程序"

# 卸载
uninstall:
	@echo "🗑️  从系统目录卸载..."
	sudo rm -f /usr/local/bin/can_motor_scanner
	sudo rm -f /usr/local/bin/velocity_acceleration_control
	sudo rm -f /usr/local/bin/angle_stream_control
	@echo "✅ 卸载完成"

# 检查 Rust 环境
check-deps:
	@echo "🔧 检查 Rust 环境..."
	@which cargo > /dev/null || (echo "❌ 需要安装 Rust: https://rustup.rs/" && exit 1)
	@cargo --version
	@rustc --version
	@echo "✅ Rust 环境检查完成"

# 快速测试编译
quick-test: release
	@echo "🧪 快速测试编译结果..."
	@echo "测试 can_motor_scanner --help..."
	./target/release/can_motor_scanner --help || echo "扫描器帮助测试"
	@echo "测试 velocity_acceleration_control --help..."
	./target/release/velocity_acceleration_control --help || echo "速度控制帮助测试"
	@echo "测试 angle_stream_control --help..."
	./target/release/angle_stream_control --help || echo "角度控制帮助测试"
	@echo "✅ 所有程序编译正常"

# 显示帮助
help:
	@echo "LivelyBot 电机控制 Rust 编译脚本"
	@echo "==================================="
	@echo ""
	@echo "可用命令:"
	@echo "  make           - 编译发布版本 (默认)"
	@echo "  make debug     - 编译开发版本"
	@echo "  make release   - 编译发布版本 (优化)"
	@echo "  make check     - 检查代码"
	@echo "  make clippy    - 运行 Clippy"
	@echo "  make fmt       - 格式化代码"
	@echo "  make test      - 运行测试"
	@echo "  make clean     - 清理编译文件"
	@echo "  make install   - 安装到系统目录"
	@echo "  make uninstall - 从系统目录卸载"
	@echo "  make check-deps - 检查 Rust 环境"
	@echo "  make quick-test - 快速测试编译结果"
	@echo "  make help      - 显示此帮助信息"
	@echo ""
	@echo "使用示例:"
	@echo "  make && make install              # 编译并安装"
	@echo "  make debug                         # 开发模式编译"
	@echo "  make clean && make release         # 清理后重新编译"
	@echo ""
	@echo "依赖检查:"
	@echo "  需要: Rust 1.70+, socketcan-dev"
	@echo ""
	@echo "程序位置:"
	@echo "  ./target/release/can_motor_scanner"
	@echo "  ./target/release/velocity_acceleration_control"
	@echo "  ./target/release/angle_stream_control"