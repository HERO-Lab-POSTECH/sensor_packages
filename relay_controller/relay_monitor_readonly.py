#!/usr/bin/env python3
"""
릴레이 상태 읽기 전용 모니터링
- GPIO가 이미 사용 중일 때 사용
- 상태만 읽고 제어는 불가
"""

import sys
import time
import os
from datetime import datetime

try:
    import Jetson.GPIO as GPIO
except ImportError:
    print("Jetson.GPIO 라이브러리가 설치되지 않았습니다.")
    sys.exit(1)


class RelayMonitorReadOnly:
    """읽기 전용 릴레이 모니터링"""
    
    def __init__(self):
        # 릴레이 핀 정의
        self.RELAY_PINS = {
            1: 40,  # CH1 - Sonoptix Echo
            2: 33,  # CH2 - Ping360  
            3: 37   # CH3 - Livox MID360
        }
        
        self.SENSOR_NAMES = {
            1: "Sonoptix Echo",
            2: "Ping360",
            3: "Livox MID360"
        }
        
        # GPIO 초기화 (입력 모드로만)
        GPIO.setmode(GPIO.BOARD)
        GPIO.setwarnings(False)
        
        for pin in self.RELAY_PINS.values():
            try:
                GPIO.setup(pin, GPIO.IN)
            except:
                pass  # 이미 사용 중이어도 계속
        
        self.running = True
    
    def clear_screen(self):
        """화면 클리어"""
        os.system('clear')
    
    def get_relay_status(self, channel):
        """릴레이 상태 확인"""
        pin = self.RELAY_PINS[channel]
        try:
            return "ON " if GPIO.input(pin) else "OFF"
        except:
            return "???"
    
    def display_status(self):
        """상태 화면 표시"""
        self.clear_screen()
        
        print("[ 릴레이 모니터링 - 읽기 전용 ]", datetime.now().strftime("%H:%M:%S"))
        print("-" * 45)
        
        for ch, name in self.SENSOR_NAMES.items():
            status = self.get_relay_status(ch)
            pin = self.RELAY_PINS[ch]
            mark = "●" if status == "ON " else "○" if status == "OFF" else "?"
            print(f" CH{ch}(P{pin:2d}) {mark} {status} {name}")
        
        print("-" * 45)
        print(" * 읽기 전용 모드 (다른 프로세스가 GPIO 사용 중)")
        print(" * Ctrl+C로 종료")
        print("-" * 45)
    
    def run(self):
        """메인 실행"""
        try:
            while self.running:
                self.display_status()
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\n모니터링 종료")
        finally:
            # GPIO는 정리하지 않음 (다른 프로세스가 사용 중)
            pass


def main():
    print("읽기 전용 릴레이 모니터링을 시작합니다...")
    time.sleep(1)
    
    monitor = RelayMonitorReadOnly()
    monitor.run()


if __name__ == "__main__":
    main()