# Changelog

All notable changes to `sonoptix_ros2` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
