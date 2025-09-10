# Sonoptix Sonar ROS2 Package

[**Sonoptix Echo sonar**](https://bluerobotics.com/store/sonars/imaging-sonars/sonoptix-echo/)를 위한 ROS2 드라이버 및 처리 노드 패키지입니다. 소나와 인터페이스하여 원시 데이터를 캡처하고 시각화를 위한 극좌표 이미지로 변환합니다.

#### Sonar Data from a Joy Ride in a Pool
![Alt text](.assets/joy_ride.gif)

#### Sonar Data with Different Range Settings
![Alt text](.assets/range_demo.gif)

---

## Table of Contents
- [Overview](#overview)
- [Installation](#installation)
- [Launch Files](#launch-files)
  - [echo.launch.py](#echolaunchpy)
  - [echo_decompress.launch.py](#echo_decompresslaunchpy)
- [Nodes](#nodes)
  - [echo](#echo)
  - [echo_imager](#echo_imager)
- [License](#license)

---

## Overview
이 패키지는 두 개의 주요 노드를 포함합니다:

* **`echo.py`**: Sonoptix 소나 하드웨어와 직접 인터페이스하는 드라이버 노드입니다. 원시 소나 데이터를 캡처하여 ROS2 sensor_msgs/Image 메시지로 퍼블리시합니다.

* **`echo_imager.py`**: 원시 소나 데이터를 구독하여 극좌표(부채꼴) 이미지로 변환하는 처리 노드입니다. 변환된 이미지를 새 토픽에 퍼블리시하거나 비디오 파일로 저장할 수 있으며, ROS2 bag 파일의 데이터도 처리할 수 있습니다.

## Installation
[sonoptix_ros2](https://github.com/itskalvik/sonoptix_ros2) 패키지를 설치하려면 이 저장소를 ROS2 워크스페이스에 클론하고 `colcon`으로 빌드하세요:

```bash
cd ~/ros2_ws/src
git clone https://github.com/itskalvik/sonoptix_ros2.git
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash
```

---

## Launch Files

### `echo.launch.py`

[echo](#echo) 노드를 실행하여 소나 데이터를 퍼블리시하고 효율적인 전송을 위해 데이터 스트림을 압축합니다.
[echo](#echo) 노드는 `best_effort` 신뢰성 QoS 설정으로 퍼블리시하며, launch 파일이나 `ros2 param set` 명령으로 변경할 수 있습니다.

**Published Topics**:
- 원시 소나 데이터: `/sensor/sonar/sonoptix/data` (`sensor_msgs/Image`)
- 압축된 소나 데이터: `/sensor/sonar/sonoptix/compressed` (`sensor_msgs/CompressedImage`)

**Launch Parameters**:
- `ip`: 소나 IP 주소 (기본값: 192.168.2.42)
- `range`: 소나 범위 [m] (기본값: 12)
- `tx_mode`: 송신 모드 'auto', 'hf', 'lf', 'lflr' (기본값: auto)
- `power_state`: 초기 전원 상태 (기본값: True)
- `data_topic`: 원시 소나 데이터 토픽명 (기본값: /sensor/sonar/sonoptix/data)
- `compressed_topic`: 압축된 소나 데이터 토픽명 (기본값: /sensor/sonar/sonoptix/compressed)
- `frame_id`: 소나 데이터 프레임 ID (기본값: echo)
- `compression_level`: 압축 레벨 1-9 (기본값: 1)
- `reliability`: QoS 신뢰성 설정 (기본값: best_effort)

**Run Example**:

```bash
# 기본 설정으로 실행
ros2 launch sonoptix_ros2 echo.launch.py

# 커스텀 IP와 범위로 실행
ros2 launch sonoptix_ros2 echo.launch.py ip:=192.168.1.100 range:=50

# 커스텀 토픽명으로 실행
ros2 launch sonoptix_ros2 echo.launch.py data_topic:=/my_sonar/data compressed_topic:=/my_sonar/compressed
```

런타임 중 범위 설정 변경:

```bash
ros2 param set /echo range:=<[3 - 200] integer value>
```

### `echo_decompress.launch.py`

이전에 압축된 소나 에코 데이터를 압축 해제하여 후속 처리나 시각화에 사용합니다. 이 노드는 주로 bag 파일의 소나 데이터를 처리할 때 사용됩니다.

**Subscribed Topics**: `/sensor/sonar/sonoptix/compressed` (`sensor_msgs/CompressedImage`)

**Published Topics**: `/sensor/sonar/sonoptix/data` (`sensor_msgs/Image`)

**Launch Parameters**:
- `data_topic`: 압축 해제된 출력 토픽명 (기본값: /sensor/sonar/sonoptix/data)
- `compressed_topic`: 압축된 입력 토픽명 (기본값: /sensor/sonar/sonoptix/compressed)
- `reliability`: QoS 신뢰성 설정 (기본값: best_effort)

**Run Example**:

```bash
# 기본 설정으로 실행
ros2 launch sonoptix_ros2 echo_decompress.launch.py

# 커스텀 토픽명으로 실행
ros2 launch sonoptix_ros2 echo_decompress.launch.py compressed_topic:=/my_sonar/compressed data_topic:=/my_sonar/data
```

---

## Nodes

### `echo`

**Description**: Sonoptix Echo에서 소나 프로파일을 퍼블리시합니다.

**Published Topics**: `/sensor/sonar/sonoptix/data` (`sensor_msgs/Image`)

**Parameters**: 노드 실행 중에 파라미터를 업데이트할 수 있습니다.

| Name                 | Type    | Default            | Description                              |
|----------------------|---------|--------------------|------------------------------------------|
| `range`              | int     | 50                 | 소나 범위 (미터) [3-200]                 |
| `ip`                 | str     | "192.168.2.42"     | 소나 장치의 IP 주소                      |
| `tx_mode`            | str     | "auto"             | 송수신기 송신 모드 [`auto`, `hf`, `lf`, `lflr`] |
| `power_state`        | bool    | `True`             | 송수신기 전원 상태                       |
| `topic`              | str     | `/sensor/sonar/sonoptix/data` | 소나 프레임 퍼블리시 토픽                |
| `frame_id`           | str     | `echo`             | TF 프레임 ID                             |

**Run Example**:
```bash
ros2 run sonoptix_ros2 echo
```

---

### `echo_imager`

**Description**: Echo 소나 데이터를 극좌표 이미지로 변환하여 퍼블리시합니다.

**Subscribed Topics**: `/sensor/sonar/sonoptix/data` (`sensor_msgs/Image`)

**Published Topics**: `/sensor/sonar/sonoptix/image` (`sensor_msgs/Image`)

**Parameters**: 노드 실행 중에 파라미터를 업데이트할 수 있습니다.

| Name           | Type   | Default              | Description                               |
|----------------|--------|----------------------|-------------------------------------------|
| `data_topic`   | str    | `/sensor/sonar/sonoptix/compressed` | 소나 데이터 입력 토픽 (원시 또는 압축)    |
| `image_topic`  | str    | `/sensor/sonar/sonoptix/image`   | 시각화된 이미지 출력 토픽                 |
| `contrast`     | float  | `10.0`               | 시각화를 위한 대비 배율                   |
| `bag_file`     | str    |                      | 소나 데이터가 있는 ROS2 bag 파일 경로 (선택) |
| `video_file`   | str    |                      | 출력 mp4 비디오 파일 경로 (선택)          |

**Run Example**:
```bash
ros2 run sonoptix_ros2 echo_imager --ros-args -p contrast:=10 -p data_topic:=/sensor/sonar/sonoptix/data
```

bag 파일에서 읽을 때, 노드는 기본적으로 `echo_sonar.mp4` 비디오 파일로 출력을 내보냅니다. 

---

## License

이 패키지는 MIT 라이선스 하에 배포됩니다. 자세한 내용은 각 파일 상단 또는 [LICENSE](LICENSE) 파일을 참조하세요.

## Original Source

이 패키지는 [itskalvik/sonoptix_ros2](https://github.com/itskalvik/sonoptix_ros2)에서 포크되었으며, launch 파일에 파라미터 설정 기능이 추가되었습니다.
