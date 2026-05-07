# Changelog

All notable changes to `ping360_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.0.4] — 2026-05-07 (patch)

### Added
- `/sensor/sonar/ping360/param/range` (std_msgs/Float32, LATCHED) — alias of the existing `/sensor/sonar/ping360/param/range_max`. Republishes the configured `range_max` value at every parameter-publish tick so generic downstream consumers that derive `range_topic` from the sonar topic root (oculus convention `<prefix>/param/range`) match without ping360-specific special-casing.

### Fixed
- `sonar_3d_reconstruction.scripts.3d_mapper_node` previously auto-derived `range_topic = <sonar_topic_root>/param/range`, which silently produced `/sensor/sonar/ping360/param/range` — a topic that did not exist. The mapper's `range_sub` therefore never received a value when launched with `sonar:=ping360`, even though `range_max` (the same data) was being published. The new `range_pub` closes that gap without requiring the mapper to know about ping360 specifically.

### Notes
- New publisher uses LATCHED QoS (RELIABLE + TRANSIENT_LOCAL + KeepLast(1)) so a late-joining mapper still receives the most recent range value. The original `/param/range_max` keeps its sensor QoS for backward compatibility with existing logging/recording subscribers.
- Header `ping360_node.h` adds the `range_pub` member; `ping360_node.cpp` creates it next to `range_max_pub` and republishes `range_max_msg` on the alias inside `publishParameters()` (one extra publish per tick, identical payload).

### Verification
- grep `/sensor/sonar/ping360/param/range` (publishers) → 2 hits (`range_max` and the new `range` alias).
- colcon build PASS (ping360_sonar).

## [2.0.3] — Post-Audit Fix PR-Q (fix, 8th audit)

### Added
- `launch/ping360.launch.py`: expose `use_sim_time` launch argument (default `false`); forwarded to the `ping360` node parameters so the driver honors `/clock` during bag replay (High Q-1, 8th audit).

### Verification
- colcon build PASS (ping360_sonar)

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
