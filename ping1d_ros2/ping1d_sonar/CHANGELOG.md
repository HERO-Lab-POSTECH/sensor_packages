# Changelog

All notable changes to `ping1d_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
