#!/usr/bin/env python3
"""
간단한 릴레이 제어 프로그램
- Active Low 릴레이 (LOW=ON, HIGH=OFF)
- 초기 상태를 변경하지 않음
- 간결한 화면 표시
"""

import sys
import time
import os
import signal
import threading
import termios
import tty
from datetime import datetime

try:
    import Jetson.GPIO as GPIO
except ImportError:
    print("Jetson.GPIO 라이브러리가 설치되지 않았습니다.")
    sys.exit(1)


class SimpleRelayControl:
    def __init__(self):
        # 릴레이 핀 정의
        self.RELAY_PINS = {
            1: 40,  # CH1 - Sonoptix Echo
            2: 33,  # CH2 - Ping360  
            3: 37   # CH3 - Livox MID360
        }
        
        self.SENSOR_NAMES = {
            1: "Sonoptix",
            2: "Ping360",
            3: "Livox"
        }
        
        # GPIO 초기화 (현재 상태 유지)
        GPIO.setmode(GPIO.BOARD)
        GPIO.setwarnings(False)
        
        # 모든 핀을 OUT으로 설정 (현재 상태 유지)
        for pin in self.RELAY_PINS.values():
            try:
                # 현재 상태를 읽으려 시도
                GPIO.setup(pin, GPIO.IN)
                current_state = GPIO.input(pin)
                # OUT으로 변경하되 현재 상태 유지
                GPIO.setup(pin, GPIO.OUT, initial=current_state)
            except:
                # 실패하면 OFF 상태로 초기화
                GPIO.setup(pin, GPIO.OUT, initial=GPIO.HIGH)
        
        self.running = True
        self.old_settings = termios.tcgetattr(sys.stdin)
        
        signal.signal(signal.SIGINT, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        self.running = False
        self.cleanup()
        sys.exit(0)
    
    def clear_screen(self):
        os.system('clear')
    
    def is_relay_on(self, channel):
        """릴레이 상태 확인 (LOW=ON for Active Low)"""
        pin = self.RELAY_PINS[channel]
        return GPIO.input(pin) == GPIO.LOW
    
    def relay_on(self, channel):
        """릴레이 ON (LOW for Active Low)"""
        GPIO.output(self.RELAY_PINS[channel], GPIO.LOW)
    
    def relay_off(self, channel):
        """릴레이 OFF (HIGH for Active Low)"""
        GPIO.output(self.RELAY_PINS[channel], GPIO.HIGH)
    
    def all_on(self):
        for ch in self.RELAY_PINS:
            self.relay_on(ch)
    
    def all_off(self):
        for ch in self.RELAY_PINS:
            self.relay_off(ch)
    
    def display(self):
        self.clear_screen()
        print(f"릴레이 제어 [{datetime.now().strftime('%H:%M:%S')}]")
        print("-" * 30)
        
        for ch, name in self.SENSOR_NAMES.items():
            status = "ON " if self.is_relay_on(ch) else "OFF"
            mark = "●" if self.is_relay_on(ch) else "○"
            print(f" {ch}. CH{ch} {mark} {status} {name}")
        
        print("-" * 30)
        print(" Q/W/E: ON   A/S/D: OFF")
        print(" 7: 모두ON   8: 모두OFF")
        print(" 0/ESC: 종료")
    
    def keyboard_handler(self):
        try:
            tty.setraw(sys.stdin.fileno())
            
            while self.running:
                if sys.stdin in __import__('select').select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1).lower()
                    
                    # 종료
                    if key == '\x1b' or key == '0':
                        self.running = False
                        break
                    
                    # ON 제어
                    elif key == 'q':
                        self.relay_on(1)
                    elif key == 'w':
                        self.relay_on(2)
                    elif key == 'e':
                        self.relay_on(3)
                    
                    # OFF 제어
                    elif key == 'a':
                        self.relay_off(1)
                    elif key == 's':
                        self.relay_off(2)
                    elif key == 'd':
                        self.relay_off(3)
                    
                    # 번호로 ON/OFF 토글
                    elif key == '1':
                        if self.is_relay_on(1):
                            self.relay_off(1)
                        else:
                            self.relay_on(1)
                    elif key == '2':
                        if self.is_relay_on(2):
                            self.relay_off(2)
                        else:
                            self.relay_on(2)
                    elif key == '3':
                        if self.is_relay_on(3):
                            self.relay_off(3)
                        else:
                            self.relay_on(3)
                    
                    # 전체 제어
                    elif key == '7':
                        self.all_on()
                    elif key == '8':
                        self.all_off()
                
                time.sleep(0.01)
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)
    
    def run(self):
        # 키보드 스레드
        kbd_thread = threading.Thread(target=self.keyboard_handler)
        kbd_thread.daemon = True
        kbd_thread.start()
        
        # 화면 업데이트
        while self.running:
            self.display()
            time.sleep(0.5)
    
    def cleanup(self):
        self.running = False
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)
        self.clear_screen()
        print("\n릴레이 제어 종료")
        print("-" * 30)
        for ch, name in self.SENSOR_NAMES.items():
            status = "ON " if self.is_relay_on(ch) else "OFF"
            print(f" CH{ch}: {status} - {name}")


def main():
    control = SimpleRelayControl()
    control.run()


if __name__ == "__main__":
    main()