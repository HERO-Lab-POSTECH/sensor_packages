# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased] — Pre-experiment Fix H-6 (fix)

### Added
- `livox_ros2/livox_driver/package.xml` — declare `<depend>livox_sdk</depend>` so colcon parallel builds livox_sdk before livox_driver. Without this, `find_library(LIVOX_LIDAR_SDK_LIBRARY)` fails on clean checkout. Cherry-picked from humble-devel `8dc03f6`.

### Changed
- `livox_ros2/livox_driver/.gitignore` — un-ignore `package.xml`.

### Verification
- `colcon build --packages-up-to livox_driver` PASS from clean state (livox_sdk built first in 0.07s, livox_driver in 7.80s).

---

## [Unreleased] — Phase P4a: QoS helper module (refactor)

### Added
- `include/livox_driver/qos.hpp` — workspace QoS 3-tier helper (SENSOR/RELIABLE/LATCHED) per spec §2.4

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P3: TF naming standard (refactor)

### Changed
- Default frame_id `livox_frame` → `livox_link` (REP-105 + Autoware)
  - `livox_driver/src/livox_driver.cpp`
  - `livox_driver/src/lddc.cpp` (imu_msg)
  - `livox_driver/launch_ROS2/{rviz_,msg_}{MID360,HAP}_launch.py` + `rviz_mixed.py`

### Notes
- ROS1 launch files (`launch_ROS1/`) and `.rviz` config files not changed — ROS1 scope.
- boat_description URDF provides legacy `livox_frame` alias for old bag compatibility.

### Verification
- colcon build PASS

## [1.2.4]
### Fixed
- Optimize framing performance

## [1.2.3]
### Fixed
- Optimize framing logic and reduce CPU usage
- Fixed some known issues

## [1.2.1]
### Fixed
- Fix offset time error regarding CustomMsg format message publishment.

## [1.2.0]
### Added
- Revise the frame segmentation logic.
- (Notice!!!) Add Timestamp to each point in Livox pointcloud2 (PointXYZRTLT) format. The PointXYZRTL format has been updated to PointXYZRTLT format. Compatibility needs to be considered.
### Fixed
- Improve support for gPTP and GPS synchronizations.

--- 
## [1.1.3]
### Fixed
- Improve performance when running in ROS2 Humble.

--- 
## [1.1.2]
### Changed
- Change publish frequency range to [0.5Hz, 10 Hz].
### Fixed
- Fix a high CPU-usage problem.

--- 
## [1.1.1]
### Added
- Offer valid line-number info in the point cloud data of MID-360 Lidar.
- Enable IMU by default.
### Changed
- Update the README slightly.

--- 
## [1.0.0]
### Added
- Support Mid-360 Lidar.
- Support for Ubuntu 22.04 ROS2 humble.
- Support multi-topic fuction, the suffix of the topic name corresponds to the ip address of each Lidar. 
### Changed
- Remove the embedded SDK.
- Constraint: Livox ROS Driver 2 for ROS2 does not support message passing with PCL native data types.
### Fixed
- Fix IMU packet loss.
- Fix some conflicts with livox ros driver.
- Fixed HAP Lidar publishing PointCloud2 and CustomMsg format point clouds with no line number.
