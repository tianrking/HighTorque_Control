#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
高擎电机角度流控制
基于 03_angle.py 协议实现
支持 0x90 流命令和MIT风格控制
"""

import can
import struct
import time
import sys
import math
import argparse


class MotorAngleStreamController:
    """高擎电机角度流控制器"""

    def __init__(self, channel: str = 'can0', bitrate: int = 1000000, motor_id: int = 1):
        self.motor_id = motor_id
        self.channel = channel
        self.bitrate = bitrate
        self.bus = None

        # SDK 系数定义
        self.FACTOR_POS = 10000.0  # 1圈 = 10000
        self.FACTOR_VEL = 4000.0   # 1r/s = 4000
        self.FACTOR_TQE = 200.0   # 通用电机系数

    def connect(self) -> bool:
        """连接CAN总线"""
        print(f"初始化 CAN: {self.channel}")
        try:
            self.bus = can.interface.Bus(channel=self.channel, interface='socketcan')
            # 清空缓冲区
            while self.bus.recv(timeout=0.0):
                pass
            print("✅ CAN接口连接成功")
            return True
        except OSError:
            print("❌ 错误: CAN 接口未打开")
            return False

    def send_frame(self, arbitration_id, data):
        """发送CAN帧"""
        msg = can.Message(arbitration_id=arbitration_id, data=data,
                         is_extended_id=True, is_fd=False)
        try:
            self.bus.send(msg)
        except can.CanError:
            pass

    def enable_motor(self):
        """
        使能步骤：需要先用 Mode 1 (ID 0x0001) 激活电机进入 0x0A 模式
        """
        print(f"-> [ID {self.motor_id}] 发送使能指令 (Register Mode)...")
        # 1. 写入模式: 0x0A (Position Mode)
        # ID: 0x0001 (Cmd 0x01 Write Int8)
        arb_id = 0x0000 | self.motor_id
        self.send_frame(arb_id, [0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50])
        time.sleep(0.05)

        # 2. 预设 PID (给一点刚度)
        # Reg 0x23 (Kp) = 1.0
        self.send_frame(arb_id, [0x0D, 0x23] + list(struct.pack('<f', 1.0)) + [0x50, 0x50])
        time.sleep(0.02)
        # Reg 0x24 (Kd) = 0.1
        self.send_frame(arb_id, [0x0D, 0x24] + list(struct.pack('<f', 0.1)) + [0x50, 0x50])

        print("✅ 电机已激活，准备发送流控制指令")

    def send_0x90_command(self, angle_deg, max_vel_rps, max_tqe_nm):
        """
        发送 0x90 命令帧 (一拖多模式 / 流模式)
        ID: 0x0090 (由 SDK 0x90 推断)
        Payload: [Pos(int16), Vel(int16), Tqe(int16), Padding...]
        """
        # 1. 计算数值
        pos_int = int((angle_deg / 360.0) * self.FACTOR_POS)
        vel_int = int(max_vel_rps * self.FACTOR_VEL)
        tqe_int = int(max_tqe_nm * self.FACTOR_TQE)

        # 限幅 Int16
        pos_int = max(min(pos_int, 32767), -32768)
        vel_int = max(min(vel_int, 32767), -32768)
        tqe_int = max(min(tqe_int, 32767), -32768)

        # 2. 打包数据
        # 结构: [PosL, PosH, VelL, VelH, TqeL, TqeH]
        data = struct.pack('<hhh', pos_int, vel_int, tqe_int)

        # 补齐 8 字节
        payload = list(data) + [0x50, 0x50]

        # 3. 发送至 ID 0x0090 (不需回复) 或 0x8090 (需回复)
        # 这里尝试 0x0090
        self.send_frame(0x0090, payload)

        print(f"   >>> 0x90流指令: Ang={angle_deg}° Vel={max_vel_rps} Tqe={max_tqe_nm} (Raw: {payload[:6]})")

    def disable_motor(self):
        """禁用电机"""
        arb_id = 0x0000 | self.motor_id
        self.send_frame(arb_id, [0x01, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50])
        print("🛑 电机已禁用")

    def set_angle(self, angle_deg, max_vel_rps=2.0, max_tqe_nm=3.0, send_count=5):
        """设置角度位置"""
        for i in range(send_count):
            self.send_0x90_command(angle_deg, max_vel_rps, max_tqe_nm)
            time.sleep(0.01)

    def run_interactive_control(self):
        """运行交互式角度控制"""
        print("="*50)
        print("🚀 0x90 流控制模式 (复刻 SDK) ")
        print("输入角度 (如 90) 回车。")
        print("默认参数: 限速 2.0 r/s, 限矩 3.0 Nm")
        print("输入 q 退出")
        print("="*50)

        try:
            # 1. 先使能电机
            self.enable_motor()

            while True:
                raw = input(f"\n(Stream 0x90) > ").strip()

                if raw in ['q', 'exit']:
                    break
                if not raw:
                    continue

                try:
                    deg = float(raw)
                    # 持续发送几次确保收到 (流模式通常需要高频发送)
                    self.set_angle(deg)
                except ValueError:
                    print("输入错误")

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.disable_motor()

    def run_sine_wave(self, amplitude_deg: float, frequency: float, duration: float):
        """运行正弦波角度控制"""
        print("="*50)
        print("🌊 正弦波角度控制")
        print(f"幅值: {amplitude_deg}°, 频率: {frequency} Hz, 时长: {duration}s")
        print("="*50)

        try:
            # 1. 先使能电机
            self.enable_motor()

            start_time = time.time()
            while time.time() - start_time < duration:
                # 计算目标角度
                elapsed = time.time() - start_time
                target_deg = amplitude_deg * math.sin(2 * math.pi * frequency * elapsed)

                # 发送角度指令
                self.set_angle(target_deg)

                # 显示当前状态
                print(f"\r目标: {target_deg:7.1f}°", end="")
                sys.stdout.flush()

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.disable_motor()

    def run_step_control(self, angles: list, step_duration: float):
        """运行阶梯角度控制"""
        print("="*50)
        print("📈 阶梯角度控制")
        print(f"角度序列: {angles}°")
        print(f"每步时长: {step_duration}s")
        print("="*50)

        try:
            # 1. 先使能电机
            self.enable_motor()

            for i, angle_deg in enumerate(angles):
                print(f"\n--- 步骤 {i+1}/{len(angles)}: {angle_deg}° ---")

                # 发送角度指令
                self.set_angle(angle_deg)

                # 等待步长时间
                step_start = time.time()
                while time.time() - step_start < step_duration:
                    print(f"\r剩余时间: {step_duration - (time.time() - step_start):.1f}s", end="")
                    sys.stdout.flush()
                    time.sleep(0.1)

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.disable_motor()

    def run_mit_control(self, target_deg: float, stiffness: float, damping: float, duration: float):
        """运行MIT控制风格的角度控制"""
        print("="*50)
        print("🤖 MIT风格阻抗控制")
        print(f"目标角度: {target_deg}°")
        print(f"刚度: {stiffness} Nm/rad")
        print(f"阻尼: {damping} Nm·s/rad")
        print(f"控制时长: {duration}s")
        print("="*50)

        try:
            # 1. 先使能电机
            self.enable_motor()

            start_time = time.time()
            last_error = 0.0
            last_time = start_time

            while time.time() - start_time < duration:
                # 获取当前状态 (需要实现状态读取)
                # 这里简化处理，实际应该读取电机反馈
                current_time = time.time()
                dt = current_time - last_time

                # 简化的误差计算 (实际应该读取电机当前位置)
                # 这里假设角度直接对应位置
                error = math.radians(target_deg) - math.radians(target_deg)  # 临时简化

                # MIT控制律
                desired_torque = stiffness * error + damping * (error - last_error) / (dt + 0.001)

                # 发送控制指令 (转换为角度+速度+力矩)
                self.send_0x90_command(target_deg, 2.0, abs(desired_torque))

                print(f"\r目标: {target_deg:6.1f}° 力矩: {desired_torque:6.3f}Nm", end="")
                sys.stdout.flush()

                last_error = error
                last_time = current_time
                time.sleep(0.01)  # 100Hz控制频率

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.disable_motor()

    def test_positions(self, positions: list):
        """测试多个角度位置"""
        print("="*50)
        print("🧪 多位置测试")
        print(f"测试位置: {positions}°")
        print("="*50)

        try:
            # 1. 先使能电机
            self.enable_motor()

            for i, angle_deg in enumerate(positions):
                print(f"\n--- 测试位置 {i+1}/{len(positions)}: {angle_deg}° ---")

                # 发送角度指令
                self.set_angle(angle_deg)

                # 等待稳定
                print("等待2秒稳定...", end="")
                sys.stdout.flush()
                time.sleep(2.0)

        except KeyboardInterrupt:
            print("\n中断")
        finally:
            self.disable_motor()

    def cleanup(self):
        """清理资源"""
        if self.bus:
            self.bus.shutdown()
            self.bus = None


def main():
    parser = argparse.ArgumentParser(description='高擎电机角度流控制')
    parser.add_argument('--channel', type=str, default='can0', help='CAN通道')
    parser.add_argument('--bitrate', type=int, default=1000000, help='CAN波特率')
    parser.add_argument('--motor_id', type=int, default=1, help='电机ID')

    parser.add_argument('--mode', type=str, default='interactive',
                       choices=['interactive', 'sine', 'step', 'mit', 'test'],
                       help='控制模式')

    parser.add_argument('--amplitude', type=float, default=90.0, help='正弦波幅值')
    parser.add_argument('--frequency', type=float, default=0.2, help='正弦波频率')
    parser.add_argument('--duration', type=float, default=10.0, help='测试时长')
    parser.add_argument('--angles', type=str, help='阶梯角度序列(逗号分隔)')
    parser.add_argument('--step_duration', type=float, default=2.0, help='阶梯步长')
    parser.add_argument('--stiffness', type=float, default=50.0, help='MIT刚度(Nm/rad)')
    parser.add_argument('--damping', type=float, default=5.0, help='MIT阻尼(Nm·s/rad)')
    parser.add_argument('--target', type=float, help='MIT目标角度')

    parser.add_argument('--test_angles', type=str, help='测试角度序列(逗号分隔)')

    args = parser.parse_args()

    # 创建控制器
    controller = MotorAngleStreamController(args.channel, args.bitrate, args.motor_id)

    try:
        # 连接CAN总线
        if not controller.connect():
            sys.exit(1)

        # 根据模式运行
        if args.mode == 'interactive':
            controller.run_interactive_control()
        elif args.mode == 'sine':
            controller.run_sine_wave(args.amplitude, args.frequency, args.duration)
        elif args.mode == 'step':
            if not args.angles:
                print("阶梯模式需要 --angles 参数")
                return
            angles = [float(a.strip()) for a in args.angles.split(',')]
            controller.run_step_control(angles, args.step_duration)
        elif args.mode == 'mit':
            if args.target is None:
                print("MIT模式需要 --target 参数")
                return
            controller.run_mit_control(args.target, args.stiffness, args.damping, args.duration)
        elif args.mode == 'test':
            if not args.test_angles:
                print("测试模式需要 --test_angles 参数")
                return
            positions = [float(p.strip()) for p in args.test_angles.split(',')]
            controller.test_positions(positions)

    except KeyboardInterrupt:
        print("\n⚠️ 用户中断")
    except Exception as e:
        print(f"\n❌ 控制过程中出错: {e}")
    finally:
        controller.cleanup()


if __name__ == "__main__":
    main()