# Changelog

All notable changes to `ping1d_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Post-Audit Fix PR-H (fix)

### Added
- `package.xml`: `<depend>` entries for `rclpy`, `sensor_msgs`, `std_msgs`, `rcl_interfaces` (4th audit Critical: previously declared as test-only deps; clean `rosdep install --from-paths src` would not pull them).

### Changed
- `ping1d_sonar/ping1d_component.py`: replaced inline `QoSProfile(BEST_EFFORT, KEEP_LAST, depth=10)` with the workspace-standard `SENSOR_QOS` (depth=5) imported from `ping1d_sonar.qos`. Aligns with §2.4 spec; matches `range_pub_component.py` pattern.

### Verification
- colcon build PASS

## [Unreleased] — Phase P5a: Launch arg standardization (refactor)

### Changed
- `ping_sonar.launch.py`: 헤더 docstring 표준 적용 (spec §2.5.3)
- `ping_sonar.launch.py`: dead imports 정리 (`IfCondition`, `PathJoinSubstitution`, `FindPackageShare`, `packages_name`)

### Verification
- colcon build PASS
- ros2 launch --show-args: 8 args 표시

---

## [Unreleased] — Phase P4a: QoS helper module (refactor)

### Added
- `ping1d_sonar/qos.py` (module-level) — workspace QoS 3-tier helper (SENSOR/RELIABLE/LATCHED) per spec §2.4

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P3: TF naming standard (refactor)

### Changed
- `range_pub_component.py`: hardcoded `"range_sensor"` → param `frame_id` (default `'ping1d_link'`)
  - Added `declare_parameter('frame_id', 'ping1d_link')` in `__init__`
  - `range_msg.header.frame_id` now reads from `self.frame_id`

### Verification
- colcon build PASS
