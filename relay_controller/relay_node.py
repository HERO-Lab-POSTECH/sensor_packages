#!/usr/bin/env python3
"""
ROS2 릴레이 제어 노드
센서 launch 시 자동으로 릴레이를 켜고, 종료 시 자동으로 끄는 기능
- CH1 (Pin 40): Sonoptix Echo 
- CH2 (Pin 33): Ping360
- CH3 (Pin 37): Livox MID360

주의: 이 릴레이 보드는 LOW=ON, HIGH=OFF (Active Low)
"""

import sys
import signal
import atexit
import rclpy
from rclpy.node import Node

try:
    import Jetson.GPIO as GPIO
except ImportError:
    print("Jetson.GPIO 라이브러리가 설치되지 않았습니다.")
    print("설치 명령: sudo pip3 install Jetson.GPIO")
    sys.exit(1)


class RelayControllerNode(Node):
    """ROS2 릴레이 제어 노드"""
    
    def __init__(self):
        super().__init__('relay_controller')
        
        # 파라미터 선언 및 가져오기
        self.declare_parameter('channel', 1)
        self.declare_parameter('sensor_name', 'unknown')
        
        self.channel = self.get_parameter('channel').value
        self.sensor_name = self.get_parameter('sensor_name').value
        
        # 릴레이 핀 정의
        self.RELAY_PINS = {
            1: 40,  # CH1 - Sonoptix Echo
            2: 33,  # CH2 - Ping360
            3: 37   # CH3 - Livox MID360
        }
        
        if self.channel not in self.RELAY_PINS:
            self.get_logger().error(f"잘못된 릴레이 채널: {self.channel}")
            sys.exit(1)
        
        self.relay_pin = self.RELAY_PINS[self.channel]
        self.cleaned_up = False
        
        # GPIO 초기화
        GPIO.setmode(GPIO.BOARD)
        GPIO.setwarnings(False)
        
        try:
            GPIO.setup(self.relay_pin, GPIO.OUT, initial=GPIO.HIGH)  # 초기값 HIGH (OFF)
        except:
            # 이미 사용 중이면 정리 후 재설정
            GPIO.cleanup(self.relay_pin)
            GPIO.setup(self.relay_pin, GPIO.OUT, initial=GPIO.HIGH)
        
        # 릴레이 ON (LOW = ON for Active Low relay)
        GPIO.output(self.relay_pin, GPIO.LOW)
        self.get_logger().info(f"✅ 릴레이 CH{self.channel} (Pin {self.relay_pin}) ON - {self.sensor_name}")
        
        # 종료 핸들러 등록 (여러 방법으로 등록하여 확실하게 처리)
        atexit.register(self.cleanup)
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """시그널 핸들러 - 종료 시 릴레이 OFF"""
        self.cleanup()
        # 즉시 종료
        import os
        os._exit(0)
    
    def cleanup(self):
        """GPIO 정리 및 릴레이 OFF"""
        if self.cleaned_up:
            return
        
        try:
            # 릴레이 OFF (HIGH = OFF for Active Low relay)
            GPIO.output(self.relay_pin, GPIO.HIGH)
            print(f"❌ 릴레이 CH{self.channel} (Pin {self.relay_pin}) OFF - {self.sensor_name}")
            self.cleaned_up = True
        except Exception as e:
            print(f"릴레이 OFF 실패: {e}")


def main(args=None):
    node = None
    try:
        rclpy.init(args=args)
        node = RelayControllerNode()
        rclpy.spin(node)
    except KeyboardInterrupt:
        print("\nCtrl+C 수신")
    except Exception as e:
        print(f"오류 발생: {e}")
    finally:
        # 정리
        if node:
            node.cleanup()
            try:
                node.destroy_node()
            except:
                pass
        try:
            if rclpy.ok():
                rclpy.shutdown()
        except:
            pass


if __name__ == '__main__':
    main()