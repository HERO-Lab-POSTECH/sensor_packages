# Marine Robotics Sensor Packages

해양 로봇공학 응용을 위한 ROS2 (Humble) 센서 드라이버 패키지 모음입니다. 소나 시스템과 LiDAR SLAM을 포함합니다.

## Overview

이 저장소는 해양 로봇공학 응용을 위해 최적화된 센서 드라이버 패키지 모음을 포함합니다:

- **소나 시스템**: Oculus 이미징 소나, Ping360 스캐닝 소나, Ping1D 고도계
- **LiDAR 시스템**: Livox 드라이버 및 FAST-LIO SLAM
- **지원 라이브러리**: 메시지 정의 및 통신 라이브러리

## Branch Structure

- `humble-devel`: ROS2 Humble 패키지 (Ubuntu 22.04)
- `noetic-devel`: ROS1 Noetic 패키지 (Ubuntu 20.04) - *준비 중*

## Package Sources & Modifications

### 수정된 패키지 (Modified Packages)

#### ping360_ros2
- **원본 저장소**: https://github.com/CentraleNantesRobotics/ping360_sonar.git (branch: ros2)
- **라이선스**: MIT License
- **수정 사항**:
  - launch 파일에 유연한 포트 설정을 위한 device 파라미터 추가
  - UDP 연결 파라미터 추가 (udp_address, udp_port)
  - baudrate 및 connection_type 파라미터로 launch 파일 개선
  - 기본 device를 `/dev/ttyUSB0`에서 `/dev/ping360`으로 변경 (설정 가능)

#### ping1d_ros2
- **원본 저장소**: https://github.com/tasada038/ping_sonar_ros.git (branch: master)
- **라이선스**: Apache License 2.0
- **수정 사항**:
  - launch 파일에 device 파라미터 추가
  - 포괄적인 launch 파라미터 추가 (speed_of_sound, interval, gain 등)
  - RViz 실행 제어를 위한 use_rviz 파라미터 추가
  - 컴포넌트의 포트 파라미터 처리 수정
  - 기본 device를 `/dev/ttyUSB0`에서 `/dev/ping`으로 변경 (설정 가능)

### 수정되지 않은 외부 패키지 (Unmodified External Packages)

#### Livox 패키지
- **fast_lio**: 
  - 원본: https://github.com/Ericsii/FAST_LIO_ROS2.git
  - 라이선스: GPL-2.0
  - 설명: 실시간 LiDAR-관성 주행거리 측정 및 매핑
- **livox_driver**: 
  - 원본: https://github.com/Livox-SDK/livox_ros_driver2.git (branch: master)
  - 라이선스: Livox SDK License
  - 설명: Livox LiDAR ROS2 드라이버
- **livox_sdk**: 
  - 원본: https://github.com/Livox-SDK/Livox-SDK2.git (branch: master)
  - 라이선스: Livox SDK License
  - 설명: Livox LiDAR 통신 SDK

#### Oculus 소나 의존성 패키지
- **apl_msgs**: 
  - 원본: https://gitlab.com/apl-ocean-engineering/apl_msgs.git (branch: ros2)
  - 라이선스: BSD-3-Clause
  - 설명: APL 해양 공학 메시지 정의
- **marine_msgs**: 
  - 원본: https://github.com/apl-ocean-engineering/marine_msgs.git (branch: ros2)
  - 라이선스: BSD-3-Clause
  - 설명: 해양 센서 메시지 정의
- **liboculus**: 
  - 원본: https://github.com/apl-ocean-engineering/liboculus.git (branch: dev/hybrid_ros1_ros2)
  - 라이선스: BSD License
  - 설명: Blueprint Subsea Oculus 소나 통신 라이브러리
- **g3log**: 
  - 원본: https://github.com/KjellKod/g3log.git (branch: master)
  - 라이선스: Unlicense
  - 설명: 비동기 로깅 라이브러리

### 로컬 개발 패키지 (Locally Developed Packages)
- **oculus_sonar**: Blueprint Subsea Oculus 이미징 소나용 커스텀 ROS2 드라이버
- **oculus_sonar_msgs**: Oculus 소나 메시지 정의
- **ping360_sonar_msgs**: Ping360 스캐닝 소나 메시지 정의
- **sonoptix_ros2**: Sonoptix 소나 ROS2 인터페이스 (MIT License)

## Package Structure

```
sensor_packages/
├── livox_ros2/
│   ├── fast_lio/         # LiDAR-Inertial SLAM
│   ├── livox_driver/     # Livox ROS2 driver
│   └── livox_sdk/        # Livox SDK
├── oculus_ros2/
│   ├── apl_msgs/         # APL message definitions
│   ├── g3log/            # Logging library
│   ├── liboculus/        # Oculus communication library
│   ├── marine_msgs/      # Marine sensor messages
│   ├── oculus_sonar/     # Main Oculus driver (local)
│   └── oculus_sonar_msgs/# Oculus messages (local)
├── ping1d_ros2/          # Ping1D altimeter driver
└── ping360_ros2/         # Ping360 scanning sonar
    ├── ping360_sonar/
    └── ping360_sonar_msgs/
```

## Installation

### 사전 요구사항 (Prerequisites)
```bash
# ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop

# Build tools
sudo apt install python3-colcon-common-extensions
sudo apt install ros-humble-image-transport ros-humble-cv-bridge

# Additional dependencies
sudo apt install ros-humble-pcl-ros ros-humble-pcl-conversions
sudo apt install ros-humble-tf2-sensor-msgs
```

### 빌드 방법 (Building)
```bash
# Clone repository
git clone -b humble-devel https://github.com/HERO-Lab-POSTECH/sensor_packages.git
cd sensor_packages

# Create workspace
mkdir -p ~/ros2_ws/src
cp -r * ~/ros2_ws/src/

# Install dependencies
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -y

# Build
colcon build --symlink-install
source install/setup.bash
```

## Usage Examples

### 소나 시스템 (Sonar Systems)
```bash
# Oculus imaging sonar
ros2 launch oculus_sonar oculus_launch.py

# Ping360 scanning sonar
ros2 launch ping360_sonar ping360.launch.py device:=/dev/ttyUSB0

# Ping1D altimeter
ros2 launch ping1d_sonar ping_sonar.launch.py device:=/dev/ttyUSB1
```

### LiDAR SLAM
```bash
# FAST-LIO with Livox MID360
ros2 launch fast_lio mapping.launch.py config_file:=mid360.yaml

# Livox driver
ros2 launch livox_driver msg_MID360_launch.py
```

## Device Configuration

### 기본 디바이스 경로 (Default Device Paths)
패키지는 다음의 기본 디바이스 경로를 사용합니다 (launch 파라미터로 변경 가능):
- **Ping360**: `/dev/ping360` (또는 `/dev/ttyUSB0`)
- **Ping1D**: `/dev/ping` (또는 `/dev/ttyUSB0`)
- **Oculus**: 네트워크를 통한 자동 탐색

### Udev Rules (선택사항)
일관된 디바이스 명명을 위해 udev rules를 생성할 수 있습니다:

```bash
# /etc/udev/rules.d/99-marine-sensors.rules
SUBSYSTEM=="tty", ATTRS{serial}=="PING360_SERIAL", SYMLINK+="ping360"
SUBSYSTEM=="tty", ATTRS{serial}=="PING1D_SERIAL", SYMLINK+="ping"

sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Launch 파라미터 (Launch Parameters)

#### Ping360
- `device`: 시리얼 디바이스 경로 (기본값: `/dev/ping360`)
- `baudrate`: 시리얼 통신 속도 (기본값: 115200)
- `connection_type`: 'serial' 또는 'udp' (기본값: serial)
- `udp_address`: 네트워크 연결용 UDP 주소
- `udp_port`: 네트워크 연결용 UDP 포트

#### Ping1D
- `device`: 시리얼 디바이스 경로 (기본값: `/dev/ping`)
- `speed_of_sound`: 음속 mm/s 단위 (기본값: 1450000)
- `interval`: 핑 간격 ms 단위 (기본값: 100)
- `gain`: 게인 설정 0-6 (기본값: 1)
- `scan_start`: 시작 거리 mm 단위 (기본값: 100)
- `scan_length`: 스캔 길이 mm 단위 (기본값: 3000)
- `use_rviz`: RViz 시각화 실행 (기본값: true)

## Troubleshooting

### 일반적인 문제 해결 (Common Issues)

1. **디바이스를 찾을 수 없음**: USB 권한 확인
   ```bash
   sudo chmod 666 /dev/ttyUSB*
   ```

2. **빌드 에러**: 모든 의존성이 설치되었는지 확인
   ```bash
   rosdep install --from-paths src --ignore-src -y
   ```

3. **Livox SDK 문제**: SDK가 제대로 소스되었는지 확인
   ```bash
   source ~/ros2_ws/install/setup.bash
   ```

4. **rosidl_typesupport_c 에러**: ROS2 메시지 생성 패키지 설치
   ```bash
   sudo apt install ros-humble-rosidl-typesupport-c ros-humble-rosidl-default-generators
   ```

## Contributing

기여를 환영합니다! 적절한 브랜치에 Pull Request를 제출해주세요:
- `humble-devel`: ROS2 Humble 변경사항
- `noetic-devel`: ROS1 Noetic 변경사항

## License

각 패키지는 원본 라이선스를 유지합니다. 구체적인 라이선스 파일은 각 패키지 디렉토리를 참조하세요:

### 외부 패키지 라이선스
- **ping360_ros2**: MIT License (CentraleNantesRobotics)
- **ping1d_ros2**: Apache License 2.0 (tasada038)
- **fast_lio**: GPL-2.0 License (Ericsii)
- **livox_driver/livox_sdk**: Livox SDK License Agreement
- **liboculus**: BSD License (APL Ocean Engineering)
- **apl_msgs/marine_msgs**: BSD-3-Clause License
- **g3log**: Unlicense (Public Domain)

### 로컬 개발 패키지
- **oculus_sonar**: MIT License
- **oculus_sonar_msgs**: MIT License
- **ping360_sonar_msgs**: MIT License
- **sonoptix_ros2**: MIT License

## Contact

HERO Lab, POSTECH
- GitHub: https://github.com/HERO-Lab-POSTECH
- Website: https://hero.postech.ac.kr/

## Acknowledgments

이 저장소는 다음 기여자들의 패키지를 포함합니다:
- **CentraleNantesRobotics**: Ping360 드라이버 제공
- **Blue Robotics**: Ping 소나 라이브러리 제공
- **Livox-SDK Team**: LiDAR 드라이버 제공
- **APL Ocean Engineering**: 해양 메시지 정의 제공
- **Fast-LIO Team (HKU Mars Lab)**: SLAM 구현 제공
- **tasada038**: Ping1D ROS2 드라이버 제공
- **KjellKod**: g3log 로깅 라이브러리 제공

## Original Sources

원본 코드 출처 및 상세 정보:
- Ping360: https://github.com/CentraleNantesRobotics/ping360_sonar
- Ping1D: https://github.com/tasada038/ping_sonar_ros
- FAST-LIO: https://github.com/hku-mars/FAST_LIO
- Livox ROS Driver: https://github.com/Livox-SDK/livox_ros_driver2
- liboculus: https://github.com/apl-ocean-engineering/liboculus
- Marine Messages: https://github.com/apl-ocean-engineering/marine_msgs