#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
🛠️ ESP32控制板CAN工具主程序

这是一个交互式的CAN设备检测、配置和测试工具，专为ESP32控制板项目设计。
支持与LKBLS481502电机驱动器的CAN通信测试。

使用方法:
    python can_tool.py                    # 交互式模式
    python can_tool.py --detect           # 仅检测接口
    python can_tool.py --monitor          # 监控模式
    python can_tool.py --test-motor       # 电机测试模式

作者: ESP32控制板项目组
版本: 1.0.0
"""

import sys
import time
import argparse
from can_detector import CANDetector

def print_banner():
    """打印程序横幅"""
    print("=" * 70)
    print("🚗 ESP32控制板CAN盒检测与配置工具 v1.0.0")
    print("=" * 70)
    print("📋 功能特性:")
    print("   • 自动检测CAN接口设备")
    print("   • 配置250kbps波特率（匹配ESP32 TWAI）")
    print("   • 发送/接收CAN消息测试")
    print("   • 实时CAN总线监控")
    print("   • LKBLS481502电机控制命令模拟")
    print("=" * 70)

def interactive_mode():
    """交互式模式主函数"""
    detector = CANDetector()
    
    try:
        while True:
            print("\n🎯 请选择操作:")
            print("1. 🔍 检测CAN接口")
            print("2. 🔌 连接CAN接口")
            print("3. 🧪 测试CAN通信")
            print("4. 🚗 发送电机控制命令")
            print("5. 👁️ 启动CAN总线监控")
            print("6. 🛑 停止CAN总线监控")
            print("7. 📊 显示当前状态")
            print("8. 🔌 断开连接")
            print("9. ❌ 退出程序")
            
            choice = input("\n请输入选择 (1-9): ").strip()
            
            if choice == '1':
                # 检测CAN接口
                interfaces = detector.detect_interfaces()
                if not interfaces:
                    print("❌ 未找到可用的CAN接口")
                    print("💡 请检查:")
                    print("   • CAN设备是否正确连接")
                    print("   • 驱动程序是否已安装")
                    print("   • 设备权限是否正确")
            
            elif choice == '2':
                # 连接CAN接口
                if not detector.available_interfaces:
                    print("⚠️ 请先检测CAN接口")
                    continue
                
                print("\n可用接口:")
                for i, interface in enumerate(detector.available_interfaces):
                    print(f"   {i+1}. {interface}")
                
                try:
                    idx = int(input("请选择接口编号: ")) - 1
                    if 0 <= idx < len(detector.available_interfaces):
                        interface = detector.available_interfaces[idx]
                        detector.connect_interface(interface)
                    else:
                        print("❌ 无效的接口编号")
                except ValueError:
                    print("❌ 请输入有效的数字")
            
            elif choice == '3':
                # 测试CAN通信
                detector.test_communication()
            
            elif choice == '4':
                # 发送电机控制命令
                if not detector.active_bus:
                    print("❌ 请先连接CAN接口")
                    continue
                
                print("\n🚗 电机控制命令:")
                print("1. 使能电机")
                print("2. 禁用电机")
                print("3. 设置电机速度")
                print("4. 差速控制测试")
                
                motor_choice = input("请选择命令类型 (1-4): ").strip()
                
                if motor_choice == '1':
                    # 使能电机
                    try:
                        channel = int(input("请输入电机通道 (1=左电机, 2=右电机): "))
                        if channel in [1, 2]:
                            # 发送使能命令
                            import can
                            enable_cmd = [0x23, 0x0D, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
                            msg = can.Message(
                                arbitration_id=detector.esp32_config['motor_base_id'],
                                data=enable_cmd,
                                is_extended_id=True
                            )
                            detector.active_bus.send(msg)
                            print(f"✅ 电机{channel}使能命令已发送")
                        else:
                            print("❌ 无效的电机通道")
                    except ValueError:
                        print("❌ 请输入有效的数字")
                
                elif motor_choice == '2':
                    # 禁用电机
                    try:
                        channel = int(input("请输入电机通道 (1=左电机, 2=右电机): "))
                        if channel in [1, 2]:
                            # 发送禁用命令
                            import can
                            disable_cmd = [0x23, 0x0C, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
                            msg = can.Message(
                                arbitration_id=detector.esp32_config['motor_base_id'],
                                data=disable_cmd,
                                is_extended_id=True
                            )
                            detector.active_bus.send(msg)
                            print(f"✅ 电机{channel}禁用命令已发送")
                        else:
                            print("❌ 无效的电机通道")
                    except ValueError:
                        print("❌ 请输入有效的数字")
                
                elif motor_choice == '3':
                    # 设置电机速度
                    try:
                        channel = int(input("请输入电机通道 (1=左电机, 2=右电机): "))
                        speed = int(input("请输入速度值 (-100 到 +100): "))
                        
                        if channel in [1, 2] and -100 <= speed <= 100:
                            detector.send_motor_command(channel, speed)
                        else:
                            print("❌ 无效的参数")
                    except ValueError:
                        print("❌ 请输入有效的数字")
                
                elif motor_choice == '4':
                    # 差速控制测试
                    print("\n🎮 差速控制测试模式")
                    print("将模拟ESP32项目的差速控制逻辑")
                    
                    test_sequences = [
                        ("停止", 0, 0),
                        ("前进", 50, 50),
                        ("后退", -50, -50),
                        ("左转", -30, 30),
                        ("右转", 30, -30),
                        ("停止", 0, 0)
                    ]
                    
                    for action, left_speed, right_speed in test_sequences:
                        print(f"\n🎯 执行动作: {action}")
                        print(f"   左电机速度: {left_speed}")
                        print(f"   右电机速度: {right_speed}")
                        
                        detector.send_motor_command(1, left_speed)   # 左电机
                        time.sleep(0.1)
                        detector.send_motor_command(2, right_speed)  # 右电机
                        
                        time.sleep(2)  # 每个动作持续2秒
                    
                    print("✅ 差速控制测试完成")
            
            elif choice == '5':
                # 启动CAN总线监控
                detector.start_monitor()
            
            elif choice == '6':
                # 停止CAN总线监控
                detector.stop_monitor()
            
            elif choice == '7':
                # 显示当前状态
                print("\n📊 当前状态:")
                print(f"   可用接口数量: {len(detector.available_interfaces)}")
                print(f"   连接状态: {'已连接' if detector.active_bus else '未连接'}")
                print(f"   监控状态: {'运行中' if detector.monitoring else '已停止'}")
                print(f"   配置波特率: {detector.esp32_config['bitrate']} bps")
                print(f"   CAN ID: 0x{detector.esp32_config['motor_base_id']:08X}")
                
                if detector.active_bus:
                    try:
                        state = getattr(detector.active_bus, 'state', '未知')
                        print(f"   总线状态: {state}")
                    except:
                        pass
            
            elif choice == '8':
                # 断开连接
                detector.disconnect()
            
            elif choice == '9':
                # 退出程序
                print("👋 正在退出...")
                detector.disconnect()
                break
            
            else:
                print("❌ 无效的选择，请重新输入")
    
    except KeyboardInterrupt:
        print("\n\n🛑 用户中断，正在退出...")
        detector.disconnect()
    except Exception as e:
        print(f"\n❌ 程序异常: {e}")
        detector.disconnect()

def detect_only_mode():
    """仅检测模式"""
    print("🔍 CAN接口检测模式")
    detector = CANDetector()
    interfaces = detector.detect_interfaces()
    
    if interfaces:
        print(f"\n✅ 检测结果: 找到 {len(interfaces)} 个可用接口")
        for i, interface in enumerate(interfaces):
            print(f"   {i+1}. {interface}")
    else:
        print("\n❌ 未找到可用的CAN接口")
        print("💡 请检查CAN设备连接和驱动程序")

def monitor_mode():
    """监控模式"""
    print("👁️ CAN总线监控模式")
    detector = CANDetector()
    
    # 自动检测并连接第一个可用接口
    interfaces = detector.detect_interfaces()
    if not interfaces:
        print("❌ 未找到可用的CAN接口")
        return
    
    print(f"🔌 自动连接到: {interfaces[0]}")
    if detector.connect_interface(interfaces[0]):
        detector.start_monitor()
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n🛑 监控停止")
            detector.disconnect()

def test_motor_mode():
    """电机测试模式"""
    print("🚗 电机测试模式")
    detector = CANDetector()
    
    # 自动检测并连接第一个可用接口
    interfaces = detector.detect_interfaces()
    if not interfaces:
        print("❌ 未找到可用的CAN接口")
        return
    
    print(f"🔌 自动连接到: {interfaces[0]}")
    if detector.connect_interface(interfaces[0]):
        print("🧪 开始电机测试序列...")
        
        # 使能电机
        print("1. 使能电机...")
        import can
        for channel in [1, 2]:
            enable_cmd = [0x23, 0x0D, 0x20, channel, 0x00, 0x00, 0x00, 0x00]
            msg = can.Message(
                arbitration_id=detector.esp32_config['motor_base_id'],
                data=enable_cmd,
                is_extended_id=True
            )
            detector.active_bus.send(msg)
            time.sleep(0.1)
        
        # 测试序列
        test_sequences = [
            ("前进", 30, 30, 3),
            ("停止", 0, 0, 1),
            ("后退", -30, -30, 3),
            ("停止", 0, 0, 1),
            ("左转", -20, 20, 2),
            ("右转", 20, -20, 2),
            ("停止", 0, 0, 1)
        ]
        
        for action, left_speed, right_speed, duration in test_sequences:
            print(f"2. 执行: {action} (持续{duration}秒)")
            detector.send_motor_command(1, left_speed)
            time.sleep(0.1)
            detector.send_motor_command(2, right_speed)
            time.sleep(duration)
        
        print("✅ 电机测试完成")
        detector.disconnect()

def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="ESP32控制板CAN盒检测与配置工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  python can_tool.py                    # 交互式模式
  python can_tool.py --detect           # 仅检测接口
  python can_tool.py --monitor          # 监控模式
  python can_tool.py --test-motor       # 电机测试模式
        """
    )
    
    parser.add_argument('--detect', action='store_true', 
                       help='仅检测可用的CAN接口')
    parser.add_argument('--monitor', action='store_true',
                       help='启动CAN总线监控模式')
    parser.add_argument('--test-motor', action='store_true',
                       help='启动电机测试模式')
    
    args = parser.parse_args()
    
    print_banner()
    
    if args.detect:
        detect_only_mode()
    elif args.monitor:
        monitor_mode()
    elif args.test_motor:
        test_motor_mode()
    else:
        interactive_mode()

if __name__ == "__main__":
    main()
