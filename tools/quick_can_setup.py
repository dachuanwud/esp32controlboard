#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
⚡ ESP32控制板CAN快速配置工具

这是一个简化的CAN设备快速配置脚本，专门用于快速设置和测试CAN通信。
适合初次使用或需要快速验证CAN连接的场景。

使用方法:
    python quick_can_setup.py

作者: ESP32控制板项目组
版本: 1.0.0
"""

import can
import time
import sys
from typing import Optional

class QuickCANSetup:
    """快速CAN配置类"""
    
    def __init__(self):
        self.bus: Optional[can.Bus] = None
        
        # ESP32兼容配置
        self.config = {
            'bitrate': 250000,
            'motor_id': 0x06000001,
        }
        
        # 常见CAN接口配置
        self.common_interfaces = [
            # SocketCAN (Linux)
            {'type': 'socketcan', 'channel': 'can0'},
            {'type': 'socketcan', 'channel': 'can1'},
            
            # PEAK CAN
            {'type': 'pcan', 'channel': 'PCAN_USBBUS1'},
            {'type': 'pcan', 'channel': 'PCAN_USBBUS2'},
            
            # Vector CAN
            {'type': 'vector', 'channel': 'VN1610_1'},
            {'type': 'vector', 'channel': 'VN1640_1'},
            
            # Kvaser CAN
            {'type': 'kvaser', 'channel': '0'},
            {'type': 'kvaser', 'channel': '1'},
            
            # 串口CAN (需要根据实际端口修改)
            {'type': 'serial', 'channel': 'COM3'},  # Windows
            {'type': 'serial', 'channel': 'COM4'},
            {'type': 'serial', 'channel': '/dev/ttyUSB0'},  # Linux
            {'type': 'serial', 'channel': '/dev/ttyUSB1'},
        ]
    
    def auto_detect_and_connect(self) -> bool:
        """
        自动检测并连接CAN接口
        
        Returns:
            bool: 连接是否成功
        """
        print("🔍 正在自动检测CAN接口...")
        
        for interface in self.common_interfaces:
            try:
                print(f"   尝试连接: {interface['type']}:{interface['channel']}")
                
                self.bus = can.interface.Bus(
                    channel=interface['channel'],
                    bustype=interface['type'],
                    bitrate=self.config['bitrate']
                )
                
                # 测试连接
                test_msg = can.Message(
                    arbitration_id=0x123,
                    data=[0x01, 0x02, 0x03, 0x04],
                    is_extended_id=False
                )
                
                self.bus.send(test_msg, timeout=0.1)
                
                print(f"✅ 成功连接到: {interface['type']}:{interface['channel']}")
                print(f"   波特率: {self.config['bitrate']} bps")
                return True
                
            except Exception as e:
                if self.bus:
                    try:
                        self.bus.shutdown()
                    except:
                        pass
                    self.bus = None
                continue
        
        print("❌ 未找到可用的CAN接口")
        return False
    
    def send_motor_enable(self, channel: int = 1) -> bool:
        """
        发送电机使能命令
        
        Args:
            channel (int): 电机通道 (1或2)
            
        Returns:
            bool: 发送是否成功
        """
        if not self.bus:
            print("❌ CAN接口未连接")
            return False
        
        try:
            # 电机使能命令 (匹配ESP32项目格式)
            enable_data = [0x23, 0x0D, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
            
            msg = can.Message(
                arbitration_id=self.config['motor_id'],
                data=enable_data,
                is_extended_id=True
            )
            
            self.bus.send(msg)
            print(f"✅ 电机{channel}使能命令已发送")
            return True
            
        except Exception as e:
            print(f"❌ 发送失败: {e}")
            return False
    
    def send_motor_speed(self, channel: int, speed: int) -> bool:
        """
        发送电机速度命令
        
        Args:
            channel (int): 电机通道 (1或2)
            speed (int): 速度值 (-100到+100)
            
        Returns:
            bool: 发送是否成功
        """
        if not self.bus:
            print("❌ CAN接口未连接")
            return False
        
        try:
            # 限制速度范围
            speed = max(-100, min(100, speed))
            
            # 转换为驱动器速度值 (匹配ESP32项目: speed * 100)
            driver_speed = speed * 100
            
            # 构建速度命令
            speed_data = [0x23, 0x00, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
            
            # 32位有符号整数，高字节在前
            speed_data[4] = (driver_speed >> 24) & 0xFF
            speed_data[5] = (driver_speed >> 16) & 0xFF
            speed_data[6] = (driver_speed >> 8) & 0xFF
            speed_data[7] = driver_speed & 0xFF
            
            msg = can.Message(
                arbitration_id=self.config['motor_id'],
                data=speed_data,
                is_extended_id=True
            )
            
            self.bus.send(msg)
            print(f"✅ 电机{channel}速度设置为{speed} (驱动器值:{driver_speed})")
            return True
            
        except Exception as e:
            print(f"❌ 发送失败: {e}")
            return False
    
    def quick_test(self) -> None:
        """快速测试序列"""
        print("\n🧪 开始快速测试序列...")
        
        # 1. 使能电机
        print("\n1️⃣ 使能电机...")
        self.send_motor_enable(1)  # 左电机
        time.sleep(0.1)
        self.send_motor_enable(2)  # 右电机
        time.sleep(1)
        
        # 2. 测试动作序列
        test_actions = [
            ("前进", 1, 30, 2, 30),
            ("停止", 0, 0, 1, 0),
            ("后退", -1, -30, 2, -30),
            ("停止", 0, 0, 1, 0),
            ("左转", -1, -20, 2, 20),
            ("右转", 1, 20, 2, -20),
            ("停止", 0, 0, 1, 0),
        ]
        
        for i, (action, left_speed, right_speed, duration, _) in enumerate(test_actions, 2):
            print(f"\n{i}️⃣ {action} (持续{duration}秒)")
            self.send_motor_speed(1, left_speed)
            time.sleep(0.1)
            self.send_motor_speed(2, right_speed)
            time.sleep(duration)
        
        print("\n✅ 快速测试完成！")
    
    def monitor_messages(self, duration: int = 10) -> None:
        """
        监控CAN消息
        
        Args:
            duration (int): 监控时长（秒）
        """
        if not self.bus:
            print("❌ CAN接口未连接")
            return
        
        print(f"\n👁️ 开始监控CAN消息 (持续{duration}秒)...")
        print("格式: [时间] ID:数据")
        print("-" * 50)
        
        start_time = time.time()
        message_count = 0
        
        while time.time() - start_time < duration:
            try:
                msg = self.bus.recv(timeout=0.1)
                if msg:
                    timestamp = time.strftime("%H:%M:%S", time.localtime(msg.timestamp))
                    id_str = f"0x{msg.arbitration_id:08X}" if msg.is_extended_id else f"0x{msg.arbitration_id:03X}"
                    data_str = ' '.join(f'{b:02X}' for b in msg.data)
                    print(f"[{timestamp}] {id_str}:{data_str}")
                    message_count += 1
            except:
                continue
        
        print(f"\n📊 监控结束，共接收到 {message_count} 条消息")
    
    def disconnect(self) -> None:
        """断开CAN连接"""
        if self.bus:
            try:
                self.bus.shutdown()
                print("🔌 CAN接口已断开")
            except:
                pass
            self.bus = None

def main():
    """主函数"""
    print("=" * 60)
    print("⚡ ESP32控制板CAN快速配置工具 v1.0.0")
    print("=" * 60)
    print("🎯 功能: 快速检测、连接和测试CAN通信")
    print("🔧 配置: 250kbps波特率，兼容ESP32 TWAI")
    print("🚗 目标: LKBLS481502电机驱动器")
    print("=" * 60)
    
    setup = QuickCANSetup()
    
    try:
        # 自动检测并连接
        if not setup.auto_detect_and_connect():
            print("\n💡 请检查:")
            print("   • CAN设备是否正确连接")
            print("   • 驱动程序是否已安装")
            print("   • 设备权限是否正确")
            return
        
        while True:
            print("\n🎯 请选择操作:")
            print("1. 🚗 快速电机测试")
            print("2. 🔧 手动电机控制")
            print("3. 👁️ 监控CAN消息")
            print("4. 📊 显示当前配置")
            print("5. ❌ 退出")
            
            choice = input("\n请输入选择 (1-5): ").strip()
            
            if choice == '1':
                setup.quick_test()
            
            elif choice == '2':
                print("\n🔧 手动电机控制模式")
                try:
                    channel = int(input("请输入电机通道 (1=左电机, 2=右电机): "))
                    if channel not in [1, 2]:
                        print("❌ 无效的电机通道")
                        continue
                    
                    # 先使能电机
                    setup.send_motor_enable(channel)
                    time.sleep(0.5)
                    
                    speed = int(input("请输入速度值 (-100 到 +100): "))
                    setup.send_motor_speed(channel, speed)
                    
                except ValueError:
                    print("❌ 请输入有效的数字")
            
            elif choice == '3':
                try:
                    duration = int(input("请输入监控时长（秒，默认10秒）: ") or "10")
                    setup.monitor_messages(duration)
                except ValueError:
                    setup.monitor_messages(10)
            
            elif choice == '4':
                print(f"\n📊 当前配置:")
                print(f"   连接状态: {'已连接' if setup.bus else '未连接'}")
                print(f"   波特率: {setup.config['bitrate']} bps")
                print(f"   电机CAN ID: 0x{setup.config['motor_id']:08X}")
                if setup.bus:
                    try:
                        state = getattr(setup.bus, 'state', '未知')
                        print(f"   总线状态: {state}")
                    except:
                        pass
            
            elif choice == '5':
                print("\n👋 正在退出...")
                break
            
            else:
                print("❌ 无效的选择")
    
    except KeyboardInterrupt:
        print("\n\n🛑 用户中断")
    except Exception as e:
        print(f"\n❌ 程序异常: {e}")
    finally:
        setup.disconnect()

if __name__ == "__main__":
    main()
