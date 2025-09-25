# ROS2 Oculus 소나 드라이버

Blueprint Subsea Oculus 이미징 소나 시스템과 인터페이스하기 위한 ROS2 패키지입니다.

## Overview

이 패키지는 Oculus 소나 장치를 위한 ROS2 컴포넌트 기반 드라이버를 제공하며, 소나 이미지와 메타데이터를 ROS2 토픽으로 퍼블리시합니다. M750d와 M1200d 모델을 모두 지원하며, 실시간 파라미터 조정과 유연한 배포 옵션을 제공합니다. 또한 polar 좌표계 데이터를 Cartesian 좌표계로 변환하여 fan image를 생성하는 기능을 포함합니다.

## Supported Models

- **Oculus M750d**: 750kHz/1.2MHz 듀얼 주파수, 120m 범위, 130° FOV
- **Oculus M1200d**: 1.2MHz/2.1MHz 듀얼 주파수, 40m 범위, 130°/60° FOV

## Package Structure

```
oculus_sonar/
├── CMakeLists.txt              # 빌드 설정
├── package.xml                 # 패키지 메타데이터
├── plugins.xml                 # 컴포넌트 플러그인 정의
├── README.md
│
├── include/oculus_sonar/
│   ├── oculus_driver_component.hpp
│   ├── ping_to_sonar_image.hpp
│   ├── polar_to_cartesian.hpp
│   └── publishing_data_rx.h
│
├── src/
│   ├── oculus_driver_component.cpp
│   └── publishing_data_rx.cpp
│
├── launch/
│   ├── m750d.launch.py         # M750d 전용 launch 파일
│   └── m1200d.launch.py        # M1200d 전용 launch 파일
│
└── config/
    ├── m750d_params.yaml       # M750d 기본 파라미터
    └── m1200d_params.yaml      # M1200d 기본 파라미터
```

## Dependencies

- **ROS2** (Humble/Iron에서 테스트됨)
- **rclcpp** 및 **rclcpp_components**
- **liboculus** - Oculus 소나 통신 라이브러리
- **marine_acoustic_msgs** - 해양 음향 메시지 정의
- **apl_msgs** - 원시 데이터 메시지 정의
- **cv_bridge** 및 **image_transport**
- **oculus_sonar_msgs** - Oculus 메타데이터 메시지
- **OpenCV** - Fan image 생성을 위한 이미지 처리

## Installation

### 1. 저장소 클론
```bash
cd ~/ros2_ws/src
git clone <repository_url> oculus_ros2
```

### 2. 의존성 설치
```bash
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
```

### 3. 패키지 빌드
```bash
colcon build --packages-select oculus_sonar
source install/setup.bash
```

## Usage

### M750d 실행

#### 기본 설정으로 실행 (config 파일 사용)
```bash
ros2 launch oculus_sonar m750d.launch.py
```

#### 파라미터 오버라이드
```bash
ros2 launch oculus_sonar m750d.launch.py range:=10.0 gain:=80
ros2 launch oculus_sonar m750d.launch.py ip_address:=192.168.0.200
```

### M1200d 실행

#### 기본 설정으로 실행
```bash
ros2 launch oculus_sonar m1200d.launch.py
```

#### 파라미터 오버라이드
```bash
ros2 launch oculus_sonar m1200d.launch.py range:=20.0 ping_rate:=1
```

## Parameter Priority

파라미터는 다음 우선순위에 따라 적용됩니다 (높은 우선순위 → 낮은 우선순위):

1. **명령줄 args** - Launch 시 제공되는 파라미터
   ```bash
   ros2 launch oculus_sonar m750d.launch.py range:=10.0
   ```

2. **YAML config 파일** - config/*.yaml 파일의 설정
   ```yaml
   # m750d_params.yaml
   range: 2.0
   ```

3. **C++ 코드 기본값** - 소스 코드에 정의된 기본값
   ```cpp
   this->declare_parameter<double>("range", 2.0);
   ```

## Parameters

| 파라미터 | 타입 | 기본값 | 범위 | 설명 |
|-----------|------|---------|-------|-------------|
| `sonar_model` | string | "m750d"/"m1200d" | - | 소나 모델 지정 |
| `ip_address` | string | "192.168.0.200" | - | 소나 IP 주소 또는 "auto" (자동 검색) |
| `frame_id` | string | "oculus" | - | TF 프레임 ID |
| `range` | double | 2.0/10.0 | 0-120 (M750d), 0-40 (M1200d) | 소나 범위(미터) |
| `gain` | int | 100 | 1-100 | 게인 백분율 |
| `gamma` | int | 200 | 0-255 | 감마 보정 |
| `ping_rate` | int | 3 | 0-5 | 핑 속도* |
| `freq_mode` | int | 2 | 1-2 | 주파수 모드** |
| `data_size` | string | "8bit" | - | "8bit", "16bit", "32bit" |
| `num_beams` | int | 1 | 0-1 | 0=256 빔, 1=512 빔 |
| `publish_fan_image` | bool | true | - | Fan image 퍼블리싱 활성화 |
| `send_range_as_meters` | bool | true | - | 범위 단위 (true=미터, false=백분율) |
| `send_gain` | bool | true | - | 게인 전송 활성화 |
| `send_simple_return` | bool | true | - | 단순 반환 메시지 |
| `gain_assistance` | bool | false | - | 게인 보조 활성화 |

*핑 속도: 0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest, 5=Standby
**주파수 모드: M750d (1=750kHz, 2=1.2MHz), M1200d (1=1.2MHz, 2=2.1MHz)

### IP Address Auto Detection

`ip_address:=auto`를 사용하면 네트워크에서 Oculus 소나를 자동으로 검색합니다:

```bash
ros2 launch oculus_sonar m750d.launch.py ip_address:=auto
```

**동작 원리:**
- Oculus 소나가 UDP 포트 52102로 브로드캐스트하는 상태 메시지 수신
- 첫 번째로 발견된 소나에 자동 연결

**권장 사용 시나리오:**
- 개발/테스트 환경
- 소나 IP가 자주 변경되는 환경

**주의사항:**
- 네트워크에 여러 Oculus가 있으면 예측 불가
- 실제 운용 시에는 고정 IP 사용 권장

## Dynamic Parameter Tuning

### 런타임 파라미터 변경
```bash
# M750d 노드의 파라미터 변경
ros2 param set /oculus/m750d range 5.0
ros2 param set /oculus/m750d gain 80

# M1200d 노드의 파라미터 변경
ros2 param set /oculus/m1200d freq_mode 1
```

### 현재 파라미터 조회
```bash
ros2 param list /oculus/m750d
ros2 param get /oculus/m750d range
```

## Published Topics

토픽은 모델에 따라 다음 형식을 따릅니다:
- M750d: `/sensor/sonar/oculus/m750d/*`
- M1200d: `/sensor/sonar/oculus/m1200d/*`

| 토픽 | 메시지 타입 | 설명 |
|-------|--------------|-------------|
| `/sensor/sonar/oculus/<model>/sonar` | `marine_acoustic_msgs/msg/SonarImage` | 메타데이터 포함 전체 소나 데이터 |
| `/sensor/sonar/oculus/<model>/image` | `sensor_msgs/msg/Image` | Polar 좌표계 원본 이미지 |
| `/sensor/sonar/oculus/<model>/fan_image` | `sensor_msgs/msg/Image` | Cartesian 좌표계 Fan 이미지 |
| `/sensor/sonar/oculus/<model>/metadata` | `oculus_sonar_msgs/msg/OculusMetadata` | TVG 포함 소나 메타데이터 |
| `/sensor/sonar/oculus/<model>/raw_data` | `apl_msgs/msg/RawData` | 원시 통신 데이터 |
| `/sensor/sonar/oculus/<model>/param/*` | 다양함 | 녹화용 파라미터 퍼블리싱 |

### 파라미터 퍼블리싱 토픽

실시간 파라미터가 다음 토픽으로 퍼블리시됩니다:
- `/sensor/sonar/oculus/<model>/param/sonar_model`
- `/sensor/sonar/oculus/<model>/param/range`
- `/sensor/sonar/oculus/<model>/param/gain`
- `/sensor/sonar/oculus/<model>/param/gamma`
- `/sensor/sonar/oculus/<model>/param/ping_rate`
- `/sensor/sonar/oculus/<model>/param/freq_mode`
- `/sensor/sonar/oculus/<model>/param/data_size`
- `/sensor/sonar/oculus/<model>/param/ip_address`
- `/sensor/sonar/oculus/<model>/param/frame_id`

## Node Names

노드는 모델에 따라 다음과 같이 명명됩니다:
- M750d: `/oculus/m750d`
- M1200d: `/oculus/m1200d`

## Troubleshooting

### 연결 문제
```bash
# 소나 연결 확인
ping <sonar_ip_address>

# 통신 원시 데이터 모니터링
ros2 topic echo /sensor/sonar/oculus/m750d/raw_data

# 토픽 퍼블리싱 확인
ros2 topic hz /sensor/sonar/oculus/m750d/raw
```

### 노드 상태 확인
```bash
# 실행 중인 노드 확인
ros2 node list

# 노드 정보 확인
ros2 node info /oculus/m750d
```

### 시각화 (rviz2)
```bash
ros2 launch oculus_sonar m750d.launch.py
rviz2
# rviz2에서:
# 1. Image 디스플레이 추가
# 2. Polar 이미지: /sensor/sonar/oculus/m750d/image
# 3. Fan 이미지: /sensor/sonar/oculus/m750d/fan_image
```

### Fan Image 정보

Fan image는 polar 좌표계의 소나 데이터를 Cartesian 좌표계로 변환한 이미지입니다:
- **좌표계**: 소나가 이미지 하단 중앙에 위치
- **FOV**: M750d는 130°, M1200d는 130° 또는 60° (모드에 따름)
- **해상도**: Range와 주파수에 따라 자동 조정
  - 2m range @ 1.2MHz: 약 5mm 해상도 (399x724 pixels)
  - 더 긴 range 설정 시 해상도가 낮아집니다

## Development

### 디버그 빌드
```bash
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Debug
```

### 로그 모니터링
```bash
ros2 topic echo /rosout | grep oculus
```

## Model-Specific Configurations

### M750d 최적 설정
```yaml
sonar_model: "m750d"
range: 2.0          # 근거리 고해상도
freq_mode: 2        # 1.2MHz (고주파)
ping_rate: 3        # Low (안정적)
```

### M1200d 최적 설정
```yaml
sonar_model: "m1200d"
range: 10.0         # 중거리 범위
freq_mode: 2        # 2.1MHz (초고주파)
ping_rate: 2        # Highest (빠른 업데이트)
```

## License

MIT 라이선스 - 자세한 내용은 LICENSE 파일을 참조하세요.

## Technical Notes

### Beam Configuration
- M750d는 512개의 빔을 제공하여 130° FOV를 커버합니다
- 각 빔 사이의 간격은 약 0.25°입니다
- 하드웨어는 실제로 70° FOV의 bearing 데이터만 제공하지만, 소프트웨어에서 130° FOV로 확장합니다

### Resolution Details
소나 해상도는 range와 주파수에 따라 달라집니다:
- Range resolution: 약 5mm @ 2m range
- Azimuth resolution: 512 beams across 130° FOV
- 실제 픽셀 수는 하드웨어에서 제공하는 range bins 수에 따라 결정됩니다

## Contribution

프로젝트 저장소에 이슈와 풀 리퀘스트를 제출해 주세요.