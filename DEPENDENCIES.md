# External Dependencies and Package Sources

This repository contains ROS2 (Humble) sensor packages for marine robotics applications.
Some packages have been modified from their original sources to fit our requirements.

## Modified Packages

### ping360_ros2
- **Original Repository**: https://github.com/CentraleNantesRobotics/ping360_sonar.git
- **Original Branch**: ros2
- **Modifications**:
  - Added device parameter to launch file for flexible port configuration
  - Added UDP connection parameters (udp_address, udp_port)
  - Enhanced launch file with baudrate and connection_type parameters
  - Default device changed from `/dev/ttyUSB0` to `/dev/ping360` (configurable)

### ping1d_ros2
- **Original Repository**: https://github.com/tasada038/ping_sonar_ros.git
- **Original Branch**: master
- **Modifications**:
  - Added device parameter to launch file
  - Added comprehensive launch parameters (speed_of_sound, interval, gain, etc.)
  - Added use_rviz parameter to control RViz launch
  - Fixed port parameter handling in component
  - Default device changed from `/dev/ttyUSB0` to `/dev/ping` (configurable)

### fast_lio
- **Original Repository**: https://github.com/Ericsii/FAST_LIO_ROS2.git
- **Original Branch**: (detached HEAD)
- **Modifications**: None (used as-is)

## Unmodified External Packages

### Livox Packages
- **livox_driver**
  - Source: https://github.com/Livox-SDK/livox_ros_driver2.git
  - Branch: master
  - Version: As of 2024-08

- **livox_sdk**
  - Source: https://github.com/Livox-SDK/Livox-SDK2.git
  - Branch: master
  - Version: As of 2024-08

### Oculus Sonar Dependencies
- **apl_msgs**
  - Source: https://gitlab.com/apl-ocean-engineering/apl_msgs.git
  - Branch: ros2

- **marine_msgs**
  - Source: https://github.com/apl-ocean-engineering/marine_msgs.git
  - Branch: ros2

- **liboculus**
  - Source: https://github.com/apl-ocean-engineering/liboculus.git
  - Branch: dev/hybrid_ros1_ros2

- **g3log**
  - Source: https://github.com/KjellKod/g3log.git
  - Branch: master

## Locally Developed Packages

### oculus_sonar
- Custom ROS2 driver for Blueprint Subsea Oculus imaging sonar
- Developed specifically for this project

### oculus_sonar_msgs
- Message definitions for Oculus sonar
- Developed specifically for this project

### ping360_sonar_msgs
- Message definitions for Ping360 scanning sonar
- Developed specifically for this project

## Package Structure

```
ros2_ws/src/
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

## Branch Structure

- `humble-devel`: ROS2 Humble (Ubuntu 22.04) development branch
- `noetic-devel`: ROS1 Noetic (Ubuntu 20.04) development branch (if available)

## Notes

1. All packages are configured for ROS2 Humble
2. Some packages may require specific udev rules for device access
3. Default device paths assume udev rules are configured:
   - Ping360: `/dev/ping360`
   - Ping1D: `/dev/ping`
   - Oculus: Auto-discovery via network

## License Information

Each package maintains its original license. Please refer to individual package directories for specific license files.