# Marine Robotics Sensor Packages

A collection of ROS2 (Humble) sensor driver packages for marine robotics applications, including sonar systems and LiDAR SLAM.

## Overview

This repository contains sensor driver packages optimized for marine robotics applications:

- **Sonar Systems**: Oculus imaging sonar, Sonoptix imaging sonar, Ping360 scanning sonar, Ping1D altimeter
- **LiDAR Systems**: Livox driver and FAST-LIO SLAM
- **Support Libraries**: Message definitions and communication libraries

## Package Sources and Modifications

### Modified Packages

#### ping360_ros2
- **Original Repository**: https://github.com/CentraleNantesRobotics/ping360_sonar.git (branch: ros2)
- **License**: MIT License
- **Modifications**:
  - Added device parameter for flexible port configuration in launch files
  - Added UDP connection parameters (udp_address, udp_port)
  - Improved launch file with baudrate and connection_type parameters
  - Changed default device from `/dev/ttyUSB0` to `/dev/ping360` (configurable)
  - Added BEST_EFFORT QoS for all publishers

#### ping1d_ros2
- **Original Repository**: https://github.com/tasada038/ping_sonar_ros.git (branch: master)
- **License**: Apache License 2.0
- **Modifications**:
  - Added device parameter to launch file
  - Added comprehensive launch parameters (speed_of_sound, interval, gain, etc.)
  - Added use_rviz parameter for RViz launch control
  - Fixed port parameter handling in component
  - Changed default device from `/dev/ttyUSB0` to `/dev/ping` (configurable)
  - Added frame_id parameter for TF frame configuration
  - Added BEST_EFFORT QoS for all publishers

### Unmodified External Packages

#### Livox Packages
- **fast_lio**:
  - Original: https://github.com/Ericsii/FAST_LIO_ROS2.git
  - License: GPL-2.0
  - Description: Real-time LiDAR-inertial odometry and mapping
- **livox_driver**:
  - Original: https://github.com/Livox-SDK/livox_ros_driver2.git (branch: master)
  - License: Livox SDK License
  - Description: Livox LiDAR ROS2 driver
- **livox_sdk**:
  - Original: https://github.com/Livox-SDK/Livox-SDK2.git (branch: master)
  - License: Livox SDK License
  - Description: Livox LiDAR communication SDK

#### Oculus Sonar Dependencies
- **apl_msgs**:
  - Original: https://gitlab.com/apl-ocean-engineering/apl_msgs.git (branch: ros2)
  - License: BSD-3-Clause
  - Description: APL ocean engineering message definitions
- **marine_msgs**:
  - Original: https://github.com/apl-ocean-engineering/marine_msgs.git (branch: ros2)
  - License: BSD-3-Clause
  - Description: Marine sensor message definitions
- **liboculus**:
  - Original: https://github.com/apl-ocean-engineering/liboculus.git (branch: dev/hybrid_ros1_ros2)
  - License: BSD License
  - Description: Blueprint Subsea Oculus sonar communication library
- **g3log**:
  - Original: https://github.com/KjellKod/g3log.git (branch: master)
  - License: Unlicense
  - Description: Asynchronous logging library

### Locally Developed Packages
- **oculus_sonar**: Custom ROS2 driver for Blueprint Subsea Oculus imaging sonar
- **oculus_sonar_msgs**: Oculus sonar message definitions
- **ping360_sonar_msgs**: Ping360 scanning sonar message definitions
- **sonoptix_ros2**: Sonoptix Echo imaging sonar ROS2 driver (MIT License)
  - TCP/IP communication based real-time sonar data reception
  - Image compression and streaming support
  - Efficient data transfer via ROS2 image_transport

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
├── ping360_ros2/         # Ping360 scanning sonar
│   ├── ping360_sonar/
│   └── ping360_sonar_msgs/
└── sonoptix_ros2/        # Sonoptix Echo imaging sonar driver
```

## Installation

### Prerequisites
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

### Building
```bash
# Clone repository
git clone https://github.com/HERO-Lab-POSTECH/sensor_packages.git
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

### Sonar Systems
```bash
# Oculus imaging sonar (unified launch for all models)
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true

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

All sensor topics follow a consistent naming convention:
```
/sensor/<sensor_type>/<manufacturer>_<model>/<data_type>
```

### QoS Policy

All sensor packages use a unified QoS profile for consistency:
- **Reliability**: BEST_EFFORT
- **History**: KEEP_LAST
- **Depth**: 5-10

This ensures proper communication between publishers and subscribers across different machines and during ROS bag recording.

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
- **Oculus M750d/M1200d/M3000d** (unified topic structure):
  - Sonar: `/sensor/sonar/oculus/{model}/sonar`
  - Image: `/sensor/sonar/oculus/{model}/image`
  - Fan Image: `/sensor/sonar/oculus/{model}/fan_image` (with_fan:=true)
  - Metadata: `/sensor/sonar/oculus/{model}/metadata`
- **Ping360**:
  - Echo: `/sensor/sonar/ping360/echo`
  - Scan: `/sensor/sonar/ping360/scan`
  - Image: `/sensor/sonar/ping360/image`
- **Ping1D**:
  - Range: `/sensor/sonar/ping1d/range`
  - Data: `/sensor/sonar/ping1d/data`
  - Parameters: `/sensor/sonar/ping1d/param/*`
- **Sonoptix Echo**:
  - Data: `/sensor/sonar/sonoptix/data`
  - Compressed: `/sensor/sonar/sonoptix/compressed`
  - Image: `/sensor/sonar/sonoptix/image`
  - Parameters: `/sensor/sonar/sonoptix/param/*`

### SLAM Output Topics
- **FAST-LIO**:
  - Odometry: `/localization/fast_lio/odometry`
  - Registered Cloud (body frame): `/localization/fast_lio/points_body`
  - Path (debug): `/fast_lio/debug/path`

## Device Configuration

### Default Device Paths
Packages use the following default device paths (configurable via launch parameters):
- **Ping360**: `/dev/ping360` (or `/dev/ttyUSB0`)
- **Ping1D**: `/dev/ping` (or `/dev/ttyUSB0`)
- **Oculus**: Auto-discovery via network

### Udev Rules (Optional)
Create udev rules for consistent device naming:

```bash
# /etc/udev/rules.d/99-marine-sensors.rules
SUBSYSTEM=="tty", ATTRS{serial}=="PING360_SERIAL", SYMLINK+="ping360"
SUBSYSTEM=="tty", ATTRS{serial}=="PING1D_SERIAL", SYMLINK+="ping"

sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Launch Parameters

#### Ping360
- `device`: Serial device path (default: `/dev/ping360`)
- `baudrate`: Serial communication speed (default: 115200)
- `connection_type`: 'serial' or 'udp' (default: serial)
- `udp_address`: UDP address for network connection
- `udp_port`: UDP port for network connection

#### Ping1D
- `device`: Serial device path (default: `/dev/ping`)
- `speed_of_sound`: Speed of sound in mm/s (default: 1450000)
- `interval`: Ping interval in ms (default: 100)
- `gain`: Gain setting 0-6 (default: 1)
- `scan_start`: Start distance in mm (default: 100)
- `scan_length`: Scan length in mm (default: 3000)
- `use_rviz`: Launch RViz visualization (default: true)
- `frame_id`: TF frame ID (default: ping1d_link)

#### Sonoptix Echo
- `ip`: Sonar IP address (default: 192.168.0.203)
- `range`: Sonar range in meters (default: 12)
- `tx_mode`: Transmit mode 'auto' or 'manual' (default: auto)
- `power_state`: Initial power state (default: True)
- `data_topic`: Raw sonar data topic (default: /sensor/sonar/sonoptix/data)
- `compressed_topic`: Compressed sonar data topic (default: /sensor/sonar/sonoptix/compressed)
- `frame_id`: Sonar data frame ID (default: echo)
- `compression_level`: Compression level 1-9 (default: 1)
- `reliability`: QoS reliability setting 'best_effort' or 'reliable' (default: best_effort)

**Note**: echo.launch.py automatically runs image_transport node to convert raw images to compressed format.

## Network Configuration

### Sonoptix Echo Sonar Network Setup

Proper network configuration is required for Sonoptix Echo sonar:

#### Default Network Settings
- **Sensor IP**: `192.168.0.203` (default)
- **Computer IP**: `192.168.0.12` (recommended)
- **Network Subnet**: `192.168.0.x/24`

#### Network Configuration Commands
```bash
# Set computer to same network subnet as sensor
sudo ip addr del 192.168.2.42/24 dev enx3c18a0127d4c
sudo ip addr add 192.168.0.12/24 dev enx3c18a0127d4c

# Test connection
ping 192.168.0.203
curl http://192.168.0.203:8000/api/v1/status
```

#### Troubleshooting
- **Connection refused**: Sensor not fully booted or using different port
- **No route to host**: Network subnet mismatch (sensor and computer on different subnets)
- **Check sensor IP**: Verify sensor IP in ARP table
  ```bash
  arp -a | grep enx3c18a0127d4c
  ```

## Troubleshooting

### Common Issues

1. **Device not found**: Check USB permissions
   ```bash
   sudo chmod 666 /dev/ttyUSB*
   ```

2. **Build errors**: Ensure all dependencies are installed
   ```bash
   rosdep install --from-paths src --ignore-src -y
   ```

3. **Livox SDK issues**: Ensure SDK is properly sourced
   ```bash
   source ~/ros2_ws/install/setup.bash
   ```

4. **rosidl_typesupport_c error**: Install ROS2 message generation packages
   ```bash
   sudo apt install ros-humble-rosidl-typesupport-c ros-humble-rosidl-default-generators
   ```

## Contributing

Contributions are welcome! Please submit Pull Requests against `main`.

## License

Each package maintains its original license. See individual package directories for specific license files:

### External Package Licenses
- **ping360_ros2**: MIT License (CentraleNantesRobotics)
- **ping1d_ros2**: Apache License 2.0 (tasada038)
- **fast_lio**: GPL-2.0 License (Ericsii)
- **livox_driver/livox_sdk**: Livox SDK License Agreement
- **liboculus**: BSD License (APL Ocean Engineering)
- **apl_msgs/marine_msgs**: BSD-3-Clause License
- **g3log**: Unlicense (Public Domain)

### Locally Developed Package Licenses
- **oculus_sonar**: MIT License
- **oculus_sonar_msgs**: MIT License
- **ping360_sonar_msgs**: MIT License
- **sonoptix_ros2**: MIT License

## Contact

HERO Lab, POSTECH
- GitHub: https://github.com/HERO-Lab-POSTECH
- Website: https://hero.postech.ac.kr/

## Acknowledgments

This repository includes packages from the following contributors:
- **CentraleNantesRobotics**: Ping360 driver
- **Blue Robotics**: Ping sonar libraries
- **Livox-SDK Team**: LiDAR driver
- **APL Ocean Engineering**: Marine message definitions
- **Fast-LIO Team (HKU Mars Lab)**: SLAM implementation
- **tasada038**: Ping1D ROS2 driver
- **KjellKod**: g3log logging library

## Original Sources

Original code sources and detailed information:
- Ping360: https://github.com/CentraleNantesRobotics/ping360_sonar
- Ping1D: https://github.com/tasada038/ping_sonar_ros
- FAST-LIO: https://github.com/hku-mars/FAST_LIO
- Livox ROS Driver: https://github.com/Livox-SDK/livox_ros_driver2
- liboculus: https://github.com/apl-ocean-engineering/liboculus
- Marine Messages: https://github.com/apl-ocean-engineering/marine_msgs
