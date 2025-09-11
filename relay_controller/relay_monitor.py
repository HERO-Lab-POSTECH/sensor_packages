#!/usr/bin/env python3
"""
릴레이 상태 모니터링 및 키보드 제어 프로그램
- 실시간 릴레이 상태 표시
- 키보드 입력으로 릴레이 제어
- 주기적으로 화면 클리어하여 깔끔한 모니터링
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
    print("설치 명령: sudo pip3 install Jetson.GPIO")
    sys.exit(1)


class RelayMonitor:
    """릴레이 모니터링 및 제어 클래스"""
    
    def __init__(self):
        """초기화"""
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
        
        # GPIO 초기화
        GPIO.setmode(GPIO.BOARD)
        for pin in self.RELAY_PINS.values():
            GPIO.setup(pin, GPIO.OUT)
        
        # 실행 플래그
        self.running = True
        
        # 터미널 설정 저장
        self.old_settings = termios.tcgetattr(sys.stdin)
        
        # 시그널 핸들러 설정
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """시그널 핸들러"""
        self.running = False
        self.cleanup()
        sys.exit(0)
    
    def clear_screen(self):
        """화면 클리어"""
        os.system('clear')
    
    def get_relay_status(self, channel):
        """릴레이 상태 확인"""
        pin = self.RELAY_PINS[channel]
        return "ON " if GPIO.input(pin) else "OFF"
    
    def toggle_relay(self, channel):
        """릴레이 토글"""
        pin = self.RELAY_PINS[channel]
        current_state = GPIO.input(pin)
        GPIO.output(pin, not current_state)
    
    def relay_on(self, channel):
        """릴레이 ON"""
        pin = self.RELAY_PINS[channel]
        GPIO.output(pin, GPIO.HIGH)
    
    def relay_off(self, channel):
        """릴레이 OFF"""
        pin = self.RELAY_PINS[channel]
        GPIO.output(pin, GPIO.LOW)
    
    def all_on(self):
        """모든 릴레이 ON"""
        for pin in self.RELAY_PINS.values():
            GPIO.output(pin, GPIO.HIGH)
    
    def all_off(self):
        """모든 릴레이 OFF"""
        for pin in self.RELAY_PINS.values():
            GPIO.output(pin, GPIO.LOW)
    
    def display_status(self):
        """상태 화면 표시"""
        self.clear_screen()
        
        # 터미널 크기 가져오기
        try:
            rows, cols = os.popen('stty size', 'r').read().split()
            term_width = min(int(cols), 80)  # 최대 80자로 제한
        except:
            term_width = 60  # 기본값
        
        # 동적 너비 계산
        line_width = term_width - 4
        header_line = "=" * line_width
        sub_line = "-" * (line_width - 2)
        
        # 헤더
        print(header_line)
        title = "릴레이 모니터링 & 제어"
        print(title.center(line_width))
        print(header_line)
        current_time = f"시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        print(current_time.center(line_width))
        print(header_line)
        print()
        
        # 릴레이 상태
        print("  [릴레이 상태]")
        print("  " + sub_line)
        for ch, name in self.SENSOR_NAMES.items():
            status = self.get_relay_status(ch)
            status_str = "ON " if status == "ON " else "OFF"
            pin = self.RELAY_PINS[ch]
            status_mark = "[●]" if status == "ON " else "[○]"
            print(f"  CH{ch} (Pin {pin:2d}): {status_mark} {status_str} - {name}")
        print()
        
        # 제어 메뉴 (컴팩트 버전)
        print("  [키보드 제어]")
        print("  " + sub_line)
        
        if term_width >= 70:
            # 넓은 화면
            print("  토글: [1] CH1  [2] CH2  [3] CH3")
            print("  ON  : [Q] CH1  [W] CH2  [E] CH3")
            print("  OFF : [A] CH1  [S] CH2  [D] CH3")
        else:
            # 좁은 화면
            print("  토글: 1(CH1) 2(CH2) 3(CH3)")
            print("  ON  : Q(CH1) W(CH2) E(CH3)")
            print("  OFF : A(CH1) S(CH2) D(CH3)")
        
        print()
        print("  전체: [7] 모두 ON  [8] 모두 OFF")
        print("  종료: [ESC] 또는 [0]")
        print(header_line)
        print("  대기 중...")
    
    def keyboard_listener(self):
        """키보드 입력 처리 스레드"""
        try:
            # 터미널을 raw 모드로 설정
            tty.setraw(sys.stdin.fileno())
            
            while self.running:
                if sys.stdin in __import__('select').select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1)
                    
                    # ESC 또는 0: 종료
                    if key == '\x1b' or key == '0':
                        self.running = False
                        break
                    
                    # 토글 제어
                    elif key == '1':
                        self.toggle_relay(1)
                    elif key == '2':
                        self.toggle_relay(2)
                    elif key == '3':
                        self.toggle_relay(3)
                    
                    # ON 제어
                    elif key.lower() == 'q':
                        self.relay_on(1)
                    elif key.lower() == 'w':
                        self.relay_on(2)
                    elif key.lower() == 'e':
                        self.relay_on(3)
                    
                    # OFF 제어
                    elif key.lower() == 'a':
                        self.relay_off(1)
                    elif key.lower() == 's':
                        self.relay_off(2)
                    elif key.lower() == 'd':
                        self.relay_off(3)
                    
                    # 전체 제어
                    elif key == '7':
                        self.all_on()
                    elif key == '8':
                        self.all_off()
                
                time.sleep(0.01)
        
        except Exception as e:
            print(f"\n키보드 입력 오류: {e}")
        finally:
            # 터미널 설정 복원
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)
    
    def display_updater(self):
        """화면 업데이트 스레드"""
        while self.running:
            self.display_status()
            # 1초마다 화면 업데이트
            for _ in range(10):
                if not self.running:
                    break
                time.sleep(0.1)
    
    def run(self):
        """메인 실행"""
        try:
            # 키보드 입력 스레드 시작
            keyboard_thread = threading.Thread(target=self.keyboard_listener)
            keyboard_thread.daemon = True
            keyboard_thread.start()
            
            # 화면 업데이트 스레드 시작
            display_thread = threading.Thread(target=self.display_updater)
            display_thread.daemon = True
            display_thread.start()
            
            # 메인 스레드는 대기
            while self.running:
                time.sleep(0.1)
        
        except KeyboardInterrupt:
            print("\n\n사용자에 의해 중단되었습니다.")
        finally:
            self.cleanup()
    
    def cleanup(self):
        """정리"""
        self.running = False
        
        # 터미널 설정 복원
        try:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)
        except:
            pass
        
        # 화면 클리어 및 상태 표시
        self.clear_screen()
        print("\n🔌 릴레이 모니터링 종료")
        print("-" * 40)
        print("최종 릴레이 상태:")
        for ch, name in self.SENSOR_NAMES.items():
            status = self.get_relay_status(ch)
            print(f"  CH{ch}: {status} - {name}")
        print("-" * 40)
        
        # GPIO 정리 (릴레이는 현재 상태 유지)
        # GPIO.cleanup()를 호출하지 않음 - 릴레이 상태 유지
        print("릴레이는 현재 상태를 유지합니다.")
        print()


def main():
    """메인 함수"""
    print("릴레이 모니터링을 시작합니다...")
    print("잠시만 기다려주세요...")
    time.sleep(1)
    
    monitor = RelayMonitor()
    monitor.run()


if __name__ == "__main__":
    main()