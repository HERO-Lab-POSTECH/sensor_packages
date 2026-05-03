# oculus_sonar Refactor Design — Phase A → B → C

| | |
|---|---|
| Date | 2026-05-03 |
| Author | luckkim123 |
| Status | Draft (awaiting user approval) |
| Scope | `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar` |
| Repo | `HERO-Lab-POSTECH/sensor_packages` (main 브랜치 보호 적용) |

---

## 1. Summary

PKRC `oculus_sonar` 패키지(약 1,500 LOC C++)를 **5개 축**(고도화·최적화·사용성·정리·단순화) 기준으로 개선합니다. PKRC `refactor-workflow.md`의 phase 분류·4축 추적(branch/commit/CHANGELOG/PR)에 맞춰 **Phase A(cleanup) → B(extract) → C(substitute) 순으로 4 PR**로 분할 진행합니다. 각 phase 머지 후 다음 phase의 baseline이 되며, "고도화" 축은 본 spec 범위 외 — Phase A~C 정리 완료 후 별도 라운드(Task #6)에서 재분석합니다.

## 2. Goals

- `OculusDriver` 단일 401줄 클래스의 SRP 위반 해소.
- ping 핫패스(`pingToSonarImage`)의 매 프레임 동적 grow 제거 → m3000d 30Hz 시 CPU 절감.
- README/launch/실제 코드 간 불일치 해소 (m3000d 누락, 존재하지 않는 launch 파일 안내, `/raw` vs `/raw_data` 등).
- `fan_imager.launch.py` 흡수 → 단일 진입점 + intra-process composition.
- 향후 phase·고도화 작업의 회귀 검증 인프라(`scripts/regression_oculus.sh`) 자산화.

## 3. Non-Goals

- 외부 `liboculus`(vendored, 별도 리포) API 변경.
- `oculus_sonar_msgs` 메시지 정의 변경.
- 새 기능 추가 (자동 재연결, dual-sonar, GPU 가속 등) — Task #6 별도 라운드.
- ROS2 distro 변경 (Humble 유지).
- 실제 센서 하드웨어 검증 — bag replay로 대체. 하드웨어 검증은 사용자가 별도 라운드로 수행.

## 4. Background — Audit Findings

`ros2-reviewer` agent로 audit한 결과 (2026-05-03):

| # | 항목 | 위치 | 영향 축 | 처리 phase |
|---|------|------|---------|----------|
| 1 | `pingToSonarImage` byte-by-byte push_back, reserve 없음 | `ping_to_sonar_image.hpp:128-141` | 최적화 | C-1 |
| 2 | `OculusDriver` 401 LOC, `init()` 120줄, SRP 위반 | `oculus_driver_component.cpp` | 단순화 | B-1 |
| 3 | README 안내 launch 파일이 실제로 존재 안 함, m3000d 누락, `/raw` 토픽명 오류 | `README.md:11-12,17-39,79,92` | 사용성 | A-1 |
| 4 | CMake 위험 include + plugins.xml fan_imager 누락 + register_nodes 중복 | `CMakeLists.txt:48-52,119-138`, `plugins.xml` | 정리 | A-3 |
| 5 | 깨진 hex log, `cv_ptr_` 미사용, dead yaml key, `frame_id` 하드코딩 | `oculus_driver_component.cpp:213-219`, `.hpp:81`, `config/m1200d_params.yaml`, `publishing_data_rx.cpp:41` | 정리 | A-4, A-6 |
| 6 | `publishParameters()` 매 ping 호출 (9 publisher) | `oculus_driver_component.cpp` | 최적화 | C-2 |
| 7 | `RCLCPP_INFO` 매 ping 콘솔 스팸 | `ping_to_sonar_image.hpp:45` | 정리 | A-5 |

## 5. Constraints

- **Branch 보호**: `main` 직접 push 금지, PR + 1 approval + linear history (`git-workflow.md` 참조). 각 phase = 1 PR, squash merge.
- **데이터 안전**: bag/db3/metadata.yaml 백업 없이 수정 금지 (`mv`만 허용).
- **PKRC 코딩 한도**: 함수 ≤50줄, 파라미터 ≤4개, 클래스 메서드 ≤20개, 중첩 ≤3 (`coding-style.md`).
- **Git Identity**: `luckkim123 / luckkim123@gmail.com`.
- **속성 금지**: 코드·커밋·문서에 AI/Claude 언급 금지.

## 6. Phase Roadmap (Overview)

| Phase | Scope | Branch | 회귀 의무 | PR 수 | 추정 LOC Δ |
|-------|-------|--------|-----------|-------|-----------|
| **A** | Cleanup (정리 + 사용성) | `refactor/phase-a-oculus-cleanup` | X | 1 | -200 |
| **B-1** | `OculusDriver` 4-way split | `refactor/phase-b1-driver-split` | O (bag replay byte-exact) | 1 | ±0 (재분배) |
| **B-2** | Launch 공통 헬퍼 추출 | `refactor/phase-b2-launch-helper` | O (launch 동작 동등) | 1 | -200 |
| **C** | Hot-path zero-copy + param echo throttle | `refactor/phase-c-hotpath-optimize` | O (byte-exact + CPU 측정) | 1 | -50 |

**Archive 태그** (Phase A 시작 시): `archive/oculus-pre-refactor-2026-05-03 @ <baseline-sha>`

## 7. Phase A — Cleanup

### A-1. README 재작성 (영어 유지)

**현재 상태**:
- m750d/m1200d만 안내 (m3000d 누락) — `README.md:11-12,17-39`
- `m750d.launch.py` / `m1200d.launch.py` 사용 안내 — 실제로 존재하지 않음
- `publish_fan_image`, `num_beams` 파라미터 안내 — `declare_parameter`되지 않음
- `/raw` 토픽 안내 — 실제는 `/raw_data`

**결과물**:
- m750d/m1200d/m3000d 3개 모델 동등 안내
- 단일 진입점: `ros2 launch oculus_sonar oculus.launch.py sonar_model:=<model> with_fan:=<bool>`
- 실제 토픽명·실제 파라미터 목록 반영

### A-2. Launch 단일 진입점화

- `launch/oculus.launch.py` (287줄) + `launch/fan_imager.launch.py` (178줄) → `oculus.launch.py` 하나로 통합.
- `with_fan:=true` 인자 → `OculusFanImager` 컴포넌트를 같은 `ComposableNodeContainer`에 로드.
- `OculusDriver` → `OculusFanImager` 메시지 전달이 **intra-process zero-copy**가 됨 (Phase C 효과 일부 선취).
- `launch/fan_imager.launch.py` 완전 삭제 (다운스트림 import 0건 사전 확인 완료).

### A-3. plugins.xml + CMakeLists.txt 정리

- `plugins.xml`에 `<class type="oculus_sonar::OculusFanImager" base_class_type="rclcpp::Node">` 추가.
- `CMakeLists.txt:119-138`에서 `OculusDriver`/`OculusFanImager`에 `rclcpp_components_register_nodes` + `register_node` 둘 다 호출되는 중복 → composition + executable이 한 번에 등록되도록 단일화.
- `CMakeLists.txt:48-52` 위험 include 삭제:
  - `../liboculus/include` (상대경로, 워크스페이스 토폴로지 가정)
  - `${CMAKE_BINARY_DIR}/marine_acoustic_msgs/...` (빌드 디렉토리 헤더)
  - 두 경로는 `ament_target_dependencies(... liboculus marine_acoustic_msgs)`가 자동 처리.

### A-4. Dead Code 제거

| 위치 | 삭제 항목 |
|------|----------|
| `oculus_driver_component.cpp:213-219` | 깨진 hex log 블록 (`"0x%s"` 포맷에 `""` placeholder) |
| `oculus_driver_component.hpp:81` | `cv_ptr_` 멤버 (`OculusFanImager`에서만 쓰는데 정의된 적 없음) |
| `config/m750d_params.yaml:22` | 주석 처리된 `num_beams: 1` (이미 죽은 줄, 주석째 삭제) |
| `config/m1200d_params.yaml:22`, `config/m3000d_params.yaml:26` | 활성 `num_beams: 1` (driver line 49 주석에 "무시됨" 명시) |

### A-5. 핫패스 로그 강등

- `ping_to_sonar_image.hpp:45` `RCLCPP_INFO` → `RCLCPP_INFO_ONCE`.
- m3000d 30Hz 운용 시 콘솔 스팸 1,800 msg/min → 1 msg.

### A-6. `frame_id` 하드코딩 수정

- `publishing_data_rx.cpp:41` `frame_id = "oculus"` → `PublishingDataRx` 생성자 인자로 주입.
- `OculusDriver`의 `frame_id_` 파라미터 (이미 declare됨) 값을 생성자에 전달.

### A-7. 매직 넘버 → 명명 상수

- Phase A 초반 1-pass 식별: `grep -nP '\b\d+\.\d+\b|\b[1-9]\d{2,}\b' src/ include/`.
- 확정 추출 후보:
  - 모델별 FOV (m750d=70°, m1200d=70°, m3000d=130°)
  - 기본 baudrate, 기본 IP/port
  - 단순 인덱스(`0`, `1`, `-1`)는 추출 제외.
- 모든 상수는 `include/oculus_sonar/constants.hpp` 단일 파일에 모음.

### A 검증

```bash
# 빌드만 필수 — 회귀 측정 X (Phase A 정의)
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
# (선택) bag replay로 launch 동작 확인 — 하드웨어 없이도 가능
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true &
ros2 bag play /workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1
ros2 topic list | grep oculus  # 기존 토픽 전수 존재
```

## 8. Phase B — Extract

### B-1. `OculusDriver` 4-way Split

**현재 구조** (`src/oculus_driver_component.cpp` 401 LOC, `init()` 36-156 = 120줄):

```
class OculusDriver : public rclcpp::Node {
  // 11 declare_parameter
  // 4 setter helper (setRange, setGainPercent, ...)
  // 1 parametersCallback (159-232 = 73줄)
  // 1 convertDataSizeString
  // 9 std_msgs param echo publisher
  // 3 raw/sonar/status publisher
  // 1 publishParameters()  ← 매 ping 호출
  // io_service 스레드, ServiceClient, data_callback, status_callback
};
```

**목표 구조**:

```
src/
├── oculus_driver_component.cpp     (orchestrator, ~80줄, executable+plugin)
├── oculus_driver_config.cpp         (~120줄)
├── oculus_driver_publishers.cpp     (~100줄)
└── oculus_connection_manager.cpp    (~120줄)

include/oculus_sonar/
├── oculus_driver_component.hpp
├── oculus_driver_config.hpp
├── oculus_driver_publishers.hpp
└── oculus_connection_manager.hpp
```

| 클래스 | 책임 | 외부 인터페이스 |
|--------|------|----------------|
| `OculusDriverConfig` | 11 declare_parameter, parametersCallback, convertDataSizeString, 4 setter helper | `get_config() -> SonarConfig`, `on_change(callback)` |
| `OculusDriverPublishers` | 9 param echo + raw/sonar/status publisher 소유, `pingToSonarImage` 호출, `publishParameters()` | `publish_ping(ping)`, `publish_status(status)`, `publish_parameters()` |
| `OculusConnectionManager` | io_service 스레드, `oculus::ServiceClient`, data_callback / status_callback 디스패치 | `start()`, `stop()`, `on_data(callback)`, `on_status(callback)` |
| `OculusDriver` (orchestrator) | 위 3개 인스턴스 소유·연결, ROS2 lifecycle | rclcpp::Node |

**검증** (회귀 의무):
```bash
# Baseline: main HEAD 빌드 → 60s bag replay → 토픽 dump
git checkout main && colcon build --packages-select oculus_sonar
./scripts/regression_oculus.sh main
# Candidate
git checkout refactor/phase-b1-driver-split && colcon build
./scripts/regression_oculus.sh candidate
# 비교: 발행 토픽 8개의 메시지 시퀀스 byte-exact 일치 (B-1은 동작 100% 보존)
diff /tmp/oculus_regression/main_*.yaml /tmp/oculus_regression/candidate_*.yaml
```

### B-2. Launch 공통 헬퍼

- `launch/_oculus_common.py` 신규:
  - `sonar_model_to_config_path(model: str) -> Path`
  - `make_sonar_container(model, with_fan, **overrides) -> ComposableNodeContainer`
  - 토픽 네임스페이스 헬퍼 (`/sensor/sonar/oculus/{model}/...`)
- `launch/oculus.launch.py` 287줄 → ~70줄 (인자 declare + `make_sonar_container` 호출만).
- **branch 분기 시점**: B-1 머지 후 (B-1의 새 클래스 구조에 맞춰 launch 작성).
- **검증**: Phase A와 동일 — launch 실행 + 토픽 발행. Python launch라 회귀 위험 매우 낮음.

## 9. Phase C — Substitute

### C-1. `pingToSonarImage` Zero-Copy

**현재** (`include/oculus_sonar/ping_to_sonar_image.hpp:128-141`, `reserve()` 없음):

```cpp
for (unsigned int r = 0; r < num_ranges; r++) {
  for (unsigned int b = 0; b < num_bearings; b++) {
    if (ping.dataSize() == 1) {
      const uint8_t data = ping.image().at_uint8(b, r);
      sonar_image.intensities.push_back(data & 0xFF);
    } else if (ping.dataSize() == 2) { /* 2바이트 분해 push */ }
    else if (ping.dataSize() == 4) { /* 4바이트 분해 push */ }
  }
}
```

**목표** (전제 조건 만족 시):

```cpp
const size_t total_bytes = num_ranges * num_bearings * ping.dataSize();
sonar_image.intensities.resize(total_bytes);
std::memcpy(sonar_image.intensities.data(),
            ping.image().raw_ptr(),  // ← liboculus API 확인 후 결정
            total_bytes);
// is_bigendian = false (현재 코드와 동일) → little-endian 직접 복사
```

**전제 검증** (Phase C 진입 시 첫 단계):

1. `liboculus/SimplePingResult.h`에서 `image()`가 raw byte ptr (e.g., `data()`, `raw_ptr()`)을 노출하는지 확인.
2. 노출하지 않으면 → liboculus에 추가 PR 또는 `at_uint8/16/32` 인덱싱을 유지하되 `reserve()` + `[]` 직접 인덱싱으로 1차 최적화.
3. storage 순서 확인: 현재 루프가 `for (r) for (b)` 순서로 push → memcpy 1회로 가능하려면 liboculus storage가 `[r][b]` row-major여야 함. `[b][r]`이면 stride-aware copy 또는 transpose helper 필요.

**측정 지표**:
| 지표 | 베이스라인 | 후보 | 측정 도구 |
|------|----------|------|----------|
| ping callback 평균 ms | TBD | TBD | `RCLCPP_DEBUG` 인스트루먼트 |
| `oculus_driver` %CPU | TBD | TBD | `top -p $(pgrep)` 60s 평균 |
| 발행 메시지 byte-exact | (n/a) | 100% 일치 | `diff` |

### C-2. Parameter Echo Throttle + Latched QoS

**현재**: `OculusDriverPublishers::publishParameters()` (B-1에서 분리된 메서드)가 매 ping 콜백마다 호출 → 9 publisher × ping freq.

**목표**:
- `wall_timer(1.0s, std::bind(&Publishers::publish_parameters, ...))`로 분리.
- 9개 std_msgs publisher의 QoS:
  ```cpp
  rclcpp::QoS qos(1);
  qos.transient_local();  // latched: 새 subscriber에 마지막 값 즉시 전달
  ```
- `OculusDriverConfig::on_change` 콜백 안에서 변경 직후 1회 즉시 publish.

**호환성 사전 검증** (Phase C 진입 시):
```bash
# 9개 param echo topic을 다른 노드가 구독하는지 grep
grep -rn "/sensor/sonar/oculus/.*/[a-z_]*\b" /workspace/ros2_ws/src --include="*.cpp" --include="*.py" \
  | grep -E "create_subscription|Subscribe" | grep -v oculus_sonar
```
- 구독자가 `RELIABLE + VOLATILE`만 받으면 `transient_local`과 incompatible — 그 경우 QoS 호환 가능한 profile로 조정.

**효과**:
- m3000d 30Hz 운용 시 9 pub × 30Hz = **270 msg/s → 9 msg/s** (97% 감소).
- 새 subscriber가 `ros2 topic echo --once` 한 번에 마지막 값 즉시 수신.

## 10. Regression Infrastructure

`scripts/regression_oculus.sh` (Phase B-1에서 신규 작성, B-2/C 모두 재사용):

```bash
#!/usr/bin/env bash
# Usage: regression_oculus.sh <branch_label>
set -euo pipefail
LABEL=$1
BAG=/workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1
OUT=/tmp/oculus_regression
mkdir -p $OUT

TOPICS=(
  /sensor/sonar/oculus/m3000d/sonar
  /sensor/sonar/oculus/m3000d/raw_data
  /sensor/sonar/oculus/m3000d/status
  # ... 9 param echo topic
)

ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d &
LAUNCH_PID=$!
sleep 3
ros2 bag play $BAG --rate 1.0 &
BAG_PID=$!

for t in "${TOPICS[@]}"; do
  timeout 60 ros2 topic echo --csv --no-arr $t > $OUT/${LABEL}_${t//\//_}.csv &
done
wait $BAG_PID
kill $LAUNCH_PID 2>/dev/null || true
```

**규칙**:
- Baseline은 매번 `main` HEAD에서 다시 빌드 (이전 phase 누적 효과 검증).
- 측정 결과 `/tmp/oculus_regression/`은 commit 금지 (재생성 가능).
- 시각 비교 plot 이미지는 PR 본문에만 attach, repo commit 금지.

## 11. Risks & Mitigations

| Risk | 발생 phase | Mitigation |
|------|----------|-----------|
| `liboculus.image().raw_ptr()` API 부재 | C-1 | `at_uint8/16/32` + `reserve()` + `[]` 직접 인덱싱으로 fallback (zero-copy 효과 일부 유지) |
| 9 param echo topic의 다운스트림 incompatible QoS | C-2 | 사전 grep으로 구독자 검증, incompatible 시 `BEST_EFFORT + VOLATILE` 유지하고 throttle만 적용 |
| Phase B-1 split 후 회귀 byte-mismatch | B-1 | 비결정적 요소(타임스탬프) 제외하고 비교, 차이 발견 시 split 경계 재검토 |
| Launch 통합 시 ROS_DOMAIN_ID·namespace 변경 | A-2 | 기존 노드 이름·토픽 이름 그대로 유지, 인자 시그니처만 추가(기존 인자 100% 호환) |
| bag 데이터의 m3000d 메시지 schema가 현 코드와 불일치 | B-1, C-1 | bag 녹화 시점의 oculus_sonar_msgs 버전 확인 (`ros2 bag info`), 불일치 시 m750d/m1200d bag로 fallback |

## 12. Verification Matrix

| Phase | colcon build | bag replay 토픽 발행 | byte-exact 일치 | CPU 측정 | 다운스트림 호환 |
|-------|:----:|:----:|:----:|:----:|:----:|
| A | 필수 | 권장 | n/a | n/a | n/a |
| B-1 | 필수 | 필수 | 필수 | n/a | n/a |
| B-2 | 필수 | 필수 | n/a | n/a | n/a |
| C-1 | 필수 | 필수 | 필수 | 필수 | n/a |
| C-2 | 필수 | 필수 | n/a | 필수 | 필수 |

## 13. Future (Out of Scope, Task #6)

Phase C 머지 완료 후 별도 라운드에서 재분석할 "고도화" 후보:

- **자동 재연결**: 소나 disconnect 시 backoff 재시도 (현재는 단발 connect 실패 시 노드 종료).
- **Dual-sonar 동시 운용**: m750d + m3000d 동시 launch 지원 (B-1의 `OculusConnectionManager` 기반에 multi-instance 추가).
- **메타 확장**: 온도·heading·voltage 등 status 메타데이터를 별도 토픽으로 노출.
- **GPU 가속 polar→cartesian**: `polar_to_cartesian.cpp`(259줄) CUDA/OpenCL 백엔드.
- **Lifecycle 노드화**: `rclcpp_lifecycle::LifecycleNode`로 전환하여 외부 orchestrator(launch sequencer)와 협조.

## 14. Open Questions

- **Q-B1**: `OculusDriverConfig`를 `OculusDriver` 노드의 멤버로 둘지(composition), 별도 helper 클래스로 둘지(독립). 후자가 단위 테스트 용이 — implementation 단계에서 결정.
- **Q-C1**: `liboculus`에 `raw_ptr()` 추가 PR을 우리가 낼 수 있는 권한이 있는지 (`/workspace/ros2_ws/src/sensor_packages/oculus_ros2/liboculus/` origin 확인 필요). 권한 없으면 fallback 전략(`reserve()` + `[]` 인덱싱)으로 진행.

## Appendix A — CHANGELOG.md Draft (4 Phase)

```markdown
## [Unreleased] — Phase A: oculus_sonar cleanup (refactor)

### Changed
- README.md — m3000d 추가, `oculus.launch.py` 단일 진입점, 실제 토픽명 반영
- launch/oculus.launch.py — `with_fan:=true` 옵션 흡수
- plugins.xml — OculusFanImager 추가
- CMakeLists.txt — register_nodes/register_node 중복 제거, 위험 include 삭제

### Removed
- launch/fan_imager.launch.py (with_fan:=true로 대체)
- 깨진 hex log, cv_ptr_ 미사용 멤버, num_beams: 1 dead key

### Fixed
- publishing_data_rx.cpp:41 frame_id 하드코딩 → 생성자 주입
- ping_to_sonar_image.hpp:45 매 ping log → INFO_ONCE

### Verification
- colcon build PASS
- (선택) bag replay 토픽 발행 확인 — 하드웨어 검증 별도 라운드

## [Unreleased] — Phase B-1: OculusDriver 4-way split (refactor)

### Changed
- src/oculus_driver_component.cpp 401줄 → ~80줄 (orchestrator만 남김)

### Added
- src/oculus_driver_config.cpp + include header
- src/oculus_driver_publishers.cpp + include header
- src/oculus_connection_manager.cpp + include header
- scripts/regression_oculus.sh (Phase B/C 재사용 인프라)

### Verification
- colcon build PASS
- bag replay 60s → 발행 토픽 byte-exact 일치

## [Unreleased] — Phase B-2: launch 공통 헬퍼 (refactor)

### Changed
- launch/oculus.launch.py 287줄 → ~70줄

### Added
- launch/_oculus_common.py — sonar_model 매핑, 컨테이너 빌더

### Verification
- colcon build PASS, launch 실행 + 토픽 발행 동등

## [Unreleased] — Phase C: hot-path optimize (refactor)

### Changed
- include/oculus_sonar/ping_to_sonar_image.hpp — push_back 루프 → resize+memcpy
- src/oculus_driver_publishers.cpp — publishParameters() 매 ping → 1Hz timer
- 9 param echo publisher QoS → TRANSIENT_LOCAL

### Verification
- bag replay byte-exact 일치
- m3000d 30Hz 60s: ping callback ms TBD→TBD, %CPU TBD→TBD
- 9 param echo Hz: ~30Hz → 1Hz (-97%)
```

## Appendix B — PR Body Template

```markdown
## Summary
- <한 줄 요약>

## Changes
- <bullet 3-5개>

## Verification
- [ ] colcon build PASS
- [ ] (Phase B/C) Regression baseline vs candidate 측정 완료
- [ ] (Phase B/C) byte-exact 일치 + CPU 측정
- [ ] CHANGELOG.md 갱신
- [ ] 다음 phase scope 한 줄 명시

## Next Phase
- <다음 phase의 scope 한 줄>
```
