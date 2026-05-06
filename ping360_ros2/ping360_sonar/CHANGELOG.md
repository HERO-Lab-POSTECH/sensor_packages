# Changelog

All notable changes to `ping360_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.0.2] — Post-Audit Fix PR-M (fix, 6th audit)

### Fixed
- `src/ping360_node.cpp`: image publish timer migrated from `create_wall_timer` to `rclcpp::create_timer(this, get_clock(), ...)` so the timer honors `use_sim_time` during bag replay (High, 6th audit).

### Changed
- `launch/ping360.launch.py`: expose `frame_id` as a launch argument (default `ping360_link`); previously it was a node-internal hardcoded default with no override (Medium, 6th audit).

### Verification
- colcon build PASS (ping360_sonar)

## [Unreleased] — Phase P5a: Launch arg standardization (refactor)

### Changed
- `ping360.launch.py`: 헤더 docstring 표준 적용 (spec §2.5.3)
- `ping360.launch.py`: dead `import os` 제거

### Verification
- colcon build PASS
- ros2 launch --show-args: 6 args 표시

---

## [Unreleased] — Phase P4a: QoS helper module (refactor)

### Added
- `include/ping360_sonar/qos.hpp` — workspace QoS 3-tier helper (SENSOR/RELIABLE/LATCHED) per spec §2.4

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P3: TF naming standard (refactor)

### Changed
- Default `frame` parameter: `"sonar"` → `"ping360_link"` (REP-105 + Autoware)
  - `src/ping360_node.cpp` (C++ driver)
  - `src/ping360.py` (Python driver)

### Notes
- `ping360_image_to_pointcloud.py` already used `'ping360_link'` — no change.

### Verification
- colcon build PASS
