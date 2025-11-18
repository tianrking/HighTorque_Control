# Python实现

高擎电机控制的Python实现，支持CAN总线直接通信和多种控制模式。

## 🚀 快速开始

### 安装依赖

```bash
pip install python-can numpy
```

### 环境配置

```bash
# 配置CAN接口
sudo ip link set can0 type can bitrate 1000000
sudo ip link set up can0
```

### 基本使用

```python
# 1. 扫描电机
python3 can_motor_scanner.py --channel can0

# 2. 速度控制
python3 velocity_acceleration_control.py --motor_id 1 --mode interactive

# 3. 角度控制
python3 angle_stream_control.py --motor_id 1 --mode interactive
```

## 📁 文件说明

### can_motor_scanner.py
电机扫描工具，用于发现总线上的电机。

**功能特性:**
- 基于真实协议的准确扫描
- 支持详细电机信息获取
- 通信质量测试
- 持续监控模式
- 扫描报告生成

**使用示例:**
```bash
# 基础扫描
python3 can_motor_scanner.py --channel can0

# 详细扫描
python3 can_motor_scanner.py --channel can0 --detailed

# 测试通信质量
python3 can_motor_scanner.py --channel can0 --test 1

# 持续监控
python3 can_motor_scanner.py --channel can0 --monitor 30

# 保存报告
python3 can_motor_scanner.py --channel can0 --save report.txt
```

### velocity_acceleration_control.py
速度+加速度控制器，支持智能急刹功能。

**功能特性:**
- 100Hz控制频率
- 智能急刹 (零速时自动使用最大减速度)
- 多种测试模式 (正弦波、阶梯、斜坡)
- 交互式控制
- 实时状态显示

**使用示例:**
```bash
# 交互式控制
python3 velocity_acceleration_control.py --motor_id 1

# 正弦波测试
python3 velocity_acceleration_control.py --motor_id 1 --mode sine --amplitude 3.0 --frequency 0.5

# 阶梯速度测试
python3 velocity_acceleration_control.py --motor_id 1 --mode step --velocities "0,2,4,0,-2,0" --step_duration 2.0

# 斜坡速度测试
python3 velocity_acceleration_control.py --motor_id 1 --mode ramp --start_vel 0 --end_vel 5.0
```

### angle_stream_control.py
角度流控制器，支持0x90流命令和MIT风格控制。

**功能特性:**
- 基于SDK的0x90流命令
- MIT风格阻抗控制
- 正弦波角度跟踪
- 阶梯角度控制
- 位置测试

**使用示例:**
```bash
# 交互式角度控制
python3 angle_stream_control.py --motor_id 1

# 正弦波角度测试
python3 angle_stream_control.py --motor_id 1 --mode sine --amplitude 90.0 --frequency 0.2

# 阶梯角度测试
python3 angle_stream_control.py --motor_id 1 --mode step --angles "0,45,90,45,0" --step_duration 3.0

# MIT阻抗控制
python3 angle_stream_control.py --motor_id 1 --mode mit --target 90.0 --stiffness 50.0 --damping 5.0

# 多位置测试
python3 angle_stream_control.py --motor_id 1 --mode test --test_angles "0,30,60,90,60,30,0"
```

## 🔧 技术细节

### CAN通信协议

#### 电机扫描协议
- **CAN ID**: `0x8000 | motor_id` (Bit15=1表示需要回复)
- **数据帧**: `[0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50]`
- **功能**: 读取电机模式，测试连接

#### 速度控制协议 (0xAD)
- **CAN ID**: `0x00AD`
- **数据帧**: `[PosL, PosH, VelL, VelH, AccL, AccH, 0x50, 0x50]`
- **位置**: `0x8000` (Magic Pos，表示速度模式)
- **速度系数**: `1 r/s = 4000`
- **加速度系数**: `1 r/s² = 1000`

#### 角度控制协议 (0x90)
- **CAN ID**: `0x0090`
- **数据帧**: `[PosL, PosH, VelL, VelH, TqeL, TqeH, 0x50, 0x50]`
- **位置系数**: `1圈 = 10000`
- **速度系数**: `1 r/s = 4000`
- **力矩系数**: `通用 1/0.005 = 200`

### 数据转换

#### 速度转换
```python
# r/s -> int16
vel_int = int(velocity * FACTOR_VEL)
# int16 -> r/s
velocity = vel_int / FACTOR_VEL
```

#### 角度转换
```python
# 度 -> int16 (圈数)
pos_int = int((angle_deg / 360.0) * FACTOR_POS)
# int16 -> 度
angle_deg = pos_int / FACTOR_POS * 360.0
```

#### 力矩转换
```python
# Nm -> int16
tqe_int = int(torque_nm * FACTOR_TQE)
# int16 -> Nm
torque_nm = tqe_int / FACTOR_TQE
```

## 🛡️ 安全特性

### 智能急刹
当目标速度设置为0时，自动使用最大加速度(30.0 r/s²)实现快速停止。

```python
if self.target_vel == 0.0:
    current_acc = self.MAX_BRAKE_ACC  # 30.0 r/s²
```

### 力矩限制
所有控制模式都支持力矩限制，防止过载损坏。

### 通信监控
实时监控CAN总线通信状态，检测通信异常。

## 📊 性能参数

- **控制频率**: 100Hz
- **延迟**: ~5ms
- **内存使用**: ~50MB
- **支持电机数**: 理论上127个/总线
- **CAN波特率**: 1Mbps

## 🔍 调试技巧

### 查看CAN流量
```bash
# 监听CAN流量
candump can0

# 查看CAN错误
candump -e can0
```

### 调试模式
```python
# 启用详细日志
import logging
logging.basicConfig(level=logging.DEBUG)

# 发送单个测试帧
python3 -c "
import can
bus = can.Bus('can0')
msg = can.Message(arbitration_id=0x8001, data=[0x11,0x00,0x50,0x50,0x50,0x50,0x50,0x50], is_extended_id=True)
bus.send(msg)
"
```

## 🧪 测试建议

### 硬件测试
1. 先运行扫描工具确认电机连接
2. 使用小幅度速度测试验证响应
3. 逐步增加控制幅度

### 软件测试
1. 使用虚拟CAN接口测试协议
2. 验证数据转换精度
3. 测试异常处理机制

## ❓ 常见问题

### Q: 扫描不到电机
**A:** 检查以下几点：
- CAN接口是否正确配置
- 120Ω终端电阻是否连接
- 电机供电是否正常
- 波特率设置是否一致

### Q: 控制无响应
**A:** 确认：
- 电机是否已使能(enable)
- 控制模式是否正确
- 数据格式是否符合协议

### Q: 电机运动不准确
**A:** 检查：
- 传感器校准
- 控制参数设置
- 机械负载情况

## 📚 参考资料

- [高擎电机SDK协议表](../../高擎电机SDK协议表.md)
- [Python-CAN文档](https://python-can.readthedocs.io/)
- [Linux SocketCAN](https://www.kernel.org/doc/Documentation/networking/can.txt)

---

*基于高擎电机SDK协议表开发*