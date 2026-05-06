# Changelog

All notable changes to `oculus_sonar` will be documented in this file.

## [Unreleased] — Post-Audit Fix PR-H (fix)

### Changed
- `src/oculus_fan_imager.cpp`: subscriber QoS default reordered. Previously `rclcpp::QoS(10)` (RELIABLE) was constructed first and only replaced when `qos_reliability:=best_effort`. Now `SensorDataQoS()` is the default and only replaced when `qos_reliability:=reliable`. Matches the upstream `/sensor/sonar/oculus/<model>/sonar` publisher (BEST_EFFORT). The declared parameter default is already `best_effort`, so runtime behavior is unchanged when params come from declared defaults — but a manual `unset` path is now safe (4th audit High).

### Verification
- colcon build PASS

## [Unreleased] — Phase P6: Config structure (refactor)

### Changed
- 3 config yaml (m750d/m1200d/m3000d_params.yaml): 헤더 docstring + 카테고리 separator + [Static]/[Dynamic] 태그 + 단위 표기 (spec §2.7)
- 카테고리 분리: MODEL / NETWORK + FRAMES / IMAGING / FLAGS

### Verification
- yaml.safe_load PASS (3 파일)
- colcon build PASS (cosmetic 변경 — 값 변경 0건)

---

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Phase P5a: Launch arg standardization (refactor)

### Changed
- `oculus.launch.py`: 헤더 docstring 표준 적용 (spec §2.5.3) — 12 launch args 모두 명시
  - `qos_reliability` arg 유지 (oculus_fan_imager.cpp의 subscriber QoS 노드 파라미터로 사용 중 — spec §2.4 helper 적용은 후속 phase)

### Verification
- colcon build PASS
- ros2 launch --show-args: 12 args 표시

---

## [Unreleased] — Phase P4a: QoS helper module (refactor)

### Added
- `include/oculus_sonar/qos.hpp` — workspace QoS 3-tier helper (SENSOR/RELIABLE/LATCHED) per spec §2.4

### Verification
- colcon build PASS

---

## [Unreleased] — Phase P3: TF naming standard (refactor)

### Changed
- Driver default `frame_id`: `"oculus"` → `"sonar_link"` (`src/oculus_driver_config.cpp`)
- yaml config files (`config/m750d_params.yaml`, `m1200d_params.yaml`, `m3000d_params.yaml`) already consistent with `"sonar_link"` — no change.

### Verification
- colcon build PASS

## [Unreleased] — Phase C-2: parameter echo throttle + latched QoS (refactor)

### Changed
- `OculusDriverPublishers::initialize()` — 9 `/param/*` publishers now use
  `rclcpp::QoS(1).transient_local()` (RELIABLE + TRANSIENT_LOCAL +
  KEEP_LAST(1)) instead of `SensorDataQoS`. Late-joining subscribers
  receive the most recent value via DDS replay.
- `OculusDriverPublishers::publishPing<>()` — removed `publishParameters()`
  call. Per-ping wasted work for 9 std_msgs publishers eliminated.
- `OculusDriver::init()` — `on_change` listener now also calls
  `publishers_->republishParameters()` so parameter updates land in
  the latched cache within ~1 ms of the parameter service callback.

### Added
- `OculusDriverPublishers::republishParameters()` — public wrapper around
  the existing private `publishParameters()` for use by the
  `OculusDriverConfig::on_change` listener.
- `OculusDriverPublishers::param_publish_timer_` — `wall_timer(1s)` member
  that drives steady-state `publishParameters()` invocations at ~1 Hz.

### Verification
- colcon build PASS (Release + BUILD_TESTING=ON, clean rebuild)
- 8/8 existing C-1 gtest fixtures PASS (no algorithm regression)
- Structural snapshot diff: only `04_topic_info.txt` differs, only on the
  9 `/param/*` topics, only the expected `Reliability QoS` flip
  (`BEST_EFFORT → RELIABLE`) and `Durability QoS` flip
  (`VOLATILE → TRANSIENT_LOCAL`). Other 3 dimensions byte-identical.
- `ros2 topic hz /sensor/sonar/oculus/m750d/param/range` over 30 s:
  average ≈ 1.0 Hz (acceptance: 0.95–1.05).
- `ros2 topic echo --once /sensor/sonar/oculus/m750d/param/range`:
  returns immediately on freshly launched node (latched seed PASS).

### Performance (measurement-only, no gate)
- Throughput model: m3000d 30 Hz × 9 publishers = 270 msg/s →
  steady-state 9 msg/s (97% reduction).
- C-1 instrumentation `[c1-perf] pings=N window=W p50=Xms p99=Yms`
  automatically captures the publishPing latency reduction once
  hardware is available. Bag-replay does not exercise publishPing.

### Notes
- No new gtest fixtures. `wall_timer` is a deterministic ROS 2 primitive;
  the structural snapshot already detects QoS regressions.
- Downstream compatibility: only `3d_mapper_node.py`
  (`/sensor/sonar/.../param/range`) was found subscribing to oculus
  param topics. Its VOLATILE durability and dynamic reliability are
  compatible with the new TRANSIENT_LOCAL + RELIABLE publisher.
- Single-threaded executor assumption preserved. If `MultiThreadedExecutor`
  is adopted later, `publishParameters` will need a mutex to serialize
  the timer callback, the on_change listener, and the seed call.

## [Unreleased] — Phase C-1: pingToSonarImage zero-copy refactor (refactor)

> Master design: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §9 C-1
> Spec: `docs/superpowers/specs/2026-05-04-oculus-sonar-phase-c-1-zero-copy-design.md`
> Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-c-1-zero-copy.md`
> Verification: 8 byte-exact gtest fixtures + 4-dimension structural snapshot regression — PASS

### Changed
- `oculus_ros2/liboculus/include/liboculus/ImageData.h` — add 4 public `const noexcept` accessors (`data()`, `stride()`, `offset()`, `numBytes()`). Vendored library; no external `.git` (full edit rights confirmed).
- `oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp` — replace per-element `push_back` hot loop (lines 126-143, 18 LOC) with branch-hoisted `memcpy` path (~22 LOC). Two paths: fast (single memcpy when stride==row_bytes), stride (row-by-row memcpy when gain prefix present). 3-way `if (dataSize == 1/2/4)` branch removed — little-endian byte storage matches `is_bigendian=false` output.
- `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp` + `oculus_driver_publishers.cpp` — `publishPing` instrumented with `chrono::steady_clock` p50/p99 sliding-window stats. `RCLCPP_INFO_THROTTLE(1000ms, ...)` keeps log overhead negligible.
- `oculus_ros2/oculus_sonar/CMakeLists.txt` — replace stale commented-out test block with working `ament_add_gtest` block targeting `test/test_ping_to_sonar_image.cpp`. First gtest in `oculus_sonar` package.

### Added
- `oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp` (~270 LOC) — 8 byte-exact gtest fixtures: 3 fast-path × element size (1/2/4 bytes) + 3 stride-path × element size + 2 edge cases (zero ranges, zero beams). Frozen pre-C-1 implementation embedded inline as golden reference. Synthetic ping helper builds `OculusSimplePingResult` buffer with deterministic byte pattern.

### Verification
- colcon build PASS (Release, BUILD_TESTING=ON)
- **Layer 1 — colcon test**: 8/8 gtest fixtures PASS against current production code (proves byte-for-byte equivalence to pre-refactor implementation)
- **Layer 3 — structural snapshot**: baseline (pre-C-1, `4a8bca4`) vs candidate (post-refactor, `60e027c`) — 4 snapshot files (`01_component_types.txt`, `02_param_dump.yaml`, `03_topic_list.txt`, `04_topic_info.txt`) all byte-identical
- **Layer 4 — performance** (data-collection, NOT a fail gate per spec §7): instrumentation deployed; field measurements deferred to hardware availability

### Notes
- Performance measurement requires real sonar hardware (UDP input — bag-replay impossible). When hardware is available, run with `m3000d` at 30Hz for highest amplification target. Baseline comparison should use B-1 head (`adc79cc`) build, candidate should use C-1 head, both measured under identical conditions (same bag run / same target), 60s sample, 3 repetitions. Record p50/p99/CPU% in PR description.
- Phase C-2 (parameter echo throttle + latched QoS) gets a separate brainstorm round after C-1 merge. The C-1 instrumentation infrastructure in `publishPing` will inform how C-2 measures publisher load.
- Carry-forward (post-merge polish, non-blocking): (1) sort-every-ping in `recordLatencyAndLog` could move into throttle branch to reduce measurement perturbation; (2) thread-safety of `latency_window_` is single-threaded-executor assumed — comment to be added if MultiThreadedExecutor adoption planned.
- This is the FIRST PR that modifies vendored `liboculus`. CMakeLists.txt of `liboculus` does not need touching — header-only addition. Confirmed only `oculus_sonar` consumes `liboculus` headers in this workspace.
- Regression DDS gotcha: `regression_oculus.sh` requires `export ROS_DOMAIN_ID=99` BEFORE `source ROS` + `STARTUP_WAIT_S=12-15`. Same workaround as B-2 (memory `reference_oculus_regression_dds_gotcha.md`).

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
