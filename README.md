# Marine Robotics Sensor Packages

ROS2 (Humble) sensor driver packages for marine robotics applications, including sonar systems and LiDAR SLAM.

## Overview

This repository contains a collection of sensor driver packages optimized for marine robotics applications:

- **Sonar Systems**: Oculus imaging sonar, Ping360 scanning sonar, Ping1D altimeter
- **LiDAR Systems**: Livox drivers and FAST-LIO SLAM
- **Supporting Libraries**: Message definitions and communication libraries

## Branch Structure

- `humble-devel`: ROS2 Humble packages (Ubuntu 22.04)
- `noetic-devel`: ROS1 Noetic packages (Ubuntu 20.04) - *if available*

## Package List

### Sonar Packages
- `oculus_ros2/`: Blueprint Subsea Oculus imaging sonar
- `ping360_ros2/`: BlueRobotics Ping360 scanning sonar  
- `ping1d_ros2/`: BlueRobotics Ping1D altimeter/echosounder

### LiDAR Packages
- `livox_ros2/fast_lio/`: Real-time LiDAR-Inertial SLAM
- `livox_ros2/livox_driver/`: Livox LiDAR ROS2 driver
- `livox_ros2/livox_sdk/`: Livox SDK

### Support Libraries
- `oculus_ros2/liboculus/`: Oculus communication library
- `oculus_ros2/marine_msgs/`: Marine sensor message definitions
- `oculus_ros2/g3log/`: Logging library

## Installation

### Prerequisites
```bash
# ROS2 Humble
sudo apt update
sudo apt install ros-humble-desktop

# Build tools
sudo apt install python3-colcon-common-extensions
sudo apt install ros-humble-image-transport ros-humble-cv-bridge
```

### Building
```bash
# Clone repository
git clone -b humble-devel https://github.com/HERO-Lab-POSTECH/sensor_packages.git
cd sensor_packages

# Create workspace
mkdir -p ~/ros2_ws/src
cp -r * ~/ros2_ws/src/

# Build
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash
```

## Usage Examples

### Sonar Systems
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

### Udev Rules (Optional)
For consistent device naming, create udev rules:

```bash
# /etc/udev/rules.d/99-marine-sensors.rules
SUBSYSTEM=="tty", ATTRS{serial}=="PING360_SERIAL", SYMLINK+="ping360"
SUBSYSTEM=="tty", ATTRS{serial}=="PING1D_SERIAL", SYMLINK+="ping"

sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Documentation

- See [DEPENDENCIES.md](DEPENDENCIES.md) for package sources and modifications
- Individual packages may have their own README files

## License

Each package maintains its original license. See individual package directories for details.

## Contributing

Contributions are welcome! Please submit pull requests to the appropriate branch:
- `humble-devel` for ROS2 Humble changes
- `noetic-devel` for ROS1 Noetic changes

## Contact

HERO Lab, POSTECH
- GitHub: https://github.com/HERO-Lab-POSTECH