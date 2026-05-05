# Changelog

All notable changes to `sonoptix_ros2` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
