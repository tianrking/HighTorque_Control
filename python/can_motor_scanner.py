#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
高擎电机CAN总线扫描工具
基于实际协议实现，准确扫描总线上的电机ID
协议参考: 01_scan_motors.py
"""

import can
import time
import struct
import argparse
import sys
from typing import List, Dict, Optional, Tuple


class LivelyMotorScanner:
    """高擎电机CAN扫描器"""

    def __init__(self, channel: str = 'can0', bitrate: int = 1000000):
        self.channel = channel
        self.bitrate = bitrate
        self.bus = None

    def connect(self) -> bool:
        """连接CAN总线"""
        print(f"正在初始化 {self.channel} @ {self.bitrate}bps ...")
        try:
            self.bus = can.interface.Bus(channel=self.channel, interface='socketcan')
            # 清空缓冲区
            while self.bus.recv(timeout=0.01):
                pass
            print("✅ CAN接口连接成功")
            return True
        except OSError as e:
            print(f"❌ 错误: 无法打开接口 {self.channel}")
            print("请先运行: sudo ip link set can0 up type can bitrate 1000000 restart-ms 100")
            return False

    def disconnect(self):
        """断开CAN连接"""
        if self.bus:
            self.bus.shutdown()
            self.bus = None

    def build_ping_frame(self, motor_id: int) -> can.Message:
        """
        构建Ping指令帧
        协议: ID 高8位(Bit15=1表示需回复) | 低8位(电机ID)
        CMD: 0x11 = 读(0x1_) + int8(0x_0) + 1个数据(0x_1)
        地址: 0x00 = 读取电机模式
        """
        # CAN ID: 高8位设置Bit15=1表示需要回复，低8位为电机ID
        arbitration_id = 0x8000 | (motor_id & 0xFF)

        # 数据: CMD 0x11 + 地址 0x00 + 填充 0x50
        data = [0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50]

        return can.Message(
            arbitration_id=arbitration_id,
            data=data,
            is_extended_id=True,  # 必须开启扩展帧以支持16位ID
            is_fd=False           # 强制普通CAN
        )

    def parse_response(self, rx_msg: can.Message, target_id: int) -> Optional[int]:
        """解析响应消息，返回检测到的电机ID"""
        can_id = rx_msg.arbitration_id
        raw_id = can_id & 0xFFFF

        # 尝试解析源ID
        source_id = (raw_id >> 8) & 0x7F
        direct_id = can_id & 0xFF

        detected_id = None
        if 0 < source_id < 128:
            detected_id = source_id
        elif direct_id == target_id:
            detected_id = direct_id

        return detected_id

    def scan_single_motor(self, motor_id: int, timeout: float = 0.05) -> bool:
        """扫描单个电机ID"""
        try:
            # 发送ping帧
            ping_msg = self.build_ping_frame(motor_id)
            self.bus.send(ping_msg)
            time.sleep(0.01)  # 防止发送太快

            # 监听响应
            time_end = time.time() + timeout
            while time.time() < time_end:
                rx_msg = self.bus.recv(timeout=0.01)
                if rx_msg and not rx_msg.is_error_frame:
                    detected_id = self.parse_response(rx_msg, motor_id)
                    if detected_id:
                        print(f"✅ [响应] 发现电机 ID: {detected_id} (CAN ID: 0x{rx_msg.arbitration_id:X})")
                        return True
            return False

        except can.CanError as e:
            print(f"❌ 发送失败 (ID {motor_id}): {e}")
            print(">>> 严重警告: 物理层不通，请检查接线和120Ω电阻！")
            return False

    def scan_range(self, start_id: int = 1, end_id: int = 14, timeout: float = 0.05) -> List[int]:
        """扫描ID范围内的所有电机"""
        print(f"\n{'='*50}")
        print(f"开始扫描电机 ID (范围: {start_id}-{end_id})...")
        print(f"超时时间: {timeout}秒/电机")
        print("按 Ctrl+C 可随时停止")
        print(f"{'='*50}")

        found_ids = []

        try:
            for target_id in range(start_id, end_id + 1):
                print(f"扫描 ID {target_id:2d}...", end=" ")
                if self.scan_single_motor(target_id, timeout):
                    found_ids.append(target_id)
                else:
                    print("无响应")

        except KeyboardInterrupt:
            print("\n⚠️ 用户中断扫描")

        return found_ids

    def get_motor_info(self, motor_id: int) -> Optional[Dict]:
        """获取电机详细信息"""
        print(f"\n获取电机 {motor_id} 详细信息...")

        # 发送读取电机模式命令
        try:
            arbitration_id = 0x8000 | (motor_id & 0xFF)
            data = [0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50]
            msg = can.Message(arbitration_id=arbitration_id, data=data, is_extended_id=True, is_fd=False)
            self.bus.send(msg)

            # 监听响应
            time_end = time.time() + 0.1
            while time.time() < time_end:
                rx_msg = self.bus.recv(timeout=0.02)
                if rx_msg and not rx_msg.is_error_frame:
                    detected_id = self.parse_response(rx_msg, motor_id)
                    if detected_id == motor_id:
                        info = {
                            'id': motor_id,
                            'can_id': f"0x{rx_msg.arbitration_id:X}",
                            'data': list(rx_msg.data),
                            'length': len(rx_msg.data),
                            'timestamp': time.time()
                        }

                        # 尝试解析电机模式
                        if len(rx_msg.data) >= 2:
                            mode = rx_msg.data[1]
                            info['mode'] = f"0x{mode:02X}"
                            mode_names = {
                                0x00: "停止模式",
                                0x0A: "位置模式",
                                0x0B: "速度模式",
                                0x0C: "力矩模式"
                            }
                            if mode in mode_names:
                                info['mode_name'] = mode_names[mode]

                        return info

        except Exception as e:
            print(f"获取电机信息失败: {e}")

        return None

    def continuous_monitor(self, motor_ids: List[int], duration: float = 30.0):
        """持续监控指定电机"""
        print(f"\n{'='*50}")
        print(f"持续监控模式")
        print(f"监控电机: {motor_ids}")
        print(f"监控时长: {duration} 秒")
        print("按 Ctrl+C 提前停止")
        print(f"{'='*50}")

        start_time = time.time()

        try:
            while time.time() - start_time < duration:
                current_time = time.time() - start_time
                print(f"\n时间: {current_time:.1f}s")
                print("-" * 40)

                for motor_id in motor_ids:
                    info = self.get_motor_info(motor_id)
                    if info:
                        mode_name = info.get('mode_name', '未知模式')
                        print(f"电机 {motor_id:2d}: CAN ID={info['can_id']:<8} 模式={mode_name}")
                    else:
                        print(f"电机 {motor_id:2d}: 通信失败")

                time.sleep(1.0)

        except KeyboardInterrupt:
            print("\n⚠️ 用户中断监控")

    def test_motor_communication(self, motor_id: int) -> bool:
        """测试电机通信可靠性"""
        print(f"\n测试电机 {motor_id} 通信可靠性...")

        success_count = 0
        total_tests = 10

        for i in range(total_tests):
            if self.scan_single_motor(motor_id, timeout=0.02):
                success_count += 1
            time.sleep(0.1)

        reliability = (success_count / total_tests) * 100
        print(f"通信可靠性: {success_count}/{total_tests} ({reliability:.1f}%)")

        if reliability >= 90:
            print("✅ 通信质量优秀")
            return True
        elif reliability >= 70:
            print("⚠️ 通信质量一般")
            return True
        else:
            print("❌ 通信质量较差")
            return False

    def generate_report(self, found_ids: List[int]) -> Dict:
        """生成扫描报告"""
        report = {
            'timestamp': time.time(),
            'channel': self.channel,
            'bitrate': self.bitrate,
            'found_motors': found_ids,
            'motor_count': len(found_ids),
            'scan_range': f"1-14",
            'details': {}
        }

        # 获取每个电机的详细信息
        for motor_id in found_ids:
            info = self.get_motor_info(motor_id)
            if info:
                report['details'][motor_id] = info

        return report


def main():
    parser = argparse.ArgumentParser(description='高擎电机CAN总线扫描工具')
    parser.add_argument('--channel', type=str, default='can0', help='CAN通道名称')
    parser.add_argument('--bitrate', type=int, default=1000000, help='CAN波特率')
    parser.add_argument('--start', type=int, default=1, help='扫描起始ID')
    parser.add_argument('--end', type=int, default=14, help='扫描结束ID')
    parser.add_argument('--timeout', type=float, default=0.05, help='每个电机的超时时间(秒)')
    parser.add_argument('--detailed', action='store_true', help='获取电机详细信息')
    parser.add_argument('--monitor', type=float, help='持续监控时长(秒)')
    parser.add_argument('--test', type=int, help='测试指定电机ID的通信可靠性')
    parser.add_argument('--save', type=str, help='保存扫描报告到文件')

    args = parser.parse_args()

    # 创建扫描器
    scanner = LivelyMotorScanner(args.channel, args.bitrate)

    # 连接CAN总线
    if not scanner.connect():
        sys.exit(1)

    try:
        # 测试单个电机
        if args.test:
            scanner.test_motor_communication(args.test)
            return

        # 扫描电机
        found_ids = scanner.scan_range(args.start, args.end, args.timeout)

        print(f"\n{'='*50}")
        if found_ids:
            print(f"✅ 扫描完成，在线电机列表: {sorted(found_ids)}")
            print(f"总计: {len(found_ids)} 个电机")

            # 详细信息
            if args.detailed:
                print(f"\n{'='*50}")
                print("电机详细信息:")
                for motor_id in sorted(found_ids):
                    info = scanner.get_motor_info(motor_id)
                    if info:
                        mode_name = info.get('mode_name', '未知模式')
                        print(f"\n电机 {motor_id}:")
                        print(f"  CAN ID: {info['can_id']}")
                        print(f"  模式: {mode_name} ({info.get('mode', 'N/A')})")
                        print(f"  数据: {info['data']}")

            # 持续监控
            if args.monitor:
                scanner.continuous_monitor(found_ids, args.monitor)

            # 保存报告
            if args.save:
                report = scanner.generate_report(found_ids)
                with open(args.save, 'w', encoding='utf-8') as f:
                    f.write("# 高擎电机CAN扫描报告\n")
                    f.write(f"# 扫描时间: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(report['timestamp']))}\n")
                    f.write(f"# CAN通道: {report['channel']}\n")
                    f.write(f"# 波特率: {report['bitrate']} bps\n")
                    f.write(f"# 扫描范围: {report['scan_range']}\n")
                    f.write(f"\n发现的电机: {report['found_motors']}\n")
                    f.write(f"电机数量: {report['motor_count']}\n")

                    if report['details']:
                        f.write(f"\n详细信息:\n")
                        for motor_id, info in report['details'].items():
                            f.write(f"\n电机{motor_id}:\n")
                            f.write(f"  CAN ID: {info['can_id']}\n")
                            f.write(f"  模式: {info.get('mode_name', '未知')}\n")
                            f.write(f"  数据: {info['data']}\n")

                print(f"\n📄 报告已保存到: {args.save}")

        else:
            print("❌ 未扫描到任何电机")
            print("\n建议:")
            print("1. 检查120Ω终端电阻")
            print("2. 尝试手动转动电机一下（有些电机休眠需要激活）")
            print("3. 检查CAN接口连接")
            print("4. 确认电机供电正常")

    except KeyboardInterrupt:
        print("\n⚠️ 用户中断操作")
    except Exception as e:
        print(f"\n❌ 扫描过程中出错: {e}")
    finally:
        scanner.disconnect()


if __name__ == "__main__":
    main()