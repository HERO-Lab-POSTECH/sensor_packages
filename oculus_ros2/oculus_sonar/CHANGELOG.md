# Changelog

All notable changes to `oculus_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Phase A: oculus_sonar cleanup (refactor)

### Changed
- `README.md` — m3000d 추가, `oculus.launch.py` 단일 진입점화, 실제 토픽명(`/raw_data`)·실제 파라미터(13 launch arg 표) 반영. 306줄 → 108줄.
- `plugins.xml` — `OculusFanImager` class 등록 추가 (composition 가능). TODO `ReprocessOculusRawData` 주석 삭제.
- `CMakeLists.txt` — 위험 전역 include path 5줄(`../liboculus/include`, `${CMAKE_BINARY_DIR}/marine_acoustic_msgs/...`, `${CMAKE_INSTALL_PREFIX}/include` 등) 제거. `target_include_directories($<BUILD_INTERFACE:>)` generator expression으로 타겟별 적용 (전역 오염 없음). TODO 주석·주석 처리된 reprocess executable 블록 정리. 223줄 → 183줄.
- `include/oculus_sonar/sonar_config.hpp` — 기존 `SonarConstants` 네임스페이스에 `M{750D,1200D,370S,3000D}_PART_NUMBERS` 배열, `DEFAULT_SONAR_IP`, `DEFAULT_RANGE_M`, `DEFAULT_GAIN_PERCENT`, `DEFAULT_GAMMA` 추가.
- `src/oculus_driver_component.cpp` — 20개 하드코딩 part_num 리터럴을 `SonarConstants::*_PART_NUMBERS` 배열 + `std::find` lambda로 교체. `declare_parameter` default(range/gain/gamma)도 `SonarConstants::DEFAULT_*` 참조.
- `include/oculus_sonar/oculus_driver_component.hpp` + `src/oculus_driver_component.cpp` — `data_rx_` 멤버를 값 타입에서 `std::unique_ptr<PublishingDataRx>`로 변경 (frame_id 주입을 위한 lazy init 필요).

### Added
- `CHANGELOG.md` (본 파일) — 패키지 변경 이력 추적.

### Removed
- `launch/fan_imager.launch.py` — `oculus.launch.py with_fan:=true`가 동일 기능 + intra-process composition 이득 제공 (다운스트림 import 0건 사전 확인). 178줄.
- `src/oculus_driver_component.cpp` 깨진 hex log 블록 (`"0x%s"` 포맷에 빈 placeholder만 전달, 13줄).
- `include/oculus_sonar/oculus_fan_imager.hpp:81` `cv_bridge::CvImagePtr cv_ptr_` 미사용 멤버 (선언만 있고 read/write 0건). cv_bridge 라이브러리 자체는 다른 사용(`cv_bridge::CvImage`, `cv_bridge::Exception`)이 있어 include 유지.
- `config/m750d_params.yaml`, `config/m1200d_params.yaml`, `config/m3000d_params.yaml` — `num_beams` 키 (driver line 49 주석에 "무시됨" 명시).
- `launch/oculus.launch.py` — `num_beams` `DeclareLaunchArgument` + `param_overrides` 분기 + 헤더 docstring 행. yaml 키 제거에 따른 잔존물 정리 (CLI override 시 미선언 파라미터 reject 방지).
- `README.md:46` — `num_beams` 표 행 (yaml/launch 양쪽에서 제거됨).

### Fixed
- `src/publishing_data_rx.cpp:41` — `frame_id` 하드코딩(`"oculus"`) 제거 → 생성자 주입으로 `OculusDriver`의 `frame_id_` 파라미터 반영. `PublishingDataRx` 생성자 시그니처에 `const std::string &frame_id` 추가.
- `include/oculus_sonar/ping_to_sonar_image.hpp:45` — 매 ping `RCLCPP_INFO` → `RCLCPP_INFO_ONCE` (m3000d 30Hz 시 분당 1,800 msg 콘솔 스팸 → 1 msg).

### Verification
- `colcon build --packages-select oculus_sonar` PASS.
- `ros2 component types | grep oculus` → `OculusDriver` + `OculusFanImager` 둘 다 노출 확인.
- 회귀 측정 X (Phase A 정의: 알고리즘 영향 0%).

### Notes
- 실제 센서 하드웨어 검증은 별도 라운드로 미룸 (사용자 결정).
- Archive 태그: `archive/oculus-pre-refactor-2026-05-03 @ a30bbb4` (Phase A 시작 시 baseline 보존).
- LSP(clangd) 진단 false positive 다수 발생 — `rclcpp/rclcpp.hpp not found` 등은 ament가 자동 주입하는 include path를 clangd가 인식 못 함. 실제 colcon build는 모두 PASS.
- 다음 phase: `refactor/phase-b1-driver-split` — `OculusDriver` 4-way split + `scripts/regression_oculus.sh` 회귀 인프라 신규.
