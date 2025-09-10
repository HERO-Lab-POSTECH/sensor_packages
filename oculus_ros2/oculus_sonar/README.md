# ROS2 Oculus 소나 드라이버

Blueprint Subsea Oculus 이미징 소나 시스템과 인터페이스하기 위한 ROS2 패키지입니다.

## Overview

이 패키지는 Oculus 소나 장치를 위한 ROS2 컴포넌트 기반 드라이버를 제공하며, 소나 이미지와 메타데이터를 ROS2 토픽으로 퍼블리시합니다. 실시간 파라미터 조정과 유연한 배포 옵션을 지원합니다.

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
│   └── publishing_data_rx.h
│
├── src/
│   └── oculus_driver_component.cpp
│
├── launch/
│   └── oculus_launch.py        # Launch 파일
│
├── config/
│   └── oculus_params.yaml      # 기본 파라미터
│
└── test/
    └── unit/
        └── main.cpp            # 단위 테스트
```

## Dependencies

- **ROS2** (Humble/Iron에서 테스트됨)
- **rclcpp** 및 **rclcpp_components**
- **liboculus** - Oculus 소나 통신 라이브러리
- **marine_acoustic_msgs** - 해양 음향 메시지 정의
- **apl_msgs** - 원시 데이터 메시지 정의
- **cv_bridge** 및 **image_transport**
- **oculus_sonar_msgs** - Oculus 메타데이터 메시지

**참고:** ROS1의 `sonar_image_proc`와 `g3log_ros` 패키지는 아직 ROS2로 포팅되지 않았습니다. 소나 후처리 및 시각화 기능을 위해서는 별도의 ROS2 호환 패키지가 필요할 수 있습니다.

## Installation

### 1. 저장소 클론
```bash
cd ~/ros2_ws/src
git clone <repository_url> oculus_sonar
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

### 기본 실행

기본 파라미터로 Oculus 드라이버 시작:
```bash
ros2 launch oculus_sonar oculus_launch.py
```

### 사용자 정의 파라미터로 실행

실행 시점에 파라미터 재정의:
```bash
ros2 launch oculus_sonar oculus_launch.py \
    ip_address:=192.168.1.10 \
    range:=5.0 \
    gamma:=150 \
    data_size:=16bit
```

### 독립 노드 실행 (컴포넌트 컨테이너 없이)
```bash
ros2 run oculus_sonar oculus_driver_node
```

## Parameters

| 파라미터 | 타입 | 기본값 | 범위 | 설명 |
|-----------|------|---------|-------|-------------|
| `ip_address` | string | "auto" | - | 소나 IP 주소 또는 자동 탐지를 위한 "auto" |
| `frame_id` | string | "oculus" | - | 소나 데이터를 위한 TF 프레임 ID |
| `range` | double | 2.0 | 0-40 | 소나 범위(미터) |
| `gain` | int | 100 | 1-100 | 게인 백분율 |
| `gamma` | int | 200 | 0-255 | 감마 보정 |
| `ping_rate` | int | 3 | 0-5 | 핑 속도 (0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest, 5=Standby) |
| `freq_mode` | int | 2 | 1-2 | 주파수 모드 (1=Low, 2=High) |
| `data_size` | string | "8bit" | - | 데이터 크기 ("8bit", "16bit", "32bit") |
| `num_beams` | int | 1 | 0-1 | 빔 수 (0=256개, 1=512개) |
| `send_range_as_meters` | bool | true | - | 범위를 미터 단위로 전송 vs 백분율 |
| `send_gain` | bool | true | - | 게인 전송 활성화 |
| `send_simple_return` | bool | true | - | 단순 반환 메시지 전송 |
| `gain_assistance` | bool | false | - | 게인 보조 활성화 |

## Dynamic Parameter Tuning

드라이버가 실행 중일 때 파라미터 변경:

```bash
ros2 param set /oculus_sonar range 5.0
ros2 param set /oculus_sonar gain 80
ros2 param set /oculus_sonar data_size "16bit"
ros2 param set /oculus_sonar freq_mode 1
```

### 현재 파라미터 조회
```bash
ros2 param list /oculus_sonar
ros2 param get /oculus_sonar range
```

### GUI 파라미터 조정
```bash
rqt
# 메뉴: Plugins > Configuration > Parameter Reconfigure
```

## Published Topics

| 토픽 | 메시지 타입 | 설명 |
|-------|--------------|-------------|
| `/sensor/sonar/oculus_m750d/image` | `marine_acoustic_msgs/msg/SonarImage` | 메타데이터를 포함한 전체 소나 데이터 |
| `/sensor/sonar/oculus_m750d/raw` | `sensor_msgs/msg/Image` | rviz2 시각화용 표준 이미지 |
| `/sensor/sonar/oculus_m750d/metadata` | `oculus_sonar_msgs/msg/OculusMetadata` | TVG를 포함한 소나 메타데이터 |
| `/sensor/sonar/oculus_m750d/raw_data` | `apl_msgs/msg/RawData` | 원시 통신 데이터 |

## Configuration File

### YAML 설정 사용
사용자 정의 파라미터 파일 생성:

```yaml
# my_oculus_config.yaml
oculus:
  oculus_sonar:
    ros__parameters:
      ip_address: "192.168.1.10"
      range: 10.0
      gain: 75
      gamma: 180
      ping_rate: 1
      freq_mode: 2
      data_size: "16bit"
```

설정 로드:
```bash
ros2 launch oculus_sonar oculus_launch.py \
    params_file:=path/to/my_oculus_config.yaml
```

## Troubleshooting

### 연결 문제
```bash
# 소나 연결 가능 여부 확인
ping <sonar_ip_address>

# 통신을 위한 원시 데이터 모니터링
ros2 topic echo /sensor/sonar/oculus_m750d/raw_data

# 소나 이미지가 퍼블리시되는지 확인
ros2 topic hz /sensor/sonar/oculus_m750d/raw
ros2 topic hz /sensor/sonar/oculus_m750d/image
```

### 시각화 문제
```bash
# 사용 가능한 이미지 토픽 목록 확인
ros2 topic list | grep image

# 이미지 메시지 세부 정보 확인
ros2 topic info /sensor/sonar/oculus_m750d/raw
ros2 interface show sensor_msgs/msg/Image
```

### 파라미터 문제
```bash
# 파라미터를 기본값으로 리셋
ros2 param delete /oculus_sonar range
# YAML/launch 파일의 기본값 사용
```

### 로그 모니터링
```bash
# 드라이버 로그 모니터링
ros2 topic echo /rosout | grep oculus
```

## Development

### 소스에서 빌드
```bash
cd ~/ros2_ws
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Debug
```

### 테스트 실행
```bash
colcon test --packages-select oculus_sonar
colcon test-result --verbose
```

## Limitations

현재 ROS2 버전에서는 다음 기능들이 제한됩니다:

- **소나 시각화**: ROS1의 `sonar_image_proc/draw_sonar`가 ROS2로 포팅되지 않음
- **소나 후처리**: ROS1의 `sonar_image_proc/sonar_postprocessor`가 ROS2로 포팅되지 않음
- **고급 로깅**: ROS1의 `g3log_ros`가 ROS2로 포팅되지 않음

**대안:**
- 소나 이미지는 `/sensor/sonar/oculus_m750d/raw` 토픽을 통해 `rviz2`에서 시각화 가능 (표준 Image 형식)
- 고급 소나 데이터는 `/sensor/sonar/oculus_m750d/image` 토픽을 통해 이용 가능 (커스텀 SonarImage 형식)
- 후처리는 별도의 ROS2 노드로 구현 가능
- 기본 ROS2 로깅 시스템 사용

### rviz2에서 시각화

드라이버는 커스텀 소나 데이터와 표준 이미지를 모두 퍼블리시합니다:

```bash
ros2 launch oculus_sonar oculus_launch.py
rviz2
# rviz2에서:
# 1. Image 디스플레이 추가
# 2. 토픽을 다음으로 설정: /sensor/sonar/oculus_m750d/raw
# 3. 소나 이미지가 표시됨
```

**토픽 선택:**
- **`/sensor/sonar/oculus_m750d/raw`** - rviz2 및 표준 이미지 도구용
- **`/sensor/sonar/oculus_m750d/image`** - 전체 소나 메타데이터를 포함한 커스텀 처리용

## License

MIT 라이선스 - 자세한 내용은 LICENSE 파일을 참조하세요.

## Contribution

프로젝트 저장소에 이슈와 풀 리퀘스트를 제출해 주세요.