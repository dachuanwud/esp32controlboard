#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
🧪 CAN工具测试脚本

用于测试CAN工具的基本功能，验证代码正确性。
不需要实际的CAN硬件，使用虚拟接口进行测试。

使用方法:
    python test_can_tools.py

作者: ESP32控制板项目组
版本: 1.0.0
"""

import sys
import time
import unittest
from unittest.mock import Mock, patch, MagicMock

# 导入要测试的模块
try:
    from can_detector import CANDetector
    from quick_can_setup import QuickCANSetup
except ImportError as e:
    print(f"❌ 导入错误: {e}")
    print("💡 请确保在tools目录下运行此脚本")
    sys.exit(1)

class TestCANDetector(unittest.TestCase):
    """CANDetector类测试"""
    
    def setUp(self):
        """测试前准备"""
        self.detector = CANDetector()
    
    def test_init(self):
        """测试初始化"""
        self.assertIsNotNone(self.detector)
        self.assertEqual(self.detector.esp32_config['bitrate'], 250000)
        self.assertEqual(self.detector.esp32_config['motor_base_id'], 0x06000001)
        self.assertTrue(self.detector.esp32_config['extended_id'])
    
    def test_motor_commands(self):
        """测试电机命令格式"""
        # 测试使能命令
        enable_cmd = self.detector.motor_commands['enable_motor']
        self.assertEqual(len(enable_cmd), 8)
        self.assertEqual(enable_cmd[0:3], [0x23, 0x0D, 0x20])
        
        # 测试禁用命令
        disable_cmd = self.detector.motor_commands['disable_motor']
        self.assertEqual(len(disable_cmd), 8)
        self.assertEqual(disable_cmd[0:3], [0x23, 0x0C, 0x20])
        
        # 测试速度命令基础格式
        speed_cmd = self.detector.motor_commands['speed_command_base']
        self.assertEqual(len(speed_cmd), 8)
        self.assertEqual(speed_cmd[0:3], [0x23, 0x00, 0x20])
    
    @patch('can.interface.Bus')
    def test_connect_interface(self, mock_bus):
        """测试接口连接"""
        # 模拟成功连接
        mock_bus_instance = Mock()
        mock_bus.return_value = mock_bus_instance
        
        result = self.detector.connect_interface("socketcan:can0")
        
        self.assertTrue(result)
        self.assertIsNotNone(self.detector.active_bus)
        mock_bus.assert_called_once()
    
    @patch('can.interface.Bus')
    def test_connect_interface_failure(self, mock_bus):
        """测试接口连接失败"""
        # 模拟连接失败
        mock_bus.side_effect = Exception("Connection failed")
        
        result = self.detector.connect_interface("invalid:interface")
        
        self.assertFalse(result)
        self.assertIsNone(self.detector.active_bus)

class TestQuickCANSetup(unittest.TestCase):
    """QuickCANSetup类测试"""
    
    def setUp(self):
        """测试前准备"""
        self.setup = QuickCANSetup()
    
    def test_init(self):
        """测试初始化"""
        self.assertIsNotNone(self.setup)
        self.assertEqual(self.setup.config['bitrate'], 250000)
        self.assertEqual(self.setup.config['motor_id'], 0x06000001)
        self.assertIsNone(self.setup.bus)
    
    def test_common_interfaces(self):
        """测试常见接口配置"""
        interfaces = self.setup.common_interfaces
        self.assertGreater(len(interfaces), 0)
        
        # 检查接口格式
        for interface in interfaces:
            self.assertIn('type', interface)
            self.assertIn('channel', interface)
            self.assertIsInstance(interface['type'], str)
            self.assertIsInstance(interface['channel'], str)

class TestIntegration(unittest.TestCase):
    """集成测试"""
    
    @patch('can.interface.Bus')
    @patch('can.Message')
    def test_motor_command_integration(self, mock_message, mock_bus):
        """测试电机命令集成"""
        # 设置模拟对象
        mock_bus_instance = Mock()
        mock_bus.return_value = mock_bus_instance
        mock_message_instance = Mock()
        mock_message.return_value = mock_message_instance
        
        # 创建检测器并连接
        detector = CANDetector()
        detector.connect_interface("test:interface")
        
        # 发送电机命令
        result = detector.send_motor_command(1, 50)
        
        # 验证结果
        self.assertTrue(result)
        mock_bus_instance.send.assert_called_once()
        mock_message.assert_called_once()
        
        # 验证消息参数
        call_args = mock_message.call_args
        self.assertEqual(call_args[1]['arbitration_id'], 0x06000001)
        self.assertTrue(call_args[1]['is_extended_id'])
        self.assertEqual(len(call_args[1]['data']), 8)

def run_functional_tests():
    """运行功能测试（不需要硬件）"""
    print("🧪 开始功能测试...")
    
    # 测试1: 检测器初始化
    print("1️⃣ 测试CANDetector初始化...")
    detector = CANDetector()
    assert detector.esp32_config['bitrate'] == 250000
    print("   ✅ 初始化测试通过")
    
    # 测试2: 接口检测（模拟）
    print("2️⃣ 测试接口检测...")
    interfaces = detector.detect_interfaces()
    print(f"   📊 检测到 {len(interfaces)} 个接口（可能为0，这是正常的）")
    print("   ✅ 检测测试通过")
    
    # 测试3: 快速设置初始化
    print("3️⃣ 测试QuickCANSetup初始化...")
    setup = QuickCANSetup()
    assert setup.config['bitrate'] == 250000
    assert setup.config['motor_id'] == 0x06000001
    print("   ✅ 快速设置测试通过")
    
    # 测试4: 命令格式验证
    print("4️⃣ 测试命令格式...")
    enable_cmd = detector.motor_commands['enable_motor']
    assert len(enable_cmd) == 8
    assert enable_cmd[0:3] == [0x23, 0x0D, 0x20]
    print("   ✅ 命令格式测试通过")
    
    print("✅ 所有功能测试通过！")

def run_mock_hardware_test():
    """运行模拟硬件测试"""
    print("\n🔧 开始模拟硬件测试...")
    
    with patch('can.interface.Bus') as mock_bus:
        # 设置模拟总线
        mock_bus_instance = Mock()
        mock_bus.return_value = mock_bus_instance
        
        # 测试连接
        print("1️⃣ 测试模拟连接...")
        detector = CANDetector()
        result = detector.connect_interface("mock:test")
        assert result == True
        print("   ✅ 模拟连接成功")
        
        # 测试发送命令
        print("2️⃣ 测试模拟发送...")
        with patch('can.Message') as mock_message:
            mock_message_instance = Mock()
            mock_message.return_value = mock_message_instance
            
            result = detector.send_motor_command(1, 50)
            assert result == True
            mock_bus_instance.send.assert_called_once()
            print("   ✅ 模拟发送成功")
        
        # 测试断开连接
        print("3️⃣ 测试模拟断开...")
        detector.disconnect()
        print("   ✅ 模拟断开成功")
    
    print("✅ 所有模拟硬件测试通过！")

def main():
    """主测试函数"""
    print("=" * 60)
    print("🧪 ESP32控制板CAN工具测试套件")
    print("=" * 60)
    print("📋 测试内容:")
    print("   • 基本功能测试")
    print("   • 单元测试")
    print("   • 模拟硬件测试")
    print("   • 集成测试")
    print("=" * 60)
    
    try:
        # 运行功能测试
        run_functional_tests()
        
        # 运行模拟硬件测试
        run_mock_hardware_test()
        
        # 运行单元测试
        print("\n🔬 开始单元测试...")
        unittest.main(argv=[''], exit=False, verbosity=2)
        
        print("\n" + "=" * 60)
        print("🎉 所有测试完成！")
        print("💡 如果所有测试都通过，说明CAN工具代码正常")
        print("🔌 接下来可以连接实际的CAN硬件进行测试")
        print("=" * 60)
        
    except Exception as e:
        print(f"\n❌ 测试过程中出现错误: {e}")
        print("💡 请检查代码或依赖包是否正确安装")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
