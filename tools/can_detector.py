#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
🚗 ESP32控制板CAN盒检测与配置工具

本工具用于检测、配置和测试CAN设备，与ESP32控制板的CAN通信兼容。
支持的功能：
- 自动检测可用CAN接口
- 配置250kbps波特率（匹配ESP32 TWAI配置）
- 发送/接收CAN消息测试
- 实时CAN总线监控
- 模拟LKBLS481502电机控制命令

作者: ESP32控制板项目组
版本: 1.0.0
"""

import can
import time
import sys
import threading
from typing import List, Optional, Dict, Any
import argparse
import json
from datetime import datetime

class CANDetector:
    """CAN设备检测和配置类"""
    
    def __init__(self):
        self.available_interfaces = []
        self.active_bus = None
        self.monitor_thread = None
        self.monitoring = False
        
        # ESP32项目兼容的CAN配置
        self.esp32_config = {
            'bitrate': 250000,  # 250kbps，匹配ESP32 TWAI_TIMING_CONFIG_250KBITS()
            'extended_id': True,  # 扩展帧，匹配ESP32项目
            'motor_base_id': 0x06000001,  # 匹配ESP32项目的CAN ID
        }
        
        # LKBLS481502电机控制命令格式（匹配ESP32项目）
        self.motor_commands = {
            'enable_motor': [0x23, 0x0D, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00],
            'disable_motor': [0x23, 0x0C, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00],
            'speed_command_base': [0x23, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00]
        }
    
    def detect_interfaces(self) -> List[str]:
        """
        检测可用的CAN接口
        
        Returns:
            List[str]: 可用接口列表
        """
        print("🔍 正在检测可用的CAN接口...")
        self.available_interfaces = []
        
        # 常见的CAN接口类型
        interface_types = [
            'socketcan',  # Linux SocketCAN
            'pcan',       # PEAK CAN
            'vector',     # Vector CAN
            'kvaser',     # Kvaser CAN
            'usb2can',    # USB2CAN
            'serial',     # 串口CAN
        ]
        
        for interface_type in interface_types:
            try:
                # 尝试获取该接口类型的可用通道
                if interface_type == 'socketcan':
                    # Linux SocketCAN接口检测
                    try:
                        import subprocess
                        result = subprocess.run(['ip', 'link', 'show'], 
                                              capture_output=True, text=True)
                        if 'can' in result.stdout:
                            lines = result.stdout.split('\n')
                            for line in lines:
                                if 'can' in line and ':' in line:
                                    interface_name = line.split(':')[1].strip().split('@')[0]
                                    if interface_name.startswith('can'):
                                        self.available_interfaces.append(f"{interface_type}:{interface_name}")
                    except:
                        pass
                
                elif interface_type == 'pcan':
                    # PEAK CAN接口检测
                    try:
                        for i in range(1, 9):  # PCAN_USBBUS1 到 PCAN_USBBUS8
                            channel = f"PCAN_USBBUS{i}"
                            self.available_interfaces.append(f"{interface_type}:{channel}")
                    except:
                        pass
                
                elif interface_type == 'vector':
                    # Vector CAN接口检测
                    try:
                        for i in range(0, 4):  # 通常0-3通道
                            channel = f"VN1610_{i}"
                            self.available_interfaces.append(f"{interface_type}:{channel}")
                    except:
                        pass
                
                elif interface_type == 'kvaser':
                    # Kvaser CAN接口检测
                    try:
                        for i in range(0, 8):  # 通常0-7通道
                            channel = f"kvaser:{i}"
                            self.available_interfaces.append(f"{interface_type}:{channel}")
                    except:
                        pass
                
                elif interface_type == 'usb2can':
                    # USB2CAN接口检测
                    try:
                        import serial.tools.list_ports
                        ports = serial.tools.list_ports.comports()
                        for port in ports:
                            if 'USB2CAN' in port.description or 'CAN' in port.description:
                                self.available_interfaces.append(f"{interface_type}:{port.device}")
                    except:
                        pass
                
                elif interface_type == 'serial':
                    # 串口CAN接口检测
                    try:
                        import serial.tools.list_ports
                        ports = serial.tools.list_ports.comports()
                        for port in ports:
                            # 检测可能的串口CAN设备
                            if any(keyword in port.description.upper() for keyword in 
                                  ['CAN', 'SLCAN', 'LAWICEL']):
                                self.available_interfaces.append(f"{interface_type}:{port.device}")
                    except:
                        pass
                        
            except Exception as e:
                print(f"⚠️ 检测{interface_type}接口时出错: {e}")
        
        # 移除重复项
        self.available_interfaces = list(set(self.available_interfaces))
        
        print(f"✅ 检测完成，找到 {len(self.available_interfaces)} 个可用接口:")
        for i, interface in enumerate(self.available_interfaces):
            print(f"   {i+1}. {interface}")
        
        return self.available_interfaces
    
    def connect_interface(self, interface_spec: str) -> bool:
        """
        连接到指定的CAN接口
        
        Args:
            interface_spec (str): 接口规格，格式为 "type:channel"
            
        Returns:
            bool: 连接是否成功
        """
        try:
            interface_type, channel = interface_spec.split(':', 1)
            
            print(f"🔌 正在连接到 {interface_spec}...")
            print(f"   接口类型: {interface_type}")
            print(f"   通道: {channel}")
            print(f"   波特率: {self.esp32_config['bitrate']} bps")
            
            # 创建CAN总线连接
            self.active_bus = can.interface.Bus(
                channel=channel,
                bustype=interface_type,
                bitrate=self.esp32_config['bitrate']
            )
            
            print("✅ CAN接口连接成功！")
            print(f"   总线状态: {self.active_bus.state if hasattr(self.active_bus, 'state') else '未知'}")
            
            return True
            
        except Exception as e:
            print(f"❌ 连接失败: {e}")
            return False
    
    def test_communication(self) -> bool:
        """
        测试CAN通信
        
        Returns:
            bool: 测试是否成功
        """
        if not self.active_bus:
            print("❌ 未连接到CAN接口")
            return False
        
        print("🧪 开始CAN通信测试...")
        
        try:
            # 发送测试消息（电机使能命令）
            test_msg = can.Message(
                arbitration_id=self.esp32_config['motor_base_id'],
                data=self.motor_commands['enable_motor'],
                is_extended_id=self.esp32_config['extended_id']
            )
            
            print(f"📤 发送测试消息:")
            print(f"   ID: 0x{test_msg.arbitration_id:08X}")
            print(f"   数据: {' '.join(f'{b:02X}' for b in test_msg.data)}")
            
            self.active_bus.send(test_msg)
            print("✅ 消息发送成功")
            
            # 尝试接收消息（超时1秒）
            print("👂 等待接收消息...")
            received_msg = self.active_bus.recv(timeout=1.0)
            
            if received_msg:
                print(f"📥 接收到消息:")
                print(f"   ID: 0x{received_msg.arbitration_id:08X}")
                print(f"   数据: {' '.join(f'{b:02X}' for b in received_msg.data)}")
                print(f"   时间戳: {received_msg.timestamp}")
            else:
                print("⏰ 接收超时（这是正常的，如果没有其他设备响应）")
            
            return True
            
        except Exception as e:
            print(f"❌ 通信测试失败: {e}")
            return False
    
    def send_motor_command(self, channel: int, speed: int) -> bool:
        """
        发送电机控制命令（匹配ESP32项目格式）
        
        Args:
            channel (int): 电机通道 (1=左电机, 2=右电机)
            speed (int): 速度值 (-100 到 +100)
            
        Returns:
            bool: 发送是否成功
        """
        if not self.active_bus:
            print("❌ 未连接到CAN接口")
            return False
        
        try:
            # 限制速度范围
            speed = max(-100, min(100, speed))
            
            # 转换为驱动器速度值（匹配ESP32项目：speed * 100）
            driver_speed = speed * 100
            
            # 构建速度命令数据
            cmd_data = self.motor_commands['speed_command_base'].copy()
            cmd_data[3] = channel  # 设置通道
            
            # 32位有符号整数，高字节在前（匹配ESP32项目）
            cmd_data[4] = (driver_speed >> 24) & 0xFF
            cmd_data[5] = (driver_speed >> 16) & 0xFF
            cmd_data[6] = (driver_speed >> 8) & 0xFF
            cmd_data[7] = driver_speed & 0xFF
            
            # 创建CAN消息
            msg = can.Message(
                arbitration_id=self.esp32_config['motor_base_id'],
                data=cmd_data,
                is_extended_id=self.esp32_config['extended_id']
            )
            
            print(f"🚗 发送电机命令:")
            print(f"   通道: {channel} ({'左电机' if channel == 1 else '右电机' if channel == 2 else '未知'})")
            print(f"   速度: {speed} (-100~+100)")
            print(f"   驱动器速度值: {driver_speed}")
            print(f"   CAN ID: 0x{msg.arbitration_id:08X}")
            print(f"   数据: {' '.join(f'{b:02X}' for b in msg.data)}")
            
            self.active_bus.send(msg)
            print("✅ 电机命令发送成功")
            
            return True
            
        except Exception as e:
            print(f"❌ 发送电机命令失败: {e}")
            return False
    
    def start_monitor(self) -> None:
        """开始CAN总线监控"""
        if not self.active_bus:
            print("❌ 未连接到CAN接口")
            return
        
        if self.monitoring:
            print("⚠️ 监控已在运行中")
            return
        
        self.monitoring = True
        self.monitor_thread = threading.Thread(target=self._monitor_worker, daemon=True)
        self.monitor_thread.start()
        print("👁️ CAN总线监控已启动，按Ctrl+C停止...")
    
    def stop_monitor(self) -> None:
        """停止CAN总线监控"""
        self.monitoring = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=1.0)
        print("🛑 CAN总线监控已停止")
    
    def _monitor_worker(self) -> None:
        """CAN监控工作线程"""
        print("📊 CAN总线监控开始...")
        print("格式: [时间] ID:数据 (扩展帧标识)")
        print("-" * 60)
        
        while self.monitoring:
            try:
                msg = self.active_bus.recv(timeout=0.1)
                if msg:
                    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    id_str = f"0x{msg.arbitration_id:08X}" if msg.is_extended_id else f"0x{msg.arbitration_id:03X}"
                    data_str = ' '.join(f'{b:02X}' for b in msg.data)
                    ext_flag = "EXT" if msg.is_extended_id else "STD"
                    
                    print(f"[{timestamp}] {id_str}:{data_str} ({ext_flag})")
                    
            except Exception:
                continue
    
    def disconnect(self) -> None:
        """断开CAN连接"""
        if self.monitoring:
            self.stop_monitor()
        
        if self.active_bus:
            try:
                self.active_bus.shutdown()
                print("🔌 CAN接口已断开")
            except:
                pass
            self.active_bus = None
