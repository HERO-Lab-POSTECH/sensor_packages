# Marine Robotics Sensor Packages

해양 로봇공학 응용을 위한 ROS2 (Humble) 센서 드라이버 패키지 모음입니다. 소나 시스템과 LiDAR SLAM을 포함합니다.

## Overview

이 저장소는 해양 로봇공학 응용을 위해 최적화된 센서 드라이버 패키지 모음을 포함합니다:

- **소나 시스템**: Oculus 이미징 소나, Sonoptix 이미징 소나, Ping360 스캐닝 소나, Ping1D 고도계
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
  - M750d 및 M1200d 모델 지원
  - 별도 fan image 노드로 11Hz 실시간 성능 달성
  - marine_acoustic_msgs::SonarImage 표준 메시지 지원
- **marine_acoustic_msgs**: 해양 음향 센서 표준 메시지 정의
- **oculus_sonar_msgs**: Oculus 소나 메시지 정의
- **ping360_sonar_msgs**: Ping360 스캐닝 소나 메시지 정의
- **sonoptix_ros2**: Sonoptix Echo 이미징 소나 ROS2 드라이버 (MIT License)
  - TCP/IP 통신 기반 실시간 소나 데이터 수신
  - 이미지 압축 및 스트리밍 지원
  - ROS2 image_transport를 통한 효율적인 데이터 전송

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
│   ├── marine_acoustic_msgs/ # Marine acoustic standard messages (local)
│   ├── oculus_sonar/     # Main Oculus driver (local)
│   └── oculus_sonar_msgs/# Oculus messages (local)
├── ping1d_ros2/          # Ping1D altimeter driver
├── ping360_ros2/         # Ping360 scanning sonar
│   ├── ping360_sonar/
│   └── ping360_sonar_msgs/
└── sonoptix_ros2/        # Sonoptix Echo imaging sonar driver
```

## Installation

### 사전 요구사항 (Prerequisites)
```bash
# ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop

# Build tools
sudo apt install python3-colcon-common-extensions
sudo apt install ros-humble-image-transport ros-humble-cv-bridge ros-humble-image-transport-plugins

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
# Oculus M750d imaging sonar (기본 모드 - 11Hz)
ros2 launch oculus_sonar m750d.launch.py

# Oculus M750d with fan image (별도 노드로 fan image 처리)
ros2 launch oculus_sonar m750d_with_fan.launch.py

# Oculus M1200d imaging sonar
ros2 launch oculus_sonar m1200d.launch.py

# Oculus M1200d with fan image
ros2 launch oculus_sonar m1200d_with_fan.launch.py

# Oculus 파라미터 오버라이드 예시
ros2 launch oculus_sonar m750d.launch.py range:=10.0 gain:=80 ping_rate:=2

# Ping360 scanning sonar
ros2 launch ping360_sonar ping360.launch.py device:=/dev/ttyUSB0

# Ping1D altimeter
ros2 launch ping1d_sonar ping_sonar.launch.py device:=/dev/ttyUSB1

# Sonoptix Echo sonar
ros2 launch sonoptix_ros2 echo.launch.py ip:=192.168.2.42
```

### LiDAR SLAM
```bash
# FAST-LIO with Livox MID360
ros2 launch fast_lio mapping.launch.py config_file:=mid360.yaml

# Livox driver
ros2 launch livox_driver msg_MID360_launch.py
```

## Topic Naming Convention

모든 센서 토픽은 일관된 네이밍 컨벤션을 따릅니다:
```
/sensor/<sensor_type>/<manufacturer>_<model>/<data_type>
```

### LiDAR Topics
- **Livox MID360**: 
  - Points: `/sensor/lidar/livox_mid360/points`
  - IMU: `/sensor/ins/livox_mid360/imu`
- **Livox Avia**: 
  - Points: `/sensor/lidar/livox_avia/points`
  - IMU: `/sensor/ins/livox_avia/imu`
- **Livox HAP**: 
  - Points: `/sensor/lidar/livox_hap/points`
  - IMU: `/sensor/ins/livox_hap/imu`

### Sonar Topics
- **Oculus M750d**:
  - Raw: `/sensor/sonar/oculus/m750d/raw`
  - Image: `/sensor/sonar/oculus/m750d/image`
  - SonarImage: `/sensor/sonar/oculus/m750d/sonar`
  - Metadata: `/sensor/sonar/oculus/m750d/metadata`
  - Raw Data: `/sensor/sonar/oculus/m750d/raw_data`
  - Fan Image: `/sensor/sonar/oculus/m750d/fan_image` (별도 노드 실행 시)
- **Oculus M1200d**:
  - Raw: `/sensor/sonar/oculus/m1200d/raw`
  - Image: `/sensor/sonar/oculus/m1200d/image`
  - SonarImage: `/sensor/sonar/oculus/m1200d/sonar`
  - Metadata: `/sensor/sonar/oculus/m1200d/metadata`
  - Raw Data: `/sensor/sonar/oculus/m1200d/raw_data`
  - Fan Image: `/sensor/sonar/oculus/m1200d/fan_image` (별도 노드 실행 시)
- **Ping360**:
  - Echo: `/sensor/sonar/ping360/echo`
  - Scan: `/sensor/sonar/ping360/scan`
  - Image: `/sensor/sonar/ping360/image`
- **Ping1D**:
  - Range: `/sensor/sonar/ping1d/range`
- **Sonoptix Echo**:
  - Data: `/sensor/sonar/sonoptix/data`
  - Compressed: `/sensor/sonar/sonoptix/compressed`

### SLAM Output Topics
- **FAST-LIO**:
  - Odometry: `/fast_lio/odometry`
  - Registered Cloud: `/fast_lio/cloud_registered`
  - Map: `/fast_lio/map`
  - Path: `/fast_lio/path`

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

#### Oculus M750d/M1200d
- `ip_address`: 소나 IP 주소 (기본값: 'auto' 자동탐색, 예: '192.168.0.200')
- `range`: 소나 범위 [m] (M750d: 0.1-120m, M1200d: 0.1-40m, 기본값: 2.0)
- `gain`: 게인 [%] (1-100, 기본값: 100)
- `gamma`: 감마 보정 (0-255, 기본값: 200)
- `ping_rate`: 핑 속도 (0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest, 5=Standby, 기본값: 3)
- `freq_mode`: 주파수 모드 (1=저주파, 2=고주파, 기본값: 2)
  - M750d: 1=750kHz, 2=1.2MHz
  - M1200d: 1=1.2MHz, 2=2.1MHz
- `data_size`: 데이터 크기 ('8bit', '16bit', '32bit', 기본값: '8bit')
- `num_beams`: 빔 개수 (0=256빔, 1=512빔, 기본값: 1)

#### Oculus Fan Imager (별도 노드)
Fan image 처리를 위한 별도 노드 파라미터:
- `input_topic`: 입력 SonarImage 토픽 (기본값: '/sensor/sonar/oculus/m750d/sonar')
- `output_topic`: 출력 fan image 토픽 (기본값: '/sensor/sonar/oculus/m750d/fan_image')
- `sonar_model`: 소나 모델 ('m750d' 또는 'm1200d', 기본값: 'm750d')
- `freq_mode`: 주파수 모드 (1=저주파, 2=고주파, 기본값: 2)
- `apply_colormap`: 컬러맵 적용 여부 (기본값: true)

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

#### Sonoptix Echo
- `ip`: 소나 IP 주소 (기본값: 192.168.0.203)
- `range`: 소나 범위 [m] (기본값: 12)
- `tx_mode`: 송신 모드 'auto' 또는 'manual' (기본값: auto)
- `power_state`: 초기 전원 상태 (기본값: True)
- `data_topic`: 원시 소나 데이터 토픽 (기본값: /sensor/sonar/sonoptix/data)
- `compressed_topic`: 압축된 소나 데이터 토픽 (기본값: /sensor/sonar/sonoptix/compressed)
- `frame_id`: 소나 데이터 프레임 ID (기본값: echo)
- `compression_level`: 압축 레벨 1-9 (기본값: 1)
- `reliability`: QoS 신뢰성 설정 'best_effort' 또는 'reliable' (기본값: best_effort)

**참고**: echo.launch.py는 자동으로 image_transport 노드를 실행하여 raw 이미지를 compressed 형식으로 변환합니다.

## Network Configuration

### Sonoptix Echo 소나 네트워크 설정

Sonoptix Echo 소나를 사용하기 위해서는 올바른 네트워크 설정이 필요합니다:

#### 기본 네트워크 설정
- **센서 IP**: `192.168.0.203` (기본값)
- **컴퓨터 IP**: `192.168.0.12` (권장)
- **네트워크 대역**: `192.168.0.x/24`

#### 네트워크 설정 명령어
```bash
# 센서와 같은 네트워크 대역으로 설정
sudo ip addr del 192.168.2.42/24 dev enx3c18a0127d4c
sudo ip addr add 192.168.0.12/24 dev enx3c18a0127d4c

# 연결 테스트
ping 192.168.0.203
curl http://192.168.0.203:8000/api/v1/status
```

#### 문제 해결
- **Connection refused**: 센서가 완전히 부팅되지 않았거나 다른 포트를 사용 중
- **No route to host**: 네트워크 대역이 다름 (센서와 컴퓨터가 다른 서브넷에 있음)
- **센서 IP 확인**: ARP 테이블에서 센서 IP 확인
  ```bash
  arp -a | grep enx3c18a0127d4c
  ```

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