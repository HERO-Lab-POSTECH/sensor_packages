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

## Package Sources & Modifications

### Modified Packages

#### ping360_ros2
- **Original Repository**: https://github.com/CentraleNantesRobotics/ping360_sonar.git (branch: ros2)
- **Modifications**:
  - Added device parameter to launch file for flexible port configuration
  - Added UDP connection parameters (udp_address, udp_port)
  - Enhanced launch file with baudrate and connection_type parameters
  - Default device changed from `/dev/ttyUSB0` to `/dev/ping360` (configurable)

#### ping1d_ros2
- **Original Repository**: https://github.com/tasada038/ping_sonar_ros.git (branch: master)
- **Modifications**:
  - Added device parameter to launch file
  - Added comprehensive launch parameters (speed_of_sound, interval, gain, etc.)
  - Added use_rviz parameter to control RViz launch
  - Fixed port parameter handling in component
  - Default device changed from `/dev/ttyUSB0` to `/dev/ping` (configurable)

### Unmodified External Packages

#### Livox Packages
- **fast_lio**: https://github.com/Ericsii/FAST_LIO_ROS2.git
- **livox_driver**: https://github.com/Livox-SDK/livox_ros_driver2.git (branch: master)
- **livox_sdk**: https://github.com/Livox-SDK/Livox-SDK2.git (branch: master)

#### Oculus Sonar Dependencies
- **apl_msgs**: https://gitlab.com/apl-ocean-engineering/apl_msgs.git (branch: ros2)
- **marine_msgs**: https://github.com/apl-ocean-engineering/marine_msgs.git (branch: ros2)
- **liboculus**: https://github.com/apl-ocean-engineering/liboculus.git (branch: dev/hybrid_ros1_ros2)
- **g3log**: https://github.com/KjellKod/g3log.git (branch: master)

### Locally Developed Packages
- **oculus_sonar**: Custom ROS2 driver for Blueprint Subsea Oculus imaging sonar
- **oculus_sonar_msgs**: Message definitions for Oculus sonar
- **ping360_sonar_msgs**: Message definitions for Ping360 scanning sonar

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

### Prerequisites
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

### Building
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

### Default Device Paths
The packages assume the following default device paths (can be overridden via launch parameters):
- **Ping360**: `/dev/ping360` (or `/dev/ttyUSB0`)
- **Ping1D**: `/dev/ping` (or `/dev/ttyUSB0`)
- **Oculus**: Auto-discovery via network

### Udev Rules (Optional)
For consistent device naming, create udev rules:

```bash
# /etc/udev/rules.d/99-marine-sensors.rules
SUBSYSTEM=="tty", ATTRS{serial}=="PING360_SERIAL", SYMLINK+="ping360"
SUBSYSTEM=="tty", ATTRS{serial}=="PING1D_SERIAL", SYMLINK+="ping"

sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Launch Parameters

#### Ping360
- `device`: Serial device path (default: `/dev/ping360`)
- `baudrate`: Serial baudrate (default: 115200)
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

3. **Livox SDK issues**: Make sure to source the SDK
   ```bash
   source ~/ros2_ws/install/setup.bash
   ```

## Contributing

Contributions are welcome! Please submit pull requests to the appropriate branch:
- `humble-devel` for ROS2 Humble changes
- `noetic-devel` for ROS1 Noetic changes

## License

Each package maintains its original license. Please refer to individual package directories for specific license files:
- Ping360/Ping1D: MIT License
- Livox packages: See individual LICENSE files
- Oculus packages: Various open source licenses
- Local developments: MIT License

## Contact

HERO Lab, POSTECH
- GitHub: https://github.com/HERO-Lab-POSTECH
- Website: https://hero.postech.ac.kr/

## Acknowledgments

This repository includes packages from various contributors:
- CentraleNantesRobotics for Ping360 driver
- Blue Robotics for Ping sonar libraries
- Livox-SDK team for LiDAR drivers
- APL Ocean Engineering for marine message definitions
- Fast-LIO team for SLAM implementation