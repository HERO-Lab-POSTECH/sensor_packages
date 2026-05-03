# oculus_sonar Phase A: Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Spec `2026-05-03-oculus-sonar-refactor-design.md`의 Phase A(7개 cleanup 항목)을 적용하여 단일 PR로 main에 머지. 알고리즘 영향 0%, 회귀 의무 없음.

**Architecture:** Cleanup-only phase. 각 task가 독립적인 logical commit이고, PR 머지 시 squash. 각 task 끝에 `colcon build --packages-select oculus_sonar` PASS + (해당 시) smoke test + commit. TDD 형식 대신 "변경 → 빌드 검증 → smoke test → commit" 패턴 사용 (cleanup 작업의 본성에 맞춤).

**Tech Stack:** C++17, ROS2 Humble, ament_cmake, rclcpp, rclcpp_components, liboculus(vendored).

**Spec 참조:** `/workspace/ros2_ws/src/sensor_packages/docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md`

---

## File Map

### Modified (수정)
| 파일 | 책임 | Task |
|------|------|------|
| `include/oculus_sonar/sonar_config.hpp` | 기존 `SonarConstants` 네임스페이스에 모델별 part_num·기본 ip·default 값 추가 | T2 |
| `src/oculus_driver_component.cpp` | hex log 삭제, part_num hardcoded list → SonarConstants 참조 | T2, T5 |
| `include/oculus_sonar/oculus_driver_component.hpp` | `cv_ptr_` 미사용 멤버 삭제 | T5 |
| `include/oculus_sonar/publishing_data_rx.h` | 생성자에 `frame_id` 인자 추가 | T3 |
| `src/publishing_data_rx.cpp` | `frame_id` 하드코딩 제거, 멤버 변수로 보관 | T3 |
| `include/oculus_sonar/ping_to_sonar_image.hpp` | `RCLCPP_INFO` → `RCLCPP_INFO_ONCE` | T4 |
| `config/m750d_params.yaml` | 주석 처리된 `num_beams: 1` dead 라인 삭제 | T5 |
| `config/m1200d_params.yaml` | 활성 `num_beams: 1` 삭제 (driver line 49 주석에 "무시" 명시) | T5 |
| `config/m3000d_params.yaml` | 활성 `num_beams: 1` 삭제 | T5 |
| `plugins.xml` | `OculusFanImager` 등록 추가, `ReprocessOculusRawData` TODO 주석 삭제 | T6 |
| `CMakeLists.txt` | line 48-52 위험 include 삭제, TODO 주석 정리, 주석 처리된 reprocess executable 블록 삭제 | T6 |
| `README.md` | m3000d 추가, 단일 launch 진입점 안내, 실제 토픽명·실제 파라미터 반영 | T8 |
| `CHANGELOG.md` (신규 작성) | Phase A 항목 추가 | T9 |

### Deleted (삭제)
| 파일 | 사유 | Task |
|------|------|------|
| `launch/fan_imager.launch.py` | `oculus.launch.py`의 `with_fan:=true`가 이미 동일 기능 제공 (다운스트림 import 0건 사전 확인 완료) | T7 |

### Created (신규)
| 파일 | 책임 | Task |
|------|------|------|
| `CHANGELOG.md` (패키지 루트) | Phase별 변경 이력 (지금까지 없었음) | T9 |

---

## Task Sequencing 원칙

- T2(상수)를 가장 먼저 → 후속 task에서 상수 활용 가능.
- T7(launch 삭제)을 T8(README 갱신) 직전 → README가 삭제 후 상태를 정확히 반영.
- 각 task = 1 commit. PR 머지 시 squash로 단일 commit 됨.
- **모든 task의 첫 step**: `git status`로 깨끗한 working tree 확인.
- **모든 task의 마지막 step**: `colcon build --packages-select oculus_sonar` PASS + commit.

---

## Task 1: Phase A branch 생성 + archive baseline

**Files:**
- Create: git tag `archive/oculus-pre-refactor-2026-05-03`
- Create: git branch `refactor/phase-a-oculus-cleanup`

**왜**: PKRC `refactor-workflow.md`의 "Phase A 첫 회 → archive 태그 생성" 원칙. 미래 회귀 시 `git checkout archive/...`로 즉시 baseline 확인.

- [ ] **Step 1: 현재 브랜치 상태 확인**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git branch --show-current
git status -s
```
Expected: 현재 브랜치는 `docs/oculus-refactor-spec-2026-05-03` (spec commit 직후), working tree clean.

- [ ] **Step 2: main의 baseline SHA 기록**

```bash
git fetch origin main 2>/dev/null || true
BASELINE_SHA=$(git rev-parse origin/main 2>/dev/null || git rev-parse main)
echo "Baseline: $BASELINE_SHA"
```

- [ ] **Step 3: Archive 태그 생성**

```bash
git tag archive/oculus-pre-refactor-2026-05-03 $BASELINE_SHA
git tag --list 'archive/*'
```
Expected: `archive/oculus-pre-refactor-2026-05-03`이 목록에 표시.

- [ ] **Step 4: Phase A branch 생성 (main에서 분기)**

```bash
git checkout -b refactor/phase-a-oculus-cleanup main
git branch --show-current
```
Expected: `refactor/phase-a-oculus-cleanup`.

- [ ] **Step 5: Spec branch의 spec + plan 두 commit을 가져옴 (range cherry-pick)**

```bash
git cherry-pick main..docs/oculus-refactor-spec-2026-05-03
git log --oneline -4
```
Expected (역순):
```
<sha> docs(oculus_sonar): add Phase A implementation plan
<sha> docs(oculus_sonar): add Phase A→B→C refactor design spec
<main HEAD sha> <main의 직전 commit subject>
```

이유: Phase A PR에 spec + plan 두 문서가 같이 들어가야 본문이 자족적. spec branch는 별도 push 없이 흡수.

- [ ] **Step 6: 빌드 sanity check**

```bash
cd /workspace/ros2_ws && source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar
```
Expected: PASS, "Summary: 1 package finished".

(Task 1은 commit 없음 — branch/tag 작업만)

---

## Task 2: A-7 — 매직 넘버를 기존 SonarConstants 네임스페이스로 이동

**Files:**
- Modify: `include/oculus_sonar/sonar_config.hpp` — `SonarConstants` namespace에 part_num·default IP 추가
- Modify: `src/oculus_driver_component.cpp:121-127` — hardcoded part_num list → SonarConstants 참조

**왜**: spec A-7. `SonarConstants`가 이미 `sonar_config.hpp:22`에 존재하므로 새 파일 만들지 않고 확장. part_num 하드코딩(20개 리터럴)이 가장 명확한 후보.

- [ ] **Step 1: 현재 SonarConstants 정의 확인**

```bash
sed -n '20,80p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/sonar_config.hpp
```

- [ ] **Step 2: SonarConstants에 part_num 배열 추가**

`include/oculus_sonar/sonar_config.hpp`의 `namespace SonarConstants {` 블록 안(line 22 직후, 기존 상수들 옆)에 다음 추가:

```cpp
// Hardware Part Number identifiers (from observed devices)
// Used to detect actual hardware vs configured model
constexpr std::array<uint16_t, 5> M750D_PART_NUMBERS  = {1032, 2419, 1434, 1921, 1244};
constexpr std::array<uint16_t, 5> M1200D_PART_NUMBERS = {1042, 2420, 1435, 2086, 1219};
constexpr std::array<uint16_t, 5> M370S_PART_NUMBERS  = {1041, 2418, 1433, 2294, 1217};
constexpr std::array<uint16_t, 4> M3000D_PART_NUMBERS = {2203, 2599, 2659, 2658};

// Default network configuration
constexpr const char* DEFAULT_SONAR_IP = "192.168.0.200";
constexpr int DEFAULT_GAIN_PERCENT = 100;
constexpr int DEFAULT_GAMMA = 200;
constexpr double DEFAULT_RANGE_M = 2.0;
```

또한 파일 상단(`#include` 블록)에 `<array>` include가 없으면 추가:
```cpp
#include <array>
```

- [ ] **Step 3: oculus_driver_component.cpp의 part_num check를 SonarConstants 참조로 교체**

`src/oculus_driver_component.cpp:121-127`을 다음으로 교체:

```cpp
auto matches = [](uint16_t pn, const auto& list) {
  return std::find(list.begin(), list.end(), pn) != list.end();
};
if (matches(part_num, SonarConstants::M750D_PART_NUMBERS)) {
  detected_model = "M750d variant";
} else if (matches(part_num, SonarConstants::M1200D_PART_NUMBERS)) {
  detected_model = "M1200d variant";
} else if (matches(part_num, SonarConstants::M370S_PART_NUMBERS)) {
  detected_model = "M370s variant (70° FOV!)";
} else if (matches(part_num, SonarConstants::M3000D_PART_NUMBERS)) {
  detected_model = "M3000d variant";
}
```

`<algorithm>` include가 없으면 파일 상단에 추가.

- [ ] **Step 4: declare_parameter default도 상수 참조로**

`src/oculus_driver_component.cpp:39-41` 변경:

```cpp
this->declare_parameter<double>("range", SonarConstants::DEFAULT_RANGE_M);
this->declare_parameter<int>("gain",     SonarConstants::DEFAULT_GAIN_PERCENT);
this->declare_parameter<int>("gamma",    SonarConstants::DEFAULT_GAMMA);
```

- [ ] **Step 5: 빌드**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/include/oculus_sonar/sonar_config.hpp \
  oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
git -C /workspace/ros2_ws/src/sensor_packages commit -m "refactor(oculus_sonar): extract part_num and default values to SonarConstants

- 20개 하드코딩 part_num 리터럴을 SonarConstants::*_PART_NUMBERS 배열로 통합
- declare_parameter default(range/gain/gamma)를 SonarConstants에서 참조
- DEFAULT_SONAR_IP 상수 추가 (후속 task에서 활용 가능)"
```

---

## Task 3: A-6 — frame_id 하드코딩 제거

**Files:**
- Modify: `include/oculus_sonar/publishing_data_rx.h` — 생성자에 `frame_id` 인자 추가, 멤버 변수 추가
- Modify: `src/publishing_data_rx.cpp` — 생성자에서 보관, `doPublish`에서 사용
- Modify: `src/oculus_driver_component.cpp` — `PublishingDataRx` 인스턴스화 시 `frame_id_` 전달

**왜**: spec A-6. 현재 `publishing_data_rx.cpp:41`이 `frame_id = "oculus"` 하드코딩 → `OculusDriver`의 `frame_id_` 파라미터 무시.

- [ ] **Step 1: publishing_data_rx.h 생성자 시그니처 변경**

`include/oculus_sonar/publishing_data_rx.h:27` 변경:

```cpp
// Before:
PublishingDataRx(const liboculus::IoServiceThread::IoContextPtr &iosrv);

// After:
PublishingDataRx(const liboculus::IoServiceThread::IoContextPtr &iosrv,
                 const std::string &frame_id);
```

`include/oculus_sonar/publishing_data_rx.h`의 `private:` 블록(line 59-62) 마지막에 멤버 추가:

```cpp
std::string frame_id_;
```

또한 파일 상단 `#include` 블록에 `<string>` 추가 (없으면).

- [ ] **Step 2: publishing_data_rx.cpp 생성자/doPublish 변경**

`src/publishing_data_rx.cpp:13-17` 변경:

```cpp
PublishingDataRx::PublishingDataRx(const liboculus::IoServiceThread::IoContextPtr &iosrv,
                                   const std::string &frame_id)
    : liboculus::DataRx(iosrv),
      raw_data_pub_(nullptr),
      clock_(std::make_shared<rclcpp::Clock>()),
      frame_id_(frame_id) {
}
```

`src/publishing_data_rx.cpp:41` 변경:

```cpp
// Before:
msg.header.frame_id = "oculus";

// After:
msg.header.frame_id = frame_id_;
```

- [ ] **Step 3: oculus_driver_component.cpp의 호출부 수정**

`src/oculus_driver_component.cpp`에서 `PublishingDataRx`를 생성하는 부분을 grep으로 찾는다:

```bash
grep -n "PublishingDataRx\|publishing_data_rx" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
```

발견된 라인에서 `PublishingDataRx(io_srv_)` 또는 유사 호출을 `PublishingDataRx(io_srv_, frame_id_)`로 변경.

(주의: 호출이 `std::make_shared`/`std::make_unique`를 통할 수도 있음. 시그니처 변경에 맞춰 인자 전달.)

- [ ] **Step 4: 빌드**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```
Expected: PASS.

- [ ] **Step 5: Smoke test (bag replay로 frame_id 검증, 하드웨어 불필요)**

```bash
source /workspace/ros2_ws/install/setup.bash
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d &
LAUNCH_PID=$!
sleep 2
ros2 bag play /workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1 --rate 1.0 &
BAG_PID=$!
sleep 5
ros2 topic echo --once /sensor/sonar/oculus/m3000d/raw_data | grep "frame_id"
kill $BAG_PID $LAUNCH_PID 2>/dev/null
```
Expected: `frame_id: oculus_m3000d` (또는 launch에서 설정된 frame_id 값). "oculus" 하드코딩 값이 아님.

(센서 검증 미루기 옵션: 하드웨어 검증을 별도 라운드로 미룬다면 이 step의 smoke test는 생략하고 빌드 PASS만 확인.)

- [ ] **Step 6: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/include/oculus_sonar/publishing_data_rx.h \
  oculus_ros2/oculus_sonar/src/publishing_data_rx.cpp \
  oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
git -C /workspace/ros2_ws/src/sensor_packages commit -m "fix(oculus_sonar): inject frame_id into PublishingDataRx instead of hardcoding

publishing_data_rx.cpp가 'oculus'를 하드코딩하여 OculusDriver의
frame_id_ 파라미터가 무시되던 버그 수정. 생성자 인자로 frame_id
주입하도록 변경."
```

---

## Task 4: A-5 — 핫패스 로그 강등

**Files:**
- Modify: `include/oculus_sonar/ping_to_sonar_image.hpp:45-48` — `RCLCPP_INFO` → `RCLCPP_INFO_ONCE`

**왜**: spec A-5. m3000d 30Hz 운용 시 매 ping마다 콘솔 로그 → 1,800 msg/min 콘솔 스팸. 동일 정보는 1회만으로 충분.

- [ ] **Step 1: 현재 로그 호출 확인**

```bash
sed -n '45,48p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp
```
Expected:
```cpp
RCLCPP_INFO(rclcpp::get_logger("ping_to_sonar_image"),
            "num_bearings: %d, bearing_count: %d, num_ranges: %d, range_resolution: %.4fm, max_range: %.2fm",
            num_bearings, bearing_count, num_ranges, ping.ping()->rangeResolution,
            num_ranges * ping.ping()->rangeResolution);
```

- [ ] **Step 2: INFO_ONCE로 교체**

`include/oculus_sonar/ping_to_sonar_image.hpp:45` 변경:

```cpp
// Before:
RCLCPP_INFO(rclcpp::get_logger("ping_to_sonar_image"),

// After:
RCLCPP_INFO_ONCE(rclcpp::get_logger("ping_to_sonar_image"),
```

(`_ONCE` 매크로는 같은 로그 호출 위치에서 1회만 발행. ping마다 호출되어도 콘솔 출력은 1회.)

- [ ] **Step 3: 빌드**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp
git -C /workspace/ros2_ws/src/sensor_packages commit -m "refactor(oculus_sonar): downgrade hot-path log to RCLCPP_INFO_ONCE

ping_to_sonar_image.hpp:45가 매 ping마다 INFO 로그 발행 →
m3000d 30Hz 시 분당 1,800 msg 콘솔 스팸. 정보는 1회만 필요."
```

---

## Task 5: A-4 — Dead code 일괄 제거

**Files:**
- Modify: `src/oculus_driver_component.cpp:213-219` — 깨진 hex log 블록 삭제
- Modify: `include/oculus_sonar/oculus_driver_component.hpp:81` — `cv_ptr_` 멤버 삭제
- Modify: `config/m750d_params.yaml:22` — 주석 처리된 `num_beams: 1` 삭제
- Modify: `config/m1200d_params.yaml:22` — 활성 `num_beams: 1` 삭제
- Modify: `config/m3000d_params.yaml:26` — 활성 `num_beams: 1` 삭제

**왜**: spec A-4. 깨진 hex log는 `"0x%s"` 포맷에 빈 문자열 placeholder만 전달, `cv_ptr_`은 정의 없는 미사용 멤버, `num_beams: 1` yaml 키는 driver line 49 주석에 "무시됨" 명시.

- [ ] **Step 1: hex log 블록 확인**

```bash
sed -n '210,225p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
```

- [ ] **Step 2: hex log 블록 삭제**

`src/oculus_driver_component.cpp:213-219` 영역에서 `"0x%s"` 포맷의 RCLCPP_* 호출 7줄을 통째로 삭제. 정확한 삭제 대상은 step 1에서 확인된 라인. 주변 의미 있는 코드는 유지.

- [ ] **Step 3: cv_ptr_ 멤버 확인 + 삭제**

```bash
grep -n "cv_ptr_" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
```
Expected: `oculus_driver_component.hpp:81`에 선언만 있고 다른 곳에서 사용 0건.

선언 줄(`cv_bridge::CvImagePtr cv_ptr_;` 같은 라인) 삭제. 만약 위/아래에 관련 include(`#include <cv_bridge/cv_bridge.h>`)가 있고 다른 곳에서 cv_bridge를 안 쓴다면 그 include도 삭제(grep로 확인 후).

- [ ] **Step 4: yaml 파일 정리**

```bash
# m750d (주석 dead 라인)
grep -n "num_beams" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/config/m750d_params.yaml
# m1200d, m3000d (활성 dead 라인)
grep -n "num_beams" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/config/m1200d_params.yaml
grep -n "num_beams" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/config/m3000d_params.yaml
```

각 파일에서 `num_beams` 라인을 통째로 삭제 (주석 라인 포함).

- [ ] **Step 5: 빌드**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```
Expected: PASS. `cv_bridge` 관련 컴파일 에러가 나면 step 3에서 다른 사용처를 놓친 것 → grep 다시.

- [ ] **Step 6: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
  oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp \
  oculus_ros2/oculus_sonar/config/m750d_params.yaml \
  oculus_ros2/oculus_sonar/config/m1200d_params.yaml \
  oculus_ros2/oculus_sonar/config/m3000d_params.yaml
git -C /workspace/ros2_ws/src/sensor_packages commit -m "refactor(oculus_sonar): remove dead code (hex log, cv_ptr_, num_beams yaml key)

- oculus_driver_component.cpp:213-219 깨진 hex log 블록 ('0x%s' + '')
- oculus_driver_component.hpp:81 cv_ptr_ 미사용 멤버
- config/{m750d,m1200d,m3000d}_params.yaml의 num_beams 키
  (driver line 49 주석에 '무시됨' 명시)"
```

---

## Task 6: A-3 — plugins.xml + CMakeLists.txt 정리

**Files:**
- Modify: `plugins.xml` — `OculusFanImager` class 추가, `ReprocessOculusRawData` TODO 주석 블록 삭제
- Modify: `CMakeLists.txt` — line 48-52 위험 include 삭제, TODO 주석 정리, 주석 처리된 reprocess executable 블록 삭제

**왜**: spec A-3. plugins.xml에 `OculusFanImager`가 누락되어 있어 composition으로 로드 시 plugin not found. CMakeLists의 위험 include는 워크스페이스 토폴로지 가정 깨지면 즉시 빌드 실패.

- [ ] **Step 1: plugins.xml 신규 작성**

`plugins.xml` 전체를 다음으로 교체:

```xml
<library path="oculus_driver_component">
  <class name="oculus_sonar::OculusDriver" type="oculus_sonar::OculusDriver" base_class_type="rclcpp::Node">
    <description>
      A ROS2 component driver for Oculus sonar.
    </description>
  </class>
</library>
<library path="oculus_fan_imager_component">
  <class name="oculus_sonar::OculusFanImager" type="oculus_sonar::OculusFanImager" base_class_type="rclcpp::Node">
    <description>
      Converts polar SonarImage to Cartesian fan-shaped Image with optional colormap.
    </description>
  </class>
</library>
```

(주의: `OculusFanImager`가 자체 SHARED 라이브러리(`oculus_fan_imager_component`)이므로 별도 `<library>` 블록이 맞음. `CMakeLists.txt:83`에서 `add_library(oculus_fan_imager_component ...)`로 등록되어 있음.)

- [ ] **Step 2: CMakeLists.txt 위험 include 삭제**

`CMakeLists.txt:48-52` 영역의 다음 5줄 삭제:

```cmake
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../liboculus/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)
include_directories(${CMAKE_BINARY_DIR}/marine_acoustic_msgs/rosidl_generator_cpp)
include_directories(${CMAKE_SOURCE_DIR}/build/marine_acoustic_msgs/rosidl_generator_cpp)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../marine_acoustic_msgs/rosidl_generator_cpp)
```

(같은 의존성을 line 68-79의 `ament_target_dependencies(... liboculus marine_acoustic_msgs)`가 자동 처리.)

- [ ] **Step 3: CMakeLists.txt TODO/주석 처리된 reprocess 블록 정리**

다음 영역 삭제:

- `CMakeLists.txt:6-8` — TODO 주석 3줄 (sonar_image_proc, g3log_ros, ReprocessOculusRawData)
- `CMakeLists.txt:36-39` — 주석 처리된 `find_package` 2줄
- `CMakeLists.txt:58-59` — TODO 주석 + `# src/reprocess_oculus_raw_data.cpp` 1줄
- `CMakeLists.txt:108` — `# rosidl_get_typesupport_target ...` 1줄
- `CMakeLists.txt:121-122` — `# TODO: Implement ReprocessOculusRawData` + 주석 처리된 component 등록
- `CMakeLists.txt:140-154` — 주석 처리된 reprocess executable 블록 전체 (15줄)
- `CMakeLists.txt:163-167` — 빈 install 블록 (TODO 주석 + reprocess executable 자리)

(주석 처리된 BUILD_TESTING 블록 line 211-221은 미래 unit test 추가 시 활용 가능 → 유지.)

- [ ] **Step 4: 빌드**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```
Expected: PASS. include 삭제로 빌드 깨지면 → `ament_target_dependencies`가 누락한 의존성이 있음, 추가 후 재빌드.

- [ ] **Step 5: Component 등록 검증**

```bash
source /workspace/ros2_ws/install/setup.bash
ros2 component types | grep oculus
```
Expected:
```
oculus_sonar
  oculus_sonar::OculusDriver
  oculus_sonar::OculusFanImager
```

(이전엔 `OculusFanImager`가 `ros2 component types`에 안 나왔을 가능성 — 이제 노출되면 plugins.xml 수정 검증 완료.)

- [ ] **Step 6: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/plugins.xml \
  oculus_ros2/oculus_sonar/CMakeLists.txt
git -C /workspace/ros2_ws/src/sensor_packages commit -m "refactor(oculus_sonar): clean plugins.xml + CMakeLists

- plugins.xml: OculusFanImager class 등록 추가
  (ros2 component load로 composition 가능)
- plugins.xml: ReprocessOculusRawData TODO 주석 삭제
- CMakeLists.txt: 위험한 include path 5줄 삭제
  (../liboculus/include, \${CMAKE_BINARY_DIR}/marine_acoustic_msgs/...)
  ament_target_dependencies가 자동 처리
- CMakeLists.txt: TODO 주석·주석 처리된 reprocess executable 정리"
```

---

## Task 7: A-2 — fan_imager.launch.py 삭제 (oculus.launch.py에 이미 통합됨)

**Files:**
- Delete: `launch/fan_imager.launch.py`

**왜**: spec A-2. 사전 확인 결과 `oculus.launch.py`가 이미 `with_fan:=true` 옵션으로 fan imager를 같은 ComposableNodeContainer에 로드. `fan_imager.launch.py`는 standalone Node action으로 동일 기능 중복 + 다운스트림 import 0건.

- [ ] **Step 1: 다운스트림 사용 최종 확인**

```bash
grep -rn "fan_imager.launch\|fan_imager_launch" /workspace/ros2_ws/src --include="*.py" --include="*.xml" 2>/dev/null \
  | grep -v "/build/\|/install/\|fan_imager.launch.py:"
```
Expected: 0줄 (다운스트림 영향 없음). 만약 출력이 있으면 → 호출부를 `oculus.launch.py with_fan:=true`로 마이그레이션하는 추가 step 필요.

- [ ] **Step 2: 파일 삭제**

```bash
git -C /workspace/ros2_ws/src/sensor_packages rm \
  oculus_ros2/oculus_sonar/launch/fan_imager.launch.py
```

- [ ] **Step 3: 빌드 (launch 변경은 Python이라 빌드 영향 없지만 install share 갱신 확인)**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
ls /workspace/ros2_ws/install/oculus_sonar/share/oculus_sonar/launch/
```
Expected: PASS. install share에 `oculus.launch.py`만 존재, `fan_imager.launch.py`는 없음.

- [ ] **Step 4: Smoke test — with_fan 옵션 검증 (bag replay)**

```bash
source /workspace/ros2_ws/install/setup.bash
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true &
LAUNCH_PID=$!
sleep 2
ros2 bag play /workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1 --rate 1.0 &
BAG_PID=$!
sleep 5
ros2 topic list | grep -E "(sonar|fan_image)"
kill $BAG_PID $LAUNCH_PID 2>/dev/null
```
Expected:
```
/sensor/sonar/oculus/m3000d/sonar
/sensor/sonar/oculus/m3000d/fan_image
...
```

(센서 검증 미루기 옵션: 이 step 생략, 빌드 PASS만 확인.)

- [ ] **Step 5: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages commit -m "refactor(oculus_sonar): remove fan_imager.launch.py (covered by with_fan:=true)

oculus.launch.py가 이미 with_fan:=true 옵션으로 동일 기능 제공 +
intra-process composition 이득까지 제공. 다운스트림 import 0건 사전 확인.
운영 진입점 단순화."
```

---

## Task 8: A-1 — README 재작성

**Files:**
- Modify: `README.md` — m3000d 추가, `oculus.launch.py` 단일 진입점, 실제 토픽명·실제 파라미터 반영, 존재하지 않는 항목 삭제

**왜**: spec A-1. 현재 README는 m750d/m1200d만 안내, 존재하지 않는 launch 파일·파라미터 안내, `/raw` vs 실제 `/raw_data` 토픽명 오류.

- [ ] **Step 1: 현재 README 구조 확인**

```bash
grep -nE "^#|launch.py|topic|param" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/README.md | head -40
```

- [ ] **Step 2: 이미 정확한 launch 파일의 docstring을 README 진실의 근거로 활용**

`oculus.launch.py:7-91`이 이미 m750d/m1200d/m3000d/with_fan/colormap/QoS 등 모든 옵션을 정확히 문서화. README는 이를 한국어 또는 영어로 옮기되 다음 원칙 적용:

- 영어 유지 (spec 결정 — fork 기원·오픈소스 컨벤션).
- 단일 진입점만 안내: `ros2 launch oculus_sonar oculus.launch.py sonar_model:=<model> with_fan:=<bool>`.
- m750d/m1200d/m3000d 모두 지원 명시.
- 실제 토픽명: `/sensor/sonar/oculus/{model}/{sonar,image,metadata,raw_data,fan_image}`.
- 실제 파라미터만 안내 (yaml에 있는 것: `ip_address`, `range`, `gain`, `gamma`, `ping_rate`, `freq_mode`, `data_size`, `frame_id` 등). `publish_fan_image`/`num_beams`는 declare 안 됨 → 삭제.

- [ ] **Step 3: README.md 작성**

`README.md`를 다음 구조로 신규 작성 (기존 내용은 base로 하되 위 원칙으로 정정):

```markdown
# oculus_sonar

ROS2 Humble driver for Blue Print Subsea Oculus multibeam imaging sonars (M750d, M1200d, M3000d), with optional Cartesian fan-image converter.

## Components

- `oculus_sonar::OculusDriver` — Connects to sonar over UDP, publishes ping data and metadata.
- `oculus_sonar::OculusFanImager` — Subscribes to polar `SonarImage` and publishes Cartesian `Image` (optional colormap).

Both are `rclcpp_components` (composition-friendly) and also installed as standalone executables (`oculus_driver_node`, `oculus_fan_imager_node`).

## Quickstart

```bash
# Driver only (M750d / M1200d / M3000d):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d

# Driver + fan imager in same composition container (intra-process zero-copy):
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d with_fan:=true colormap:=viridis
```

## Topics Published

| Topic | Type | Description |
|-------|------|-------------|
| `/sensor/sonar/oculus/{model}/sonar` | `marine_acoustic_msgs/SonarImage` | Polar sonar image |
| `/sensor/sonar/oculus/{model}/image` | `sensor_msgs/Image` | Polar grayscale (legacy view) |
| `/sensor/sonar/oculus/{model}/metadata` | `oculus_sonar_msgs/OculusMetadata` | Per-ping metadata |
| `/sensor/sonar/oculus/{model}/raw_data` | `apl_msgs/RawData` | Raw bytes (debug/replay) |
| `/sensor/sonar/oculus/{model}/fan_image` | `sensor_msgs/Image` | Cartesian fan image (when `with_fan:=true`) |

## Launch Arguments

| Arg | Default | Description |
|-----|---------|-------------|
| `sonar_model` | `m750d` | One of `m750d`, `m1200d`, `m3000d` |
| `with_fan` | `false` | Add `OculusFanImager` to the same composition container |
| `ip_address` | from yaml | Sonar IP (or `auto` for auto-detect) |
| `range` | from yaml | Range in meters |
| `gain` | from yaml | Gain percent (1-100) |
| `gamma` | from yaml | Gamma (0-255) |
| `ping_rate` | from yaml | 0=Normal, 1=High, 2=Highest, 3=Low, 4=Lowest |
| `freq_mode` | from yaml | 1=low, 2=high |
| `data_size` | from yaml | `8bit` / `16bit` / `32bit` |
| `apply_colormap` | `true` | Fan imager: apply colormap |
| `colormap` | `turbo` | Fan imager colormap (jet, viridis, plasma, …) |
| `qos_reliability` | `best_effort` | Fan imager subscriber QoS (`reliable` / `best_effort`) |

Empty string for any sonar parameter means "use the value from `config/{sonar_model}_params.yaml`".

## Configuration Files

`config/{m750d,m1200d,m3000d}_params.yaml` — per-model defaults. Override at launch time via the arguments above.

## Dynamic Parameter Changes

```bash
ros2 param set /oculus/fan_imager colormap viridis
ros2 param set /oculus/fan_imager apply_colormap false
```

## Build

```bash
cd ros2_ws && colcon build --packages-select oculus_sonar
source install/setup.bash
```

Depends on `liboculus` (vendored under `oculus_ros2/liboculus`).
```

(필요 시 기존 README의 추가 섹션 — 라이센스, 트러블슈팅 등 — 유지/이동.)

- [ ] **Step 4: README가 안내한 명령이 실제로 동작하는지 sanity check**

```bash
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m3000d --show-args 2>&1 | head -20
```
Expected: README가 명시한 모든 인자가 출력에 포함.

- [ ] **Step 5: Commit**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/README.md
git -C /workspace/ros2_ws/src/sensor_packages commit -m "docs(oculus_sonar): rewrite README to match actual launch/topic/param

- m3000d 추가 (이전엔 m750d/m1200d만)
- 단일 진입점 oculus.launch.py 안내 (존재하지 않는 m750d.launch.py 삭제)
- 실제 토픽명 /raw_data 반영 (이전 /raw 오류)
- declare 안 되는 publish_fan_image/num_beams 안내 삭제
- 실제 launch 인자 표 추가"
```

---

## Task 9: CHANGELOG.md 신규 작성 + Phase A 항목 추가 + 최종 빌드 검증

**Files:**
- Create: `CHANGELOG.md` (패키지 루트, 지금까지 없었음)
- Verify: 전체 build PASS, branch 상태 정상

**왜**: PKRC `refactor-workflow.md`는 phase별 CHANGELOG 항목을 강제. 패키지에 CHANGELOG가 없으면 신규 작성. 본 task는 phase 마무리 — squash merge 후 main이 다음 phase의 baseline.

- [ ] **Step 1: 기존 CHANGELOG 존재 여부 확인**

```bash
ls /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md 2>&1 || echo "없음 — 신규 작성"
```

- [ ] **Step 2: CHANGELOG.md 신규 작성**

`oculus_ros2/oculus_sonar/CHANGELOG.md` 신규 생성:

```markdown
# Changelog

All notable changes to `oculus_sonar` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Phase A: oculus_sonar cleanup (refactor)

### Changed
- `README.md` — m3000d 추가, `oculus.launch.py` 단일 진입점화, 실제 토픽명(`/raw_data`)·실제 파라미터 반영
- `plugins.xml` — `OculusFanImager` class 등록 추가 (composition 가능)
- `CMakeLists.txt` — 위험 include path 5줄 삭제(`../liboculus/include`, `${CMAKE_BINARY_DIR}/marine_acoustic_msgs/...`), TODO 주석·주석 처리된 reprocess executable 정리
- `include/oculus_sonar/sonar_config.hpp` — `SonarConstants` 네임스페이스에 `*_PART_NUMBERS` 배열, `DEFAULT_SONAR_IP`, `DEFAULT_RANGE_M`, `DEFAULT_GAIN_PERCENT`, `DEFAULT_GAMMA` 추가
- `src/oculus_driver_component.cpp` — 20개 하드코딩 part_num 리터럴을 `SonarConstants` 참조로 교체, `declare_parameter` default도 상수 참조

### Removed
- `launch/fan_imager.launch.py` — `oculus.launch.py with_fan:=true`가 동일 기능 + intra-process composition 이득 제공
- `src/oculus_driver_component.cpp:213-219` 깨진 hex log 블록 (`"0x%s"` 포맷에 빈 placeholder만 전달)
- `include/oculus_sonar/oculus_driver_component.hpp:81` `cv_ptr_` 미사용 멤버
- `config/m750d_params.yaml`, `config/m1200d_params.yaml`, `config/m3000d_params.yaml` — `num_beams` 키 (driver line 49 주석에 "무시됨" 명시)

### Fixed
- `src/publishing_data_rx.cpp:41` — `frame_id` 하드코딩(`"oculus"`) → 생성자 주입으로 `OculusDriver`의 `frame_id_` 파라미터 반영
- `include/oculus_sonar/ping_to_sonar_image.hpp:45` — 매 ping `RCLCPP_INFO` → `RCLCPP_INFO_ONCE` (m3000d 30Hz 시 분당 1,800 msg 콘솔 스팸 제거)

### Verification
- `colcon build --packages-select oculus_sonar` PASS
- (선택) bag replay smoke test — `oculus_m3000d_test_v1` 재생 후 발행 토픽 확인
- 회귀 측정 X (Phase A 정의: 알고리즘 영향 0%)

### Notes
- 실제 센서 하드웨어 검증은 별도 라운드로 미룸 (사용자 결정).
- Archive 태그: `archive/oculus-pre-refactor-2026-05-03` (Phase A 시작 시 baseline 보존).
- 다음 phase: `refactor/phase-b1-driver-split` — `OculusDriver` 4-way split.
```

- [ ] **Step 3: 최종 전체 빌드 검증**

```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar
echo "Build status: $?"
```
Expected: PASS, exit 0.

- [ ] **Step 4: branch 상태 점검 (Phase A의 모든 commit이 들어갔는지)**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git log --oneline main..HEAD
```
Expected (역순):
```
<sha> docs(oculus_sonar): add Phase A CHANGELOG entry
<sha> docs(oculus_sonar): rewrite README to match actual launch/topic/param
<sha> refactor(oculus_sonar): remove fan_imager.launch.py (covered by with_fan:=true)
<sha> refactor(oculus_sonar): clean plugins.xml + CMakeLists
<sha> refactor(oculus_sonar): remove dead code (hex log, cv_ptr_, num_beams yaml key)
<sha> refactor(oculus_sonar): downgrade hot-path log to RCLCPP_INFO_ONCE
<sha> fix(oculus_sonar): inject frame_id into PublishingDataRx instead of hardcoding
<sha> refactor(oculus_sonar): extract part_num and default values to SonarConstants
<sha> docs(oculus_sonar): add Phase A→B→C refactor design spec
```

- [ ] **Step 5: Commit CHANGELOG**

```bash
git -C /workspace/ros2_ws/src/sensor_packages add \
  oculus_ros2/oculus_sonar/CHANGELOG.md
git -C /workspace/ros2_ws/src/sensor_packages commit -m "docs(oculus_sonar): add Phase A CHANGELOG entry

PKRC refactor-workflow.md의 phase별 CHANGELOG 강제. 패키지에
CHANGELOG가 없었으므로 신규 작성. Phase A 변경 내역 + 검증
결과 + 다음 phase 안내 포함."
```

- [ ] **Step 6: PR 본문 초안 출력 (사용자가 push 시 사용)**

```bash
echo "=== PR body draft ==="
cat <<'EOF'
## Summary
oculus_sonar Phase A: cleanup (정리 + 사용성). 알고리즘 영향 0%, 회귀 의무 없음.

## Changes
- README/launch/실제 코드 간 불일치 해소 (m3000d 누락, 존재하지 않는 launch 파일 안내, /raw vs /raw_data)
- launch 단일 진입점화 (fan_imager.launch.py → oculus.launch.py with_fan:=true)
- plugins.xml에 OculusFanImager 등록
- CMakeLists.txt 위험 include + TODO 주석 정리
- Dead code 제거 (hex log, cv_ptr_, num_beams yaml key)
- 핫패스 log INFO → INFO_ONCE
- frame_id 하드코딩 → 생성자 주입
- 매직 넘버(part_num 20개, default 값) → SonarConstants 통합

## Verification
- [x] colcon build --packages-select oculus_sonar PASS
- [x] CHANGELOG.md 갱신 (신규 작성)
- [ ] (선택) bag replay smoke test — 사용자 환경에서 별도 수행
- [ ] 다음 phase: refactor/phase-b1-driver-split (OculusDriver 4-way split)

## Next Phase
B-1: OculusDriver 4-way split + Regression 인프라 자산화 (scripts/regression_oculus.sh 신규)
EOF
```

- [ ] **Step 7 (사용자 명시 시에만): push + PR 생성**

PKRC `git-workflow.md`: "푸시는 명시적 지시가 있을 때만". 본 step은 사용자가 별도로 push를 지시할 때만 실행.

먼저 PR 본문을 임시 파일로 저장 (Step 6의 본문을 그대로):

```bash
cat > /tmp/oculus_phase_a_pr_body.md <<'EOF'
## Summary
oculus_sonar Phase A: cleanup (정리 + 사용성). 알고리즘 영향 0%, 회귀 의무 없음.

## Changes
- README/launch/실제 코드 간 불일치 해소 (m3000d 누락, 존재하지 않는 launch 파일 안내, /raw vs /raw_data)
- launch 단일 진입점화 (fan_imager.launch.py → oculus.launch.py with_fan:=true)
- plugins.xml에 OculusFanImager 등록
- CMakeLists.txt 위험 include + TODO 주석 정리
- Dead code 제거 (hex log, cv_ptr_, num_beams yaml key)
- 핫패스 log INFO → INFO_ONCE
- frame_id 하드코딩 → 생성자 주입
- 매직 넘버(part_num 20개, default 값) → SonarConstants 통합

## Verification
- [x] colcon build --packages-select oculus_sonar PASS
- [x] CHANGELOG.md 갱신 (신규 작성)
- [ ] (선택) bag replay smoke test — 사용자 환경에서 별도 수행

## Next Phase
B-1: OculusDriver 4-way split + Regression 인프라 자산화 (scripts/regression_oculus.sh 신규)
EOF
```

그 후 push + PR 생성:

```bash
git -C /workspace/ros2_ws/src/sensor_packages push -u origin refactor/phase-a-oculus-cleanup

gh pr create -R HERO-Lab-POSTECH/sensor_packages \
  --base main \
  --head refactor/phase-a-oculus-cleanup \
  --title "refactor(oculus_sonar): Phase A — cleanup" \
  --body-file /tmp/oculus_phase_a_pr_body.md
```

Expected: PR URL 출력. PKRC 정책상 main 보호로 1 approval + linear history 강제 → squash merge.

---

## Phase A 완료 후 다음 단계

Phase A가 main에 squash merge되면:
1. **Task #6의 Phase A 부분 완료** → Phase B-1 plan 작성으로 이동.
2. main을 새 baseline으로 `git checkout -b refactor/phase-b1-driver-split main` 분기.
3. spec의 Phase B-1 명세에 따라 별도 plan 작성 (이 plan과 같은 형식).
4. Phase B-1은 회귀 의무 발생 → `scripts/regression_oculus.sh` 신규 + bag replay byte-exact 일치 검증 추가.

다음 plan 파일 예정 경로: `docs/superpowers/plans/2026-05-XX-oculus-sonar-phase-b1-driver-split.md`
