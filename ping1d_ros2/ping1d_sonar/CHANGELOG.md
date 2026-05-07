# Changelog

All notable changes to `ping1d_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.0.3] — 2026-05-07 (patch)

### Fixed
- `range_pub_component.py`: dummy publisher now emits to `/sensor/sonar/ping1d/range_test` (and registers as node `range_publisher_test`) instead of colliding with the real driver topic `/sensor/sonar/ping1d/range`. If a user accidentally launched both `ros2 run ping1d_sonar ping1d_node` and `ros2 run ping1d_sonar range_node` at the same time, downstream subscribers received a mix of real (~0.1s cycle) and dummy (1s cycle, fixed 1.5 m) messages. Now the two streams are isolated and the dummy is clearly named for what it is — a smoke-test producer for downstream nodes when no Ping1d device is connected.
- Docstring added explaining the dummy nature and topic separation.

### Notes
- No schema or QoS changes. Both publishers still use `qos_profile_sensor_data` (BEST_EFFORT, KEEP_LAST 5).
- Downstream consumers that explicitly subscribed to `/sensor/sonar/ping1d/range_test` (none known in the workspace) would need to update; the real-driver topic name is unchanged.

### Verification
- grep `/sensor/sonar/ping1d/range\b` (publishers only) → exactly 1 hit (`ping1d_component.py`).

## [0.0.2] — Post-Audit Fix PR-Q (fix, 8th audit)

### Added
- `launch/ping_sonar.launch.py`: expose `use_sim_time` launch argument (default `false`); forwarded to the `ping1d_node` parameters so the driver honors `/clock` during bag replay (High Q-1, 8th audit).

### Fixed
- `setup.py`: bump `version` from `0.0.0` to `0.0.2` to track `package.xml` (PR-M raised `package.xml` to 0.0.1 but left `setup.py` stale; this PR resyncs and bumps both to 0.0.2 — Medium Q-2, 8th audit).

### Verification
- colcon build PASS (ping1d_sonar)

## [0.0.1] — Post-Audit Fix PR-M (fix, 6th audit)

### Changed
- `launch/ping_sonar.launch.py`: expose `frame_id` as a launch argument (default `ping1d_link`) and forward to the `ping1d_node` parameters. Previously the node had a working `frame_id` parameter but the launch file did not expose it, forcing manual edits (Medium, 6th audit).

### Verification
- colcon build PASS (ping1d_sonar)

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
