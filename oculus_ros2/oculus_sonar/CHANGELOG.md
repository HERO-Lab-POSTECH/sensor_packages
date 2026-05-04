# Changelog

All notable changes to `oculus_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Phase B-2: Launch helper extraction (refactor)

> Master design: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-2
> Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-2-launch-helper.md`
> Verification: structural snapshot regression PASS + functional --show-args + with_fan path

### Changed
- `launch/oculus.launch.py` 323 LOC → 120 LOC. `launch_setup` body delegates container assembly to `_oculus_common.make_sonar_container`. Override-collection logic moved to two file-private helpers (`_collect_driver_overrides`, `_collect_fan_imager_overrides`) since they need `LaunchConfiguration` access.
- 90-line module docstring → 4-line pointer to README. 46-line trailing comment block deleted (duplicate of Phase A README content).

### Added
- `launch/_oculus_common.py` (105 LOC) — 3 pure helpers (no LaunchConfiguration access, unit-testable):
  - `sonar_model_to_config_path(model: str) -> str` — config YAML resolution + model whitelist FileNotFoundError
  - `topic_namespace(model: str) -> str` — `/sensor/sonar/oculus/{model}` prefix
  - `make_sonar_container(model, with_fan, driver_overrides, fan_imager_overrides) -> ComposableNodeContainer`

### Verification
- colcon build PASS (Release)
- **Layer 1 — structural snapshot**: baseline (`adc79cc` post-B-1 main HEAD) vs candidate (B-2 final) — 4 snapshot files byte-identical
  - `ros2 component types` → unchanged (still 2 oculus_sonar components)
  - `ros2 param dump /oculus/m750d` → all parameter defaults unchanged
  - `ros2 topic list` + `topic info --verbose` → all topic names · message types · QoS unchanged
- **Layer 2 — functional sanity**: `ros2 launch --show-args` returns 12 args (sonar_model, with_fan, ip_address, range, gain, gamma, ping_rate, freq_mode, data_size, apply_colormap, colormap, qos_reliability); `with_fan:=true` actual launch runs 10s without crash and publishes both `/sensor/sonar/oculus/m750d/sonar` AND `/sensor/sonar/oculus/m750d/fan_image`

### Notes
- Helper module location is INSIDE `oculus_sonar/launch/` (not a cross-package `sensor_packages/launch_utils/`). When ping360_sonar refactor introduces a second consumer with overlapping helper needs, that's the trigger to revisit cross-package extraction. Until then, the underscore-prefix internal-only convention keeps coupling minimal.
- `_collect_driver_overrides` and `_collect_fan_imager_overrides` deliberately stay in the launch file (not in `_oculus_common.py`) because they call `LaunchConfiguration(...).perform(context)` — keeping them in the file means `_oculus_common` remains pure and unit-testable without a launch context.
- Phase B-1's `scripts/regression_oculus.sh` reused unchanged. Per spec §10 the regression infra cost amortizes across B-1, B-2, and C.
- Regression script DDS discovery race (B-1 implementer hit STARTUP_WAIT_S=10 manual workaround) recurred in B-2. Resolved with `export ROS_DOMAIN_ID=99` BEFORE sourcing ROS + `STARTUP_WAIT_S=12` + isolated domain (avoids cross-talk with stray `ros2 bag play --loop` on default domain 0). Documented in memory `reference_oculus_regression_dds_gotcha.md`.

## [Unreleased] — Phase B-1: OculusDriver 4-way split + regression infra (refactor)

> Master design: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-1
> Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-1-driver-split.md`
> Verification: structural snapshot regression (4 dimensions, hardware-free) — PASS

### Changed
- `src/oculus_driver_component.cpp` 395 → 105 LOC (orchestrator only — wires 3 helpers; status callback inline lambda 36 LOC가 init() 본문에 남아있어 plan 목표 ~80 LOC 초과)
- `include/oculus_sonar/oculus_driver_component.hpp` 161 → 73 LOC
- `CMakeLists.txt` — `register_nodes` 호출 2개 제거 (`register_node`만 유지). 컴포넌트 중복 노출 해소 (Phase A spec §7 A-3 미완 흡수, D1)

### Added
- `include/oculus_sonar/oculus_driver_config.hpp` + `src/oculus_driver_config.cpp` (70+167 LOC) — 11 declare_parameter, parameterCallback, 3 setter helper, updateSonarConfig, convertDataSizeString. `rclcpp::Node*` non-owning 참조 (D2: 독립 helper)
- `include/oculus_sonar/oculus_driver_publishers.hpp` + `src/oculus_driver_publishers.cpp` (90+110 LOC) — 4 sensor + 9 param echo publisher, `publishParameters()`, `sonarToImage()` 헬퍼, 템플릿 `publishPing<PingT>`
- `include/oculus_sonar/oculus_connection_manager.hpp` + `src/oculus_connection_manager.cpp` (69+77 LOC) — `IoServiceThread`, `PublishingDataRx`, `StatusRx`, ping/status 콜백 디스패치, 연결 lifecycle
- `scripts/regression_oculus.sh` — 4 차원 구조적 snapshot 회귀 인프라 (component types / param dump / topic list / per-topic info), B-2/C 재사용 자산

### Verification
- colcon build PASS (Release)
- **Layer 1 — structural snapshot**: baseline (`80b2c5e` main HEAD, post-D1 적용) vs candidate (B-1 final) — 4개 snapshot 파일 모두 byte-identical
  - `ros2 component types | grep oculus` → 각 노드 1회씩만 노출 (D1 적용 후, 이전 2회 → 1회)
  - `ros2 param dump /oculus/m750d` → 모든 parameter 동일 default
  - `ros2 topic list | grep oculus` + `ros2 topic info --verbose` → 모든 publisher 토픽 이름·메시지 타입·QoS 동일
- **Layer 2 — driver longevity smoke test**: UCRC m750d bag (`/workspace/data/7_ucrc_watertank/.../m750d-range15-tilt45-v1`) 60s 병행 재생 → driver process alive + launch.log에 SIGSEGV/`what():`/`std::terminate`/`boost::asio` error 0건

### Notes
- 본 PR은 byte-exact ping output 검증을 포함하지 않음. 드라이버는 `liboculus`를 통해 UDP 소켓 raw bytes를 받으므로 ROS bag을 driver INTO 재생할 방법이 없음. Hardware-in-loop 검증은 별도 라운드 (Phase C 또는 운용 검증).
- Longevity smoke test는 structural snapshot이 못 잡는 runtime crash 종류(io_service thread race, callback dispatch SEGV, double-stop)를 잡기 위한 두 번째 회귀 layer. UCRC bag을 사용한 이유는 sonar_3d_reconstruction Phase B-1 회귀와 같은 디렉토리 패밀리라서 미래 cross-comparison에 유리하기 때문.
- `init()` 함수 자체는 70 LOC (PKRC ≤50 한도 초과). status callback inline lambda(36 LOC)가 init() 본문에 위치한 결과. lambda 헬퍼 추출은 Phase B-2 또는 별도 patch 후보.
- Phase B-2 (launch helper 추출)는 본 PR 머지 후 `refactor/phase-b2-launch-helper` 브랜치에서 진행. B-1의 회귀 인프라(`scripts/regression_oculus.sh`)를 그대로 재사용.
- 잔존 리스크 3건 (Phase A에서 이월) 중 #2 (CMakeLists `liboculus` include path INSTALL_INTERFACE 미포함)은 본 PR과 무관, Phase B-2 또는 별도 patch에서 처리.

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
