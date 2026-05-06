# Changelog

All notable changes to `sonoptix_ros2` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.0.3] — Post-Audit Fix PR-M (fix, 6th audit)

### Fixed
- `sonoptix_ros2/echo.py`, `sonoptix_ros2/echo_imager.py`: replace inline `rclpy.qos.qos_profile_sensor_data` with the package `SENSOR_QOS` helper from `sonoptix_ros2.qos`. Aligns with the spec §2.4 QoS-helper convention used by all other PKRC packages (High, 6th audit).

### Changed
- `launch/echo.launch.py`: expose `frame_id` as a launch argument (default `sonoptix_link`); previously it was hardcoded inside the node (Medium, 6th audit).

### Verification
- colcon build PASS (sonoptix_ros2)

## [Unreleased] — Phase P5a: Launch arg standardization (refactor)

### Changed
- `echo.launch.py`, `echo_decompress.launch.py`: 헤더 docstring 표준 적용 (spec §2.5.3) — args 0개 명시 (모든 설정 hardcoded)
- MIT License 헤더는 한 줄 노트로 단축

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P4a: QoS helper module (refactor)

### Added
- `sonoptix_ros2/qos.py` — workspace QoS 3-tier helper (SENSOR/RELIABLE/LATCHED) per spec §2.4

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P3: TF naming standard (refactor)

### Changed
- Default `frame_id`: `"echo"` → `"sonoptix_link"` (REP-105 + Autoware)
  - `sonoptix_ros2/echo.py`

### Verification
- colcon build PASS
