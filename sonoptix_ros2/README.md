# Sonoptix Sonar ROS2 Package

This ROS 2 package provides a driver and processing node for the [**Sonoptix Echo sonar**](https://bluerobotics.com/store/sonars/imaging-sonars/sonoptix-echo/). It allows you to interface with the sonar, capture raw data, and convert it into a polar image for visualization.

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
This package contains two main nodes:

* **`echo.py`**: A driver node that interfaces directly with the Sonoptix sonar hardware. It captures raw sonar data and publishes it as a ROS 2 sensor_msgs/Image message.

* **`echo_imager.py`**: A processing node that subscribes to the raw sonar data, converts it into a polar (fan-shaped) image, and can either publish this image on a new topic or save it to a video file. This node can also process data from a ROS 2 bag file.

## Installation
To install the [sonoptix_ros2](https://github.com/itskalvik/sonoptix_sonar) package, clone this repository into your ROS 2 workspace and build it using `colcon`:

```bash
cd ~/ros2_ws/src
git clone https://github.com/itskalvik/sonoptix_sonar.git
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash
```

---

## Launch Files

### `echo.launch.py`

**Description**: Launches the echo node with compressed image transport.

**Launch Command**:
```bash
ros2 launch sonoptix_ros2 echo.launch.py
```

**Parameters**:
- `sonar_range`: Sonar range in meters (default: 12)
- `data_topic`: Topic name for raw sonar data (default: `/sensor/sonar/sonoptix/data`)
- `compressed_topic`: Topic name for compressed data (default: `/sensor/sonar/sonoptix/compressed`)
- `compression_level`: PNG compression level 1-9 (default: 1)
- `reliability`: QoS reliability setting (default: 'best_effort')

---

### `echo_decompress.launch.py`

**Description**: Launches a node to decompress the compressed sonar data for visualization.

**Launch Command**:
```bash
ros2 launch sonoptix_ros2 echo_decompress.launch.py
```

**Parameters**:
- `data_topic`: Output topic for decompressed data (default: `/sensor/sonar/sonoptix/data`)
- `compressed_topic`: Input topic for compressed data (default: `/sensor/sonar/sonoptix/compressed`)
- `reliability`: QoS reliability setting (default: 'best_effort')

---

## Nodes

### `echo`

**Description**: Publishes sonar profiles from the Sonoptix Echo.

**Published Topics**: 
- `/sensor/sonar/sonoptix/data` (`sensor_msgs/Image`, Raw Sonar Data)
- `/sensor/sonar/sonoptix/param/range` (`std_msgs/Int32`, Current range parameter)
- `/sensor/sonar/sonoptix/param/tx_mode` (`std_msgs/String`, Current tx_mode parameter)
- `/sensor/sonar/sonoptix/param/power_state` (`std_msgs/Bool`, Current power state)
- `/sensor/sonar/sonoptix/param/frame_id` (`std_msgs/String`, Current frame ID)

**Parameters**: The parameters can be updated while running the node.

| Name                 | Type    | Default            | Description                              |
|----------------------|---------|--------------------|------------------------------------------|
| `range`              | int     | 50                 | Sonar range in meters [3-200]            |
| `ip`                 | str     | "192.168.0.203"    | IP address of the sonar device           |
| `tx_mode`            | str     | "auto"             | The transmit mode of the transceiver [`auto`, `hf`, `lf`, `lflr`] |
| `power_state`        | bool    | `True`             | The power state of the transceiver       |
| `topic`              | str     | `/sensor/sonar/sonoptix/data` | Topic to publish sonar frames            |
| `frame_id`           | str     | `echo`             |  TF frame ID                             |

**Run Example**:
```bash
ros2 run sonoptix_ros2 echo
```

---

### `echo_imager`

**Description**: Converts Echo sonar data into a polar image and publishes it.

**Subscribed Topics**: `/sensor/sonar/sonoptix/compressed` or `/sensor/sonar/sonoptix/data`

**Published Topics**: 
- `/sensor/sonar/sonoptix/image` (`sensor_msgs/Image`, Processed polar image)
- `/sensor/sonar/sonoptix/param/contrast` (`std_msgs/Float32`, Current contrast parameter)

**Parameters**: The parameters can be updated while running the node.

| Name           | Type   | Default              | Description                               |
|----------------|--------|----------------------|-------------------------------------------|
| `data_topic`   | str    | `/sensor/sonar/sonoptix/compressed`    | Input topic for sonar data (raw or compressed) |
| `image_topic`  | str    | `/sensor/sonar/sonoptix/image`   | Output topic for visualized image         |
| `contrast`     | float  | `10.0`               | Contrast multiplier for visualization     |
| `bag_file`     | str    |                      | Optional path to an input ros2 bag file with sonar data |
| `video_file`   | str    |                      | Optional path to an output mp4 video file |

**Run Example**:
```bash
ros2 run sonoptix_ros2 echo_imager --ros-args -p contrast:=10.0 -p data_topic:=/sensor/sonar/sonoptix/data
```

Note that when reading from a bag file, the node will export the output to `echo_sonar.mp4` video file by default. 

---

## License

This package is licensed under the MIT License. See the top of each file or [LICENSE](LICENSE) for details.