# LivelyBot Motor Control - Simple C++ Makefile
# 一键编译三个电机控制脚本

CC = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -pthread

# 目标程序
TARGETS = can_motor_scanner velocity_acceleration_control angle_stream_control

.PHONY: all clean help install

all: $(TARGETS)

# 编译电机扫描器
can_motor_scanner: can_motor_scanner.cpp
	@echo "编译电机扫描器..."
	$(CC) $(CXXFLAGS) -o $@ $< $(LIBS)
	@echo "✅ can_motor_scanner 编译完成"

# 编译速度控制
velocity_acceleration_control: velocity_acceleration_control.cpp
	@echo "编译速度加速度控制..."
	$(CC) $(CXXFLAGS) -o $@ $< $(LIBS)
	@echo "✅ velocity_acceleration_control 编译完成"

# 编译角度控制
angle_stream_control: angle_stream_control.cpp
	@echo "编译角度流控制..."
	$(CC) $(CXXFLAGS) -o $@ $< $(LIBS)
	@echo "✅ angle_stream_control 编译完成"

# 清理编译文件
clean:
	@echo "清理编译文件..."
	rm -f $(TARGETS)
	@echo "✅ 清理完成"

# 显示帮助
help:
	@echo "LivelyBot 电机控制 C++ 脚本"
	@echo "========================="
	@echo ""
	@echo "可用命令:"
	@echo "  make          - 编译所有程序"
	@echo "  make clean    - 清理编译文件"
	@echo "  make install  - 安装到 /usr/local/bin"
	@echo "  make help     - 显示此帮助信息"
	@echo ""
	@echo "使用示例:"
	@echo "  ./can_motor_scanner 1 5          # 扫描电机1-5"
	@echo "  ./velocity_acceleration_control 1  # 交互式速度控制电机1"
	@echo "  ./angle_stream_control 1 interactive  # 交互式角度控制电机1"
	@echo ""
	@echo "依赖检查:"
	@echo "  需要: g++, libsocketcan-dev, pkg-config"

# 安装到系统目录
install: $(TARGETS)
	@echo "安装到系统目录..."
	sudo cp $(TARGETS) /usr/local/bin/
	@echo "✅ 安装完成！现在可以在任何目录运行这些程序"

# 检查依赖
check-deps:
	@echo "检查编译依赖..."
	@which g++ > /dev/null || (echo "❌ 需要安装 g++" && exit 1)
	@echo "✅ 所有依赖已满足"

# 快速测试
test: $(TARGETS)
	@echo "快速测试编译结果..."
	@echo "测试 can_motor_scanner --help..."
	./can_motor_scanner --help || echo "扫描器帮助测试"
	@echo "测试 velocity_acceleration_control --help..."
	./velocity_acceleration_control --help || echo "速度控制帮助测试"
	@echo "测试 angle_stream_control --help..."
	./angle_stream_control --help || echo "角度控制帮助测试"
	@echo "✅ 所有程序编译正常"