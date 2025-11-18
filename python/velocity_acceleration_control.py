#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
高擎电机速度+加速度控制
基于 02_velocity_demo.py 协议实现
支持智能急刹功能
"""

import can
import struct
import time
import sys
import threading
import argparse
import math


class MotorVelAccController:
    """高擎电机速度+加速度控制器"""

    def __init__(self, channel: str = 'can0', bitrate: int = 1000000, motor_id: int = 1):
        self.motor_id = motor_id
        self.channel = channel
        print(f"初始化 CAN: {channel}")
        try:
            self.bus = can.interface.Bus(channel=channel, interface='socketcan')
            # 清空缓冲区
            while self.bus.recv(timeout=0.0):
                pass
        except OSError:
            print("❌ 错误: CAN 接口未打开")
            sys.exit(1)

        self.running = False
        self.control_thread = None

        # 控制参数
        self.target_vel = 0.0
        self.target_acc = 5.0  # 默认加速度

        # 系数定义 (源自 SDK & PDF)
        self.FACTOR_VEL = 4000.0   # 1 r/s = 4000
        self.FACTOR_ACC = 1000.0   # 1 r/s^2 = 1000
        self.MAGIC_POS = -32768    # 0x8000 (Int16 Min) -> 代表"无位置限制"

        # 协议物理极限: 32767 / 1000 = 32.767 r/s^2
        # 我们设置急刹时的加速度为 30.0，接近极限
        self.MAX_BRAKE_ACC = 30.0

    def send_frame(self, arbitration_id, data):
        """发送CAN帧"""
        msg = can.Message(arbitration_id=arbitration_id, data=data,
                         is_extended_id=True, is_fd=False)
        try:
            self.bus.send(msg)
        except can.CanError:
            print("❌ 发送错误")

    def enable_sequence(self):
        """初始化: 进模式 + 给力矩"""
        print(f"-> [ID {self.motor_id}] 初始化 (Vel+Acc Mode 0xAD)...")
        arb_id = 0x0000 | self.motor_id

        # 1. 写入模式: 0x0A (Position/Control Mode)
        self.send_frame(arb_id, [0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50])
        time.sleep(0.05)

        # 2. 【必须】设置力矩限制 (Reg 0x22)
        # 设为 3.0 Nm，确保有力气
        print("   >>> 预设力矩限制: 3.0 Nm")
        self.send_frame(arb_id, [0x0D, 0x22] + list(struct.pack('<f', 3.0)) + [0x50, 0x50])
        time.sleep(0.02)

        # 3. 预设 PID (速度环)
        # Kp=2.0, Kd=0.2
        self.send_frame(arb_id, [0x0D, 0x23] + list(struct.pack('<f', 2.0)) + [0x50, 0x50])
        self.send_frame(arb_id, [0x0D, 0x24] + list(struct.pack('<f', 0.2)) + [0x50, 0x50])

        print("✅ 初始化完成")

    def control_loop(self):
        """100Hz 发送 0xAD 指令流"""
        while self.running:
            # 1. 准备位置数据 (Int16) -> 0x8000 代表速度模式
            pos_int = self.MAGIC_POS

            # 2. 准备速度数据
            vel_int = int(self.target_vel * self.FACTOR_VEL)
            vel_int = max(min(vel_int, 32767), -32768)

            # 3. 准备加速度数据 (核心修改)
            current_acc = self.target_acc

            # --- 智能刹车逻辑 ---
            # 如果目标速度是 0 (刹车)，且用户设定的加速度很小
            # 强制使用最大加速度 (30.0)，实现"立刻停"
            if self.target_vel == 0.0:
                current_acc = self.MAX_BRAKE_ACC
            # ------------------

            acc_int = int(current_acc * self.FACTOR_ACC)
            acc_int = max(min(acc_int, 32767), -32768)

            # 4. 打包发送: [Pos, Vel, Acc] (3个 short)
            # CAN ID: 0x00AD
            data = struct.pack('<hhh', pos_int, vel_int, acc_int)
            payload = list(data) + [0x50, 0x50]  # 补齐8字节

            self.send_frame(0x00AD, payload)

            time.sleep(0.01)  # 10ms 周期 (100Hz)

    def start_control(self):
        """启动控制线程"""
        if not self.running:
            self.running = True
            self.control_thread = threading.Thread(target=self.control_loop)
            self.control_thread.start()

    def stop_control(self):
        """停止控制线程"""
        self.running = False
        if self.control_thread:
            self.control_thread.join()

    def set_velocity(self, velocity: float):
        """设置目标速度"""
        self.target_vel = velocity
        if velocity == 0.0:
            print(f"   -> 🛑 执行急刹 (Acc={self.MAX_BRAKE_ACC})")
        else:
            print(f"   -> 目标速度: {velocity} r/s")

    def set_acceleration(self, acceleration: float):
        """设置加速度"""
        self.target_acc = abs(acceleration)
        print(f"   -> 行驶加速度设为: {self.target_acc} r/s^2")

    def disable(self):
        """禁用电机"""
        arb_id = 0x0000 | self.motor_id
        self.send_frame(arb_id, [0x01, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50])
        print("🛑 失能")

    def run_sine_wave_test(self, amplitude: float, frequency: float, duration: float):
        """运行正弦波速度测试"""
        print(f"\n{'='*50}")
        print(f"🌊 正弦波速度测试")
        print(f"幅值: {amplitude} r/s, 频率: {frequency} Hz, 时长: {duration}s")
        print(f"{'='*50}")

        start_time = time.time()
        try:
            while time.time() - start_time < duration:
                elapsed = time.time() - start_time
                # 正弦波速度
                target_vel = amplitude * math.sin(2 * math.pi * frequency * elapsed)
                self.set_velocity(target_vel)
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\n⚠️ 测试中断")
        finally:
            self.set_velocity(0.0)

    def run_step_test(self, velocities: list, step_duration: float):
        """运行阶梯速度测试"""
        print(f"\n{'='*50}")
        print(f"📈 阶梯速度测试")
        print(f"速度序列: {velocities} r/s")
        print(f"每步时长: {step_duration}s")
        print(f"{'='*50}")

        try:
            for i, velocity in enumerate(velocities):
                print(f"\n--- 步骤 {i+1}/{len(velocities)}: {velocity} r/s ---")
                self.set_velocity(velocity)
                time.sleep(step_duration)
        except KeyboardInterrupt:
            print("\n⚠️ 测试中断")
        finally:
            self.set_velocity(0.0)

    def run_ramp_test(self, start_vel: float, end_vel: float, duration: float):
        """运行斜坡速度测试"""
        print(f"\n{'='*50}")
        print(f"📊 斜坡速度测试")
        print(f"从 {start_vel} r/s 到 {end_vel} r/s, 时长: {duration}s")
        print(f"{'='*50}")

        start_time = time.time()
        try:
            while time.time() - start_time < duration:
                elapsed = time.time() - start_time
                progress = elapsed / duration
                # 线性插值
                target_vel = start_vel + (end_vel - start_vel) * progress
                self.set_velocity(target_vel)
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\n⚠️ 测试中断")
        finally:
            self.set_velocity(0.0)

    def run_interactive_mode(self):
        """运行交互式控制模式"""
        print(f"\n{'='*50}")
        print(f"🏎️  速度+加速度模式 (智能急刹版)")
        print(f"电机ID: {self.motor_id}")
        print(f"指令格式:")
        print(f"  [速度]        -> 设定目标速度 (如 5.0, -2.0)")
        print(f"  acc [数值]    -> 设定正常行驶的加速度 (如 acc 10.0)")
        print(f"  0             -> 触发急刹 (加速度自动拉满到 30.0)")
        print(f"  q             -> 退出")
        print(f"{'='*50}")

        try:
            while True:
                # 显示当前状态
                status = f"\r(Vel={self.target_vel:.1f}, Acc={self.target_acc:.1f}) > "
                sys.stdout.write(status)
                sys.stdout.flush()

                raw = input().strip().lower()

                if raw in ['q', 'exit']:
                    break
                if not raw:
                    continue

                parts = raw.split()

                try:
                    if parts[0] == 'acc':
                        val = float(parts[1])
                        self.set_acceleration(val)
                    else:
                        val = float(parts[0])
                        self.set_velocity(val)
                except ValueError:
                    print("输入错误")

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.set_velocity(0.0)


def main():
    parser = argparse.ArgumentParser(description='高擎电机速度+加速度控制')
    parser.add_argument('--channel', type=str, default='can0', help='CAN通道')
    parser.add_argument('--bitrate', type=int, default=1000000, help='CAN波特率')
    parser.add_argument('--motor_id', type=int, default=1, help='电机ID')
    parser.add_argument('--mode', type=str, default='interactive',
                       choices=['interactive', 'sine', 'step', 'ramp'],
                       help='控制模式')

    # 测试参数
    parser.add_argument('--amplitude', type=float, default=2.0, help='正弦波幅值')
    parser.add_argument('--frequency', type=float, default=0.5, help='正弦波频率')
    parser.add_argument('--duration', type=float, default=10.0, help='测试时长')
    parser.add_argument('--velocities', type=str, help='阶梯速度序列(逗号分隔)')
    parser.add_argument('--step_duration', type=float, default=2.0, help='阶梯步长')
    parser.add_argument('--start_vel', type=float, help='斜坡起始速度')
    parser.add_argument('--end_vel', type=float, help='斜坡结束速度')

    args = parser.parse_args()

    # 创建控制器
    controller = MotorVelAccController(args.channel, args.bitrate, args.motor_id)

    try:
        # 使能电机
        controller.enable_sequence()
        time.sleep(0.1)

        # 启动控制线程
        controller.start_control()

        # 根据模式运行
        if args.mode == 'interactive':
            controller.run_interactive_mode()
        elif args.mode == 'sine':
            controller.run_sine_wave_test(args.amplitude, args.frequency, args.duration)
        elif args.mode == 'step':
            if not args.velocities:
                print("阶梯模式需要 --velocities 参数")
                return
            velocities = [float(v.strip()) for v in args.velocities.split(',')]
            controller.run_step_test(velocities, args.step_duration)
        elif args.mode == 'ramp':
            if args.start_vel is None or args.end_vel is None:
                print("斜坡模式需要 --start_vel 和 --end_vel 参数")
                return
            controller.run_ramp_test(args.start_vel, args.end_vel, args.duration)

    except KeyboardInterrupt:
        print("\n⚠️ 用户中断")
    except Exception as e:
        print(f"❌ 控制过程中出错: {e}")
    finally:
        # 停止控制
        controller.stop_control()
        controller.disable()
        if controller.bus:
            controller.bus.shutdown()


if __name__ == "__main__":
    main()