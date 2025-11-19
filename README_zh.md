# LivelyBot 高擎电机控制库

基于高擎电机SDK协议表开发的电机控制库，支持Python、C++、Rust和Arduino实现。

🌐 **Languages**: [English](README.md) | [中文](README_zh.md) | [Español](README_es.md)

🔗 **官方SDK**: https://github.com/HighTorque-Robotics/livelybot_hardware_sdk

## 🚀 快速开始

### 环境配置

```bash
# 安装CAN工具
sudo apt-get install can-utils

# 配置CAN接口
sudo ip link set can0 type can bitrate 1000000
sudo ip link set up can0
```

### Python实现

```bash
cd python
pip install -r requirements.txt

# 扫描电机
python3 can_motor_scanner.py --channel can0

# 速度控制
python3 velocity_acceleration_control.py --motor_id 1 --mode interactive

# 角度控制
python3 angle_stream_control.py --motor_id 1 --mode interactive
```

### C++实现 ✅

```bash
cd cpp
make

# 扫描电机
./can_motor_scanner 1 5

# 速度控制
./velocity_acceleration_control 1

# 角度控制
./angle_stream_control 1 interactive
```

### Rust实现 ⏳ 待开发

```bash
# 待实现
cd rust
cargo run --release -- 1
```

## 📁 项目结构

```
CUS_02/
├── python/                 # Python实现
│   ├── can_motor_scanner.py           # 电机扫描工具
│   ├── velocity_acceleration_control.py # 速度+加速度控制
│   ├── angle_stream_control.py         # 角度流控制
│   └── requirements.txt               # Python依赖
├── cpp/                    # C++实现
│   ├── can_motor_scanner.cpp          # 电机扫描工具
│   ├── velocity_acceleration_control.cpp # 速度+加速度控制
│   ├── angle_stream_control.cpp       # 角度流控制
│   ├── Makefile                       # 构建系统
│   └── README.md                      # C++文档
├── rust/                   # Rust实现
│   ├── src/
│   └── Cargo.toml
├── arduino/                # Arduino实现
│   └── libraries/
└── scripts/                # 构建和配置脚本
```

## 🔧 功能特性

### Python实现 (100Hz, 5ms延迟) ✅
- ✅ CAN总线电机扫描
- ✅ 智能急刹速度控制
- ✅ 0x90流命令角度控制
- ✅ MIT风格阻抗控制
- ✅ 正弦波/阶梯/斜坡测试

### C++实现 (200Hz, 1ms延迟) ✅
- ✅ 高性能实时控制 (200Hz控制循环)
- ✅ 原生SocketCAN接口
- ✅ 多线程控制架构
- ✅ 智能急刹与最大减速
- ✅ MIT风格阻抗控制 (0x90流命令)
- ✅ 交互式和自动控制模式
- ✅ 正弦波、阶梯和多位置测试

### Rust实现 (150Hz, 2ms延迟) ⏳ 待开发
- ⏳ 内存安全保证
- ⏳ 异步控制架构
- ⏳ 跨平台支持

### Arduino实现 (50-200Hz, 2-20ms延迟) ⏳ 待开发
- ⏳ ESP32/Arduino支持
- ⏳ 低功耗运行
- ⏳ 实时反馈

## 📊 支持的电机

基于高擎电机SDK协议表：

| 电机型号 | 扭矩 | 最大速度 | 减速比 | 协议支持 |
|---------|------|----------|--------|----------|
| 5046_20 | 17 Nm | 50 rad/s | 20:1 | ✅ |
| 4538_19 | 17 Nm | 44 rad/s | 19:1 | ✅ |
| 5047_36 | 60 Nm | 50 rad/s | 36:1 | ✅ |
| 5047_09 | 17 Nm | 33 rad/s | 9:1 | ✅ |

## 🔌 CAN协议

### 通信架构
- **主控**: 通过CAN接口直接控制
- **电机**: 支持一拖多流控制
- **波特率**: 1Mbps (标准)
- **帧格式**: 扩展帧 (29位ID)

### 关键协议

#### 1. 电机扫描 (Ping)
```python
# CAN ID: 0x8000 | motor_id
# 数据: [0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50]
```

#### 2. 速度+加速度控制 (0xAD)
```python
# CAN ID: 0x00AD
# 数据: [PosL, PosH, VelL, VelH, AccL, AccH, 0x50, 0x50]
```

#### 3. 角度流控制 (0x90)
```python
# CAN ID: 0x0090
# 数据: [PosL, PosH, VelL, VelH, TqeL, TqeH, 0x50, 0x50]
```

## 🎯 使用示例

### Python电机扫描

```python
from python.can_motor_scanner import LivelyMotorScanner

scanner = LivelyMotorScanner('can0')
if scanner.connect():
    motors = scanner.scan_range(1, 14)
    print(f"发现电机: {motors}")
```

### Python速度控制

```python
from python.velocity_acceleration_control import MotorVelAccController

controller = MotorVelAccController('can0', motor_id=1)
controller.enable_sequence()
controller.start_control()
controller.set_velocity(5.0)  # 5 rad/s
```

### Python角度控制

```python
from python.angle_stream_control import MotorAngleStreamController

controller = MotorAngleStreamController('can0', motor_id=1)
controller.connect()
controller.enable_motor()
controller.set_angle(90.0)  # 90度
```

## 🛡️ 安全特性

- **力矩限制**: 可配置最大输出力矩
- **位置限制**: 支持软件限位
- **智能急刹**: 自动检测零速并强制最大减速度
- **通信监控**: 实时检测CAN通信状态
- **异常处理**: 完善的错误处理和恢复机制

## 📈 性能对比

| 语言 | 控制频率 | 延迟 | 内存使用 | 状态 | 适用平台 |
|------|----------|------|----------|------|----------|
| Python | 100 Hz | 5ms | 50MB | ✅ 已完成 | Linux开发 |
| C++ | 200 Hz | 1ms | 10MB | ✅ 已完成 | Linux生产 |
| Rust | 150 Hz | 2ms | 15MB | ⏳ 待开发 | Linux生产 |
| Arduino | 50-200Hz | 2-20ms | 10-50KB | ⏳ 待开发 | ESP32/MCU |

## 🔍 故障排除

### CAN接口问题
```bash
# 检查接口状态
ip link show can0

# 重新配置接口
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 1000000 restart-ms 100
```

### 权限问题
```bash
# 添加用户到dialout组
sudo usermod -a -G dialout $USER

# 或使用sudo运行
sudo python3 can_motor_scanner.py
```

### 硬件连接
- 确认120Ω终端电阻
- 检查CAN-H和CAN-L接线
- 验证电机供电正常
- 确认波特率设置一致

## 📚 文档

- 📄 [高擎电机SDK协议表](../高擎电机SDK协议表.md) - 完整的协议规范
- 🔗 [官方SDK](https://github.com/HighTorque-Robotics/livelybot_hardware_sdk) - 官方SDK仓库
- 📖 [Python文档](python/README.md) - Python实现详解
- ✅ [C++文档](cpp/README.md) - C++实现详解 (已完成)
- ⏳ [Rust文档](rust/README.md) - Rust实现详解 (待实现)
- ⏳ [Arduino文档](arduino/README.md) - Arduino实现详解 (待实现)

## 🗺️ 开发路线图

### ✅ 已完成
- [x] Python CAN协议实现
- [x] C++高性能实现 (200Hz控制循环)
- [x] 电机扫描工具 (Python & C++)
- [x] 速度+加速度控制与智能急刹 (Python & C++)
- [x] 角度流控制 (0x90命令) (Python & C++)
- [x] MIT风格阻抗控制 (Python & C++)
- [x] 多种测试模式 (交互式、正弦波、阶梯、多位置)
- [x] SocketCAN原生接口支持
- [x] 多线程控制架构

### ⏳ 待实现
- [ ] Rust内存安全实现
- [ ] Arduino/ESP32实现
- [ ] GUI控制界面
- [ ] 仿真测试平台
- [ ] 自动标定工具

### 🚀 未来计划
- [ ] 电机参数自动识别
- [ ] 批量电机管理
- [ ] 实时数据可视化
- [ ] 远程控制接口
- [ ] 故障诊断工具

## 🤝 贡献

欢迎提交Issue和Pull Request！

### 如何贡献
1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

## 📄 许可证

MIT License - 详见LICENSE文件

## 相关链接

- [高擎机器人官方SDK](https://github.com/HighTorque-Robotics/livelybot_hardware_sdk) - 协议参考
- [RobStride Control](https://github.com/tianrking/RobStride_Control) - 相关项目：RobStride & XIAOMI 电机控制

---

*基于高擎电机SDK协议表开发，为机器人控制提供高性能解决方案*