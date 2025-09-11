#!/usr/bin/env python3
"""
ROS2 릴레이 제어 노드 V2 - 더 강력한 종료 처리
센서 launch 시 자동으로 릴레이를 켜고, 종료 시 자동으로 끄는 기능
- CH1 (Pin 40): Sonoptix Echo 
- CH2 (Pin 33): Ping360
- CH3 (Pin 37): Livox MID360

주의: 이 릴레이 보드는 LOW=ON, HIGH=OFF (Active Low)
"""

import os
import sys
import signal
import atexit
import threading
import time

try:
    import Jetson.GPIO as GPIO
except ImportError:
    print("Jetson.GPIO 라이브러리가 설치되지 않았습니다.")
    sys.exit(1)


class RelayController:
    """간단한 릴레이 제어 (ROS2 없이)"""
    
    def __init__(self, channel, sensor_name):
        self.channel = channel
        self.sensor_name = sensor_name
        
        # 릴레이 핀 정의
        self.RELAY_PINS = {
            1: 40,  # CH1 - Sonoptix Echo
            2: 33,  # CH2 - Ping360
            3: 37   # CH3 - Livox MID360
        }
        
        if self.channel not in self.RELAY_PINS:
            print(f"잘못된 릴레이 채널: {self.channel}")
            sys.exit(1)
        
        self.relay_pin = self.RELAY_PINS[self.channel]
        self.cleaned_up = False
        self.running = True
        
        # GPIO 초기화
        GPIO.setmode(GPIO.BOARD)
        GPIO.setwarnings(False)
        
        try:
            GPIO.setup(self.relay_pin, GPIO.OUT, initial=GPIO.HIGH)  # 초기값 HIGH (OFF)
        except:
            GPIO.cleanup(self.relay_pin)
            GPIO.setup(self.relay_pin, GPIO.OUT, initial=GPIO.HIGH)
        
        # 릴레이 ON (LOW = ON for Active Low relay)
        GPIO.output(self.relay_pin, GPIO.LOW)
        print(f"✅ 릴레이 CH{self.channel} (Pin {self.relay_pin}) ON - {self.sensor_name}")
        
        # 다양한 종료 핸들러 등록
        atexit.register(self.cleanup)
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
        signal.signal(signal.SIGKILL, self._signal_handler) if hasattr(signal, 'SIGKILL') else None
    
    def _signal_handler(self, signum, frame):
        """시그널 핸들러"""
        print(f"\n시그널 {signum} 수신")
        self.cleanup()
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
            self.running = False
        except Exception as e:
            print(f"릴레이 OFF 실패: {e}")
    
    def run(self):
        """메인 루프"""
        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nKeyboardInterrupt")
        finally:
            self.cleanup()


def main():
    # 명령행 인자 파싱 (ROS2 스타일)
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--ros-args', action='store_true')
    parser.add_argument('-p', action='append', default=[])
    args, unknown = parser.parse_known_args()
    
    # 파라미터 추출
    channel = 1
    sensor_name = 'unknown'
    
    for param in args.p:
        if param.startswith('channel:='):
            channel = int(param.split(':=')[1])
        elif param.startswith('sensor_name:='):
            sensor_name = param.split(':=')[1]
    
    # 릴레이 제어 실행
    controller = RelayController(channel, sensor_name)
    controller.run()


if __name__ == '__main__':
    main()