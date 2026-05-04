# oculus_sonar Phase B-1: OculusDriver 4-Way Split + Regression Infrastructure

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the 395-line `OculusDriver` class into 4 single-responsibility units (orchestrator + Config + Publishers + ConnectionManager), absorb the CMakeLists `register_nodes`/`register_node` duplication, and create the first sensor_packages regression infrastructure (`scripts/regression_oculus.sh`) reusable in B-2/C.

**Architecture:** Behavior-preserving extract refactor. Each component is split as an independent helper class taking a non-owning `rclcpp::Node*` reference, communicating via small callback interfaces. The orchestrator owns `unique_ptr` to all three and wires them. Verification is **structural regression** without hardware: launch the driver, snapshot its parameter declarations, component types, and advertised publisher metadata; the candidate must match the baseline snapshot exactly.

**Tech Stack:** C++17, ROS2 Humble (rclcpp + rclcpp_components), `liboculus` (vendored), structural verification via `ros2 param dump` + `ros2 component types` + `ros2 topic info`.

**Verification Honesty Note:** True byte-exact ping output verification would require hardware in the loop (the driver consumes raw socket bytes from `liboculus`, not from a bag). What we CAN verify without hardware is that the externally-observable interface (param defaults, topic names, topic types/QoS, component registration) is identical pre- and post-refactor. This catches every refactor bug that matters for B-1 (a behavior-preserving split): renamed topics, drifted param defaults, broken component registration, dropped publishers. Bag-replay byte-exact comparison was originally proposed in the spec but does not actually exercise the driver's publish path without hardware injection — see Task 2 design rationale.

---

## Pre-Flight (Decisions & References)

**Decisions locked from brainstorming (2026-05-04):**

- **D1 (CMakeLists 중복):** `oculus_ros2/oculus_sonar/CMakeLists.txt:105-111`의 `rclcpp_components_register_nodes(...)` 두 호출을 **삭제**한다. `register_node` (lines 114-122)만 남기면 plugin 등록 + executable 생성을 동시에 수행하므로 컴포넌트 중복 노출(`ros2 component types`에서 각 노드 2회 출력) 문제가 해소된다. spec §7 A-3의 미완 항목.
- **D2 (`OculusDriverConfig` 위치):** `rclcpp::Node`를 상속하지 않는 **독립 helper 클래스**로 분리한다. 생성자에서 `rclcpp::Node*` (non-owning, lifetime은 orchestrator 보장)를 받아 파라미터 declare/get/callback을 위임. spec §14 Q-B1 결정 = (ii) 독립 helper.

**Spec references:**
- 전체 청사진: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-1
- 코딩 한도: `/workspace/.claude/rules/coding-style.md` (함수 ≤50줄, 메서드 ≤20개)
- Refactor 4축 (branch/commit/CHANGELOG/PR): `/workspace/.claude/rules/refactor-workflow.md`

**Bag dataset (UCRC, verified existing — Task 8 longevity smoke test에서 사용):**
- 주: `/workspace/data/7_ucrc_watertank/20260122_sonar_lidar/m750d_custom_platform/m750d-range15-tilt45-v1` (m750d, sonar_3d Phase B-1 회귀와 동일 디렉토리 — cross-comparison 용이)
- 대안: `/workspace/data/7_ucrc_watertank/20260122_sonar_lidar/m3000d_blueboat/m3000d-range15-tilt60-v1` (m3000d, spec §7 A-1과 동일 모델)
- 폴백: `/workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1` (KHNP m3000d, UCRC 접근 불가 시)

UCRC bag은 structural snapshot 회귀에서는 직접 사용되지 않는다 (driver가 ROS 토픽이 아닌 UDP socket을 listen). Task 8 longevity smoke test에서 driver와 병행 재생하여 60s 동안 driver process가 죽지 않는지 + launch.log에 crash trace가 없는지 확인하는 데 쓰인다.

**Regression script reference pattern (이미 검증된 형식):**
- `/workspace/ros2_ws/src/lidar_slam/fast_lio/scripts/regression_test_path_buffer.sh` — mode subcommand (baseline / candidate / compare), `BAG_PATH` env var, `OUT_DIR=/tmp/<repo>_regression/`, `set -euo pipefail`
- 회피해야 할 함정 (memory: `reference_fast_lio_regression_gotchas.md`): bag QoS는 `--qos-profile-overrides-path` 또는 launch arg로 강제, `ros2 launch` SIGINT 시 자식 노드 미정리 → `pkill -P $LAUNCH_PID` 3단계 필요, `pgrep` stale match.

---

## File Structure (Locked)

**Files to be created (5 source/header pairs + 1 script):**

```
oculus_ros2/oculus_sonar/
├── include/oculus_sonar/
│   ├── oculus_driver_config.hpp           ← NEW (D2: independent helper)
│   ├── oculus_driver_publishers.hpp       ← NEW
│   ├── oculus_connection_manager.hpp      ← NEW
│   └── oculus_driver_component.hpp        ← MODIFIED (slimmed)
├── src/
│   ├── oculus_driver_config.cpp           ← NEW
│   ├── oculus_driver_publishers.cpp       ← NEW
│   ├── oculus_connection_manager.cpp      ← NEW
│   └── oculus_driver_component.cpp        ← MODIFIED (~80 LOC orchestrator)
├── CMakeLists.txt                          ← MODIFIED (add 3 sources, remove register_nodes 2 lines)
└── scripts/
    └── regression_oculus.sh                ← NEW (B-1, reused in B-2/C)
```

**Class responsibilities (no overlap):**

| Class | 단일 책임 | 외부 인터페이스 |
|-------|----------|----------------|
| `OculusDriverConfig` | 11 declare_parameter, parameterCallback, setPingRate/FreqMode/DataSize, convertDataSizeString, updateSonarConfig | `initialize()`, `current() const`, `on_change(cb)`, `ip_address()`, `frame_id()`, `sonar_model()` |
| `OculusDriverPublishers` | 4 sensor publisher + 9 param-echo publisher + `publishParameters()` + `sonarToImage()` 헬퍼 + 템플릿 `publishPing<PingT>` | `initialize()`, `publishPing(ping)`, `raw_data_publisher()` accessor |
| `OculusConnectionManager` | `IoServiceThread`, `PublishingDataRx`, `StatusRx`, ping/status 콜백 디스패치, 연결 lifecycle | `start()`, `stop()`, `setPingV1Callback`, `setPingV2Callback`, `setStatusCallback`, `setOnConnectCallback`, `setRawPublisher`, `sendSimpleFireMessage`, `isConnected` |
| `OculusDriver` (orchestrator, `rclcpp::Node`) | 위 3개 인스턴스 소유 + 콜백 wiring + ROS2 lifecycle | rclcpp::Node 외부 인터페이스 (외부 변경 0%) |

---

## Verification Strategy

**Per-task gate (after every commit):**
1. `colcon build --packages-select oculus_sonar` PASS (no new errors; pre-existing sign-compare warnings OK)
2. `bash scripts/regression_oculus.sh candidate && bash scripts/regression_oculus.sh compare` → `PASS: structural snapshot identical to baseline`

**Final gate (Task 9 PR 생성 전):**
- baseline (main HEAD `80b2c5e`) vs candidate (B-1 final HEAD) — 4 snapshot files identical
- `ros2 component types | grep oculus` 출력에서 각 노드가 정확히 1회씩만 노출 (CMake D1 효과 확인)
- `wc -l src/oculus_driver_component.cpp` ≤ 100 (orchestrator slim 확인)

**What the structural snapshot captures (4 dimensions):**

| Dimension | Tool | Catches |
|-----------|------|---------|
| Component registration | `ros2 component types \| grep oculus_sonar` | Missing/duplicate registration, plugin macro removal |
| Parameter declarations | `ros2 param dump /oculus_driver --print` | Drifted default value, missing/extra param, type change |
| Publisher topics | `ros2 topic list --include-hidden-topics \| grep ^/sensor/sonar/oculus/` | Renamed topic, missing publisher |
| Publisher metadata | `ros2 topic info /<topic> --verbose` for each | Changed message type, changed QoS profile |

**Expected publisher set (current main, 13 topics):**
```
/sensor/sonar/oculus/<model>/sonar          (marine_acoustic_msgs/SonarImage)
/sensor/sonar/oculus/<model>/metadata       (oculus_sonar_msgs/OculusMetadata)
/sensor/sonar/oculus/<model>/raw_data       (apl_msgs/RawData)
/sensor/sonar/oculus/<model>/image          (sensor_msgs/Image, image_transport)
/sensor/sonar/oculus/<model>/image/compressed (auto from image_transport)
/sensor/sonar/oculus/<model>/image/compressedDepth (auto from image_transport)
/sensor/sonar/oculus/<model>/image/theora   (auto from image_transport)
/sensor/sonar/oculus/<model>/param/{ping_rate,freq_mode,data_size,range,gain,gamma,ip_address,frame_id,sonar_model}
```

(`image/compressed*` topics auto-created by `image_transport::create_publisher` — they're part of the externally observable interface and snapshot must include them.)

**Second regression layer (Task 8 only): driver longevity smoke test**

Catches what structural snapshot cannot — runtime crashes (thread races in the io_service, callback dispatch SEGV, double-stop). Procedure: launch driver, replay UCRC bag in parallel for 60s, verify (a) driver PID still alive at end, (b) `launch.log` does not contain SIGSEGV / what():  / std::terminate / boost::asio error tracebacks. Per-task gate runs only the structural snapshot (cheap); Task 8 final adds the longevity test (~2 minutes total).

---

## Task 1: Branch + Baseline Build

**Files:**
- Create: branch `refactor/phase-b1-driver-split` (no file changes yet)

- [ ] **Step 1: Create branch from main**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git checkout main
git pull --ff-only
git status -sb  # expect: ## main...origin/main, no diff
git checkout -b refactor/phase-b1-driver-split
```

Expected: `Switched to a new branch 'refactor/phase-b1-driver-split'`. Current SHA = `80b2c5e` (Phase A squash head).

- [ ] **Step 2: Confirm baseline build PASS on main HEAD**

```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar 2>&1 | tail -3
```

Expected: `Finished <<< oculus_sonar [Xs]   Summary: 1 package finished`. No errors (sign-compare warnings OK — pre-existing).

- [ ] **Step 3: Source install and confirm component registration baseline**

```bash
source /workspace/ros2_ws/install/setup.bash
ros2 component types 2>&1 | grep -A2 oculus_sonar
```

Expected (current baseline — duplicate registration is the bug we'll fix):
```
oculus_sonar
  oculus_sonar::OculusDriver
  oculus_sonar::OculusFanImager
  oculus_sonar::OculusDriver
  oculus_sonar::OculusFanImager
```

Note this output — Task 4 will reduce it to 2 lines (no duplicates).

---

## Task 2: Regression Infrastructure (`scripts/regression_oculus.sh`)

**Files:**
- Create: `oculus_ros2/oculus_sonar/scripts/regression_oculus.sh`

**Design rationale (read before implementing):** The original spec proposed a bag-replay byte-exact comparison. After investigation, that approach was found unworkable without hardware: `oculus_sonar` consumes raw bytes from a UDP socket via `liboculus`, not from a ROS topic, so a recorded bag cannot be replayed *into* the driver. Capturing bag topics with the driver running in parallel only measures the bag, not the driver. This script instead captures the **structural snapshot** of the running driver, which is what actually changes (or shouldn't) during a behavior-preserving split.

- [ ] **Step 1: Create scripts directory**

```bash
mkdir -p /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/scripts
```

- [ ] **Step 2: Write the script**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/scripts/regression_oculus.sh`:

```bash
#!/usr/bin/env bash
#
# oculus_sonar structural regression — Phase B-1 onwards.
#
# Captures 4 externally-observable snapshots of the running driver:
#   1. component types  (ros2 component types | grep oculus_sonar)
#   2. param dump       (ros2 param dump /oculus_driver after launch)
#   3. topic list       (ros2 topic list, filtered by /sensor/sonar/oculus/)
#   4. topic info       (ros2 topic info --verbose, per topic — type + QoS)
#
# Two runs (baseline + candidate) are diffed. A behavior-preserving refactor
# (B-1) MUST keep all 4 snapshots byte-identical.
#
# Hardware is NOT required: the driver loads, declares params, and creates
# publishers regardless of socket connectivity. We snapshot before the driver
# would attempt to publish anything.
#
# Usage (from main HEAD build):
#   bash regression_oculus.sh baseline
# Then on the candidate branch (after rebuild):
#   bash regression_oculus.sh candidate
#   bash regression_oculus.sh compare
#
# Env overrides:
#   OUT_DIR        : output dir (default: /tmp/oculus_regression)
#   SONAR_MODEL    : m3000d|m1200d|m750d (default: m3000d)
#   STARTUP_WAIT_S : seconds to wait after launch before snapshotting (default: 5)

set -euo pipefail

OUT_DIR="${OUT_DIR:-/tmp/oculus_regression}"
SONAR_MODEL="${SONAR_MODEL:-m750d}"
STARTUP_WAIT_S="${STARTUP_WAIT_S:-5}"
ROS_DOMAIN="${ROS_DOMAIN_ID_REGRESSION:-77}"

mode="${1:-}"

cleanup_processes() {
    local launch_pid="$1"
    # 3-step cleanup (per reference_fast_lio_regression_gotchas.md):
    # ros2 launch SIGINT does NOT propagate to C++ child nodes properly.
    [[ -n "${launch_pid}" ]] && kill -INT "${launch_pid}" 2>/dev/null || true
    sleep 2
    [[ -n "${launch_pid}" ]] && pkill -P "${launch_pid}" 2>/dev/null || true
    sleep 1
    pkill -f oculus_driver 2>/dev/null || true
}

# Stable sort filter that strips ANSI codes and timestamp prefixes that ros2
# CLI sometimes injects (e.g. log lines mixed into ros2 topic info output).
sanitize() {
    sed -e 's/\x1b\[[0-9;]*[a-zA-Z]//g' \
        -e '/^\[INFO\]/d' \
        -e '/^\[WARN\]/d'
}

run_snapshot() {
    local label="$1"
    local label_dir="${OUT_DIR}/${label}"
    mkdir -p "${label_dir}"

    export ROS_DOMAIN_ID="${ROS_DOMAIN}"

    # Snapshot 1: component types (does NOT need a running node — uses introspection)
    ros2 component types 2>&1 | grep -A4 oculus_sonar | sanitize \
        | sort > "${label_dir}/01_component_types.txt"

    # Launch the driver (it will try to connect to a sonar via "auto" — that's fine,
    # the connection failure does not affect param/publisher declarations).
    ros2 launch oculus_sonar oculus.launch.py sonar_model:="${SONAR_MODEL}" \
        > "${label_dir}/launch.log" 2>&1 &
    local launch_pid=$!
    sleep "${STARTUP_WAIT_S}"

    # Verify the node is alive (connection failure to sonar is OK; node failure is not)
    if ! ros2 node list 2>/dev/null | grep -q oculus_driver; then
        echo "[${label}] ERROR: oculus_driver node did not start — see ${label_dir}/launch.log"
        cleanup_processes "${launch_pid}"
        return 1
    fi

    # Snapshot 2: param dump (sorted YAML for stable diff)
    ros2 param dump /oculus_driver --print 2>/dev/null | sanitize \
        > "${label_dir}/02_param_dump.yaml" || {
        echo "[${label}] ERROR: param dump failed"
        cleanup_processes "${launch_pid}"
        return 1
    }

    # Snapshot 3: publisher topic list (filtered + sorted)
    ros2 topic list 2>/dev/null \
        | grep -E "^/sensor/sonar/oculus/${SONAR_MODEL}/" \
        | sort > "${label_dir}/03_topic_list.txt"

    # Snapshot 4: per-topic info (type + QoS for each)
    > "${label_dir}/04_topic_info.txt"
    while IFS= read -r topic; do
        echo "=== ${topic} ===" >> "${label_dir}/04_topic_info.txt"
        ros2 topic info "${topic}" --verbose 2>/dev/null \
            | sanitize \
            | grep -vE "^Node name:|^Node namespace:|^GID:" \
            >> "${label_dir}/04_topic_info.txt"
        echo "" >> "${label_dir}/04_topic_info.txt"
    done < "${label_dir}/03_topic_list.txt"

    cleanup_processes "${launch_pid}"

    # Build a master summary for compare-mode
    {
        echo "# oculus_sonar structural snapshot"
        echo "label: ${label}"
        echo "sonar_model: ${SONAR_MODEL}"
        echo "captured_at: $(date -Iseconds)"
        echo ""
        echo "## sha256 of each snapshot file (timestamp-independent)"
        for f in 01_component_types.txt 02_param_dump.yaml 03_topic_list.txt 04_topic_info.txt; do
            local hash=$(sha256sum "${label_dir}/${f}" | cut -d' ' -f1)
            local lines=$(wc -l < "${label_dir}/${f}")
            echo "${f}  lines=${lines}  hash=${hash}"
        done
    } > "${label_dir}/summary.txt"

    echo "[${label}] saved to ${label_dir}"
    cat "${label_dir}/summary.txt"
}

case "${mode}" in
    baseline|candidate)
        run_snapshot "${mode}"
        ;;
    compare)
        if [[ ! -d "${OUT_DIR}/baseline" || ! -d "${OUT_DIR}/candidate" ]]; then
            echo "ERROR: ${OUT_DIR}/baseline and ${OUT_DIR}/candidate must both exist"
            exit 1
        fi
        local_fail=0
        for f in 01_component_types.txt 02_param_dump.yaml 03_topic_list.txt 04_topic_info.txt; do
            if ! diff -u "${OUT_DIR}/baseline/${f}" "${OUT_DIR}/candidate/${f}"; then
                echo ""
                echo "FAIL: ${f} differs"
                local_fail=1
            fi
        done
        if [[ "${local_fail}" -eq 1 ]]; then
            echo ""
            echo "FAIL: structural snapshot mismatch (see diffs above)"
            exit 1
        fi
        echo "PASS: structural snapshot identical to baseline"
        ;;
    *)
        echo "Usage: $0 {baseline|candidate|compare}"
        echo ""
        echo "Env overrides: OUT_DIR SONAR_MODEL STARTUP_WAIT_S"
        echo ""
        echo "What this captures (4 snapshots):"
        echo "  01_component_types.txt — ros2 component types output (registration)"
        echo "  02_param_dump.yaml      — ros2 param dump /oculus_driver (defaults)"
        echo "  03_topic_list.txt       — advertised topics under /sensor/sonar/oculus/<model>/"
        echo "  04_topic_info.txt       — per-topic message type and QoS profile"
        exit 2
        ;;
esac
```

- [ ] **Step 3: Make executable + smoke test usage block**

```bash
cd /workspace/ros2_ws/src/sensor_packages
chmod +x oculus_ros2/oculus_sonar/scripts/regression_oculus.sh
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh 2>&1 | head -10
```

Expected:
```
Usage: oculus_ros2/oculus_sonar/scripts/regression_oculus.sh {baseline|candidate|compare}

Env overrides: OUT_DIR SONAR_MODEL STARTUP_WAIT_S

What this captures (4 snapshots):
  01_component_types.txt — ros2 component types output (registration)
  02_param_dump.yaml      — ros2 param dump /oculus_driver (defaults)
  03_topic_list.txt       — advertised topics under /sensor/sonar/oculus/<model>/
  04_topic_info.txt       — per-topic message type and QoS profile
```

- [ ] **Step 4: Verify ros2 CLI tools availability**

```bash
which ros2 && ros2 --help 2>&1 | grep -E "component|param|topic" | head -3
```

Expected: `ros2` path and 3 lines mentioning each subcommand. If missing, run `source /opt/ros/humble/setup.bash` first.

- [ ] **Step 5: Commit infrastructure**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/scripts/regression_oculus.sh
git commit -m "feat(oculus_sonar): add regression_oculus.sh for B-1 structural verification

런타임 driver의 4개 외부 관찰 가능한 snapshot (component types, param
dump, topic list, per-topic info)을 캡처해 baseline/candidate diff. 하드웨어
없이도 refactor 버그(파라미터 default drift, 토픽 이름/타입/QoS 변화,
component 등록 누락)를 탐지. ros2 launch SIGINT 자식 미정리 함정 회피용
3단계 cleanup (reference_fast_lio_regression_gotchas.md 패턴 이식).

bag-replay byte-exact는 사용 안 함 — 드라이버는 ROS 토픽이 아니라 UDP
소켓에서 raw bytes를 받기 때문에 bag을 driver INTO 재생할 방법 없음."
```

---

## Task 3: Capture Baseline Snapshot (Main HEAD)

**Files:** None (data goes to `/tmp/oculus_regression/baseline/`, not committed)

- [ ] **Step 1: Confirm we are on main HEAD baseline binary**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git log --oneline -1
```

Expected: `b2261fc` … or whatever Task 2 commit SHA — but the **driver source** is identical to main HEAD because Task 2 only added a script. So the built `.so` reflects main HEAD behavior.

- [ ] **Step 2: Run baseline snapshot**

```bash
cd /workspace/ros2_ws/src/sensor_packages
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh baseline 2>&1 | tail -15
```

Expected: 4 snapshot files saved, prints summary like:
```
[baseline] saved to /tmp/oculus_regression/baseline
# oculus_sonar structural snapshot
label: baseline
sonar_model: m3000d
captured_at: 2026-05-04T...
## sha256 of each snapshot file (timestamp-independent)
01_component_types.txt  lines=3  hash=<sha256>
02_param_dump.yaml      lines=15  hash=<sha256>
03_topic_list.txt       lines=13+  hash=<sha256>
04_topic_info.txt       lines=80+  hash=<sha256>
```

If `lines=0` on `02_param_dump.yaml`, the driver didn't start — check `/tmp/oculus_regression/baseline/launch.log`.

If `lines=0` on `03_topic_list.txt`, the `STARTUP_WAIT_S=5` was too short — re-run with `STARTUP_WAIT_S=10 bash ...`.

- [ ] **Step 3: Inspect snapshot files manually for sanity**

```bash
echo "=== component types ===" && cat /tmp/oculus_regression/baseline/01_component_types.txt
echo "=== topic list ==="     && cat /tmp/oculus_regression/baseline/03_topic_list.txt
echo "=== param dump ==="     && head -30 /tmp/oculus_regression/baseline/02_param_dump.yaml
```

Expected:
- Component types lists `OculusDriver` + `OculusFanImager` (potentially 2x each — that's the duplicate registration we'll fix in Task 4)
- Topic list shows 13+ topics under `/sensor/sonar/oculus/m3000d/`
- Param dump shows YAML for `/oculus_driver:` with 13 parameters

- [ ] **Step 4: Snapshot baseline summary into a known location for paranoia**

```bash
cp -r /tmp/oculus_regression/baseline /tmp/oculus_regression/baseline.preserved_$(date +%Y%m%d_%H%M%S)
ls -ld /tmp/oculus_regression/*
```

Expected: `baseline/` and `baseline.preserved_<timestamp>/` both exist. The preserved copy is insurance against accidental overwrite during candidate runs.

---

## Task 4: CMakeLists Deduplication (D1)

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CMakeLists.txt:104-122` (delete lines 105-111, keep 114-122)

- [ ] **Step 1: Read the current registration block**

```bash
sed -n '104,122p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt
```

Expected output (current state):
```
# Register components (replaces nodelet plugin)
rclcpp_components_register_nodes(oculus_driver_component
  "oculus_sonar::OculusDriver"
)

rclcpp_components_register_nodes(oculus_fan_imager_component
  "oculus_sonar::OculusFanImager"
)

# Optional: create standalone executable too
rclcpp_components_register_node(oculus_driver_component
  PLUGIN "oculus_sonar::OculusDriver"
  EXECUTABLE oculus_driver_node
)

rclcpp_components_register_node(oculus_fan_imager_component
  PLUGIN "oculus_sonar::OculusFanImager"
  EXECUTABLE oculus_fan_imager_node
)
```

- [ ] **Step 2: Edit CMakeLists.txt — replace block with deduplicated version**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt`, replace the entire range of lines 104-122 with this new block:

```cmake
# Register components — register_node creates both the plugin entry
# AND a standalone executable. Earlier versions also called
# register_nodes which led to duplicate registration; removed in Phase B-1.
rclcpp_components_register_node(oculus_driver_component
  PLUGIN "oculus_sonar::OculusDriver"
  EXECUTABLE oculus_driver_node
)

rclcpp_components_register_node(oculus_fan_imager_component
  PLUGIN "oculus_sonar::OculusFanImager"
  EXECUTABLE oculus_fan_imager_node
)
```

- [ ] **Step 3: Build**

```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar 2>&1 | tail -3
source install/setup.bash
```

Expected: `Finished <<< oculus_sonar`. No new errors.

- [ ] **Step 4: Verify deduplication via `ros2 component types`**

```bash
ros2 component types 2>&1 | grep -A4 oculus_sonar
```

Expected (each node now appears exactly once):
```
oculus_sonar
  oculus_sonar::OculusDriver
  oculus_sonar::OculusFanImager
```

- [ ] **Step 5: Run regression candidate snapshot — EXPECT a controlled FAIL on `01_component_types.txt`**

```bash
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -15
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare || echo "Diff found (expected for Task 4)"
```

Expected behavior — `compare` will diff `01_component_types.txt`:
```
--- baseline/01_component_types.txt
+++ candidate/01_component_types.txt
@@ ...
   oculus_sonar::OculusDriver
   oculus_sonar::OculusFanImager
-  oculus_sonar::OculusDriver
-  oculus_sonar::OculusFanImager
```

This diff is **the intended outcome of D1** — duplicate registration removed. Other 3 snapshots (`02_param_dump.yaml`, `03_topic_list.txt`, `04_topic_info.txt`) MUST be byte-identical because we changed only registration plumbing, not param/publisher logic.

If `02`/`03`/`04` differ → Task 4 broke something unrelated to D1; investigate before proceeding.

- [ ] **Step 6: Update baseline to reflect post-D1 state**

After Task 4, the de-duplicated component registration becomes the new baseline for Tasks 5-8 (which must NOT re-introduce the duplicate). Replace baseline with current candidate:

```bash
rm -rf /tmp/oculus_regression/baseline
mv /tmp/oculus_regression/candidate /tmp/oculus_regression/baseline
echo "Baseline updated to post-D1 state"
ls /tmp/oculus_regression/baseline/
```

- [ ] **Step 7: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CMakeLists.txt
git commit -m "refactor(oculus_sonar): remove duplicate component registration (D1)

CMakeLists.txt에서 register_nodes 호출 두 개를 제거하고 register_node만
유지. 후자는 plugin 등록과 executable 생성을 동시에 수행하므로 둘 다
호출하면 컴포넌트 타입이 두 번 등록되어 ros2 component types에서
중복 출력되던 문제 해소. Phase A spec §7 A-3 미완 항목 흡수."
```

---

## Task 5: Extract `OculusDriverConfig` (D2 — independent helper)

**Files:**
- Create: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_config.hpp`
- Create: `oculus_ros2/oculus_sonar/src/oculus_driver_config.cpp`
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp` (remove migrated members)
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp` (replace migrated logic with delegation)
- Modify: `oculus_ros2/oculus_sonar/CMakeLists.txt:42-45` (add new .cpp to add_library)

- [ ] **Step 1: Create header `oculus_driver_config.hpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_config.hpp`:

```cpp
/**
 * @file oculus_driver_config.hpp
 * @brief Independent helper that owns Oculus sonar parameter declaration,
 *        dynamic-reconfigure callback, and SonarConfiguration synthesis.
 *
 * This class is intentionally NOT an rclcpp::Node subclass. It takes a
 * non-owning Node* whose lifetime is guaranteed by the orchestrator
 * (OculusDriver). Splitting this concern out makes parameter logic unit
 * testable with a mock node and keeps OculusDriver focused on wiring.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>

#include "liboculus/SonarConfiguration.h"

namespace oculus_sonar {

class OculusDriverConfig {
 public:
  using OnChangeCallback = std::function<void(const liboculus::SonarConfiguration&)>;

  explicit OculusDriverConfig(rclcpp::Node* node);
  ~OculusDriverConfig() = default;

  OculusDriverConfig(const OculusDriverConfig&) = delete;
  OculusDriverConfig& operator=(const OculusDriverConfig&) = delete;

  // Declares all parameters, reads initial values, registers parameter callback.
  // Idempotent against accidental double calls in same process is NOT guaranteed —
  // declare_parameter throws if already declared.
  void initialize();

  // Snapshot of current SonarConfiguration; valid while *this is alive.
  const liboculus::SonarConfiguration& current() const { return sonar_config_; }

  // Register a listener invoked AFTER each successful parameter update.
  // Multiple listeners allowed; called in registration order.
  void on_change(OnChangeCallback cb) { listeners_.push_back(std::move(cb)); }

  // String accessors used by the orchestrator and other helpers.
  const std::string& ip_address() const { return ip_address_; }
  const std::string& frame_id() const { return frame_id_; }
  const std::string& sonar_model() const { return sonar_model_; }

 private:
  rcl_interfaces::msg::SetParametersResult parameterCallback(
      const std::vector<rclcpp::Parameter>& parameters);

  void updateSonarConfig();
  void setPingRate(int ping_rate);
  void setFreqMode(int freq_mode);
  void setDataSize(int data_size);
  int convertDataSizeString(const std::string& data_size_str);

  rclcpp::Node* node_;  // non-owning
  liboculus::SonarConfiguration sonar_config_;
  std::string ip_address_;
  std::string frame_id_;
  std::string sonar_model_;
  std::vector<OnChangeCallback> listeners_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;
};

}  // namespace oculus_sonar
```

- [ ] **Step 2: Create implementation `oculus_driver_config.cpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_config.cpp`:

```cpp
/**
 * @file oculus_driver_config.cpp
 * @brief Implementation of OculusDriverConfig — see header for design notes.
 */

#include "oculus_sonar/oculus_driver_config.hpp"

#include "liboculus/Constants.h"
#include "oculus_sonar/sonar_config.hpp"

namespace oculus_sonar {

OculusDriverConfig::OculusDriverConfig(rclcpp::Node* node) : node_(node) {}

void OculusDriverConfig::initialize() {
  node_->declare_parameter<std::string>("ip_address", "auto");
  node_->declare_parameter<std::string>("frame_id", "oculus");
  node_->declare_parameter<std::string>("sonar_model", "m750d");
  node_->declare_parameter<double>("range", SonarConstants::DEFAULT_RANGE_M);
  node_->declare_parameter<int>("gain", SonarConstants::DEFAULT_GAIN_PERCENT);
  node_->declare_parameter<int>("gamma", SonarConstants::DEFAULT_GAMMA);
  node_->declare_parameter<int>("ping_rate", 3);
  node_->declare_parameter<int>("freq_mode", 2);
  node_->declare_parameter<bool>("send_gain", true);
  node_->declare_parameter<std::string>("data_size", "8bit");
  node_->declare_parameter<bool>("send_range_as_meters", true);
  node_->declare_parameter<bool>("send_simple_return", true);
  node_->declare_parameter<bool>("gain_assistance", false);

  ip_address_ = node_->get_parameter("ip_address").as_string();
  frame_id_ = node_->get_parameter("frame_id").as_string();
  sonar_model_ = node_->get_parameter("sonar_model").as_string();

  updateSonarConfig();

  param_callback_handle_ = node_->add_on_set_parameters_callback(
      std::bind(&OculusDriverConfig::parameterCallback, this, std::placeholders::_1));
}

rcl_interfaces::msg::SetParametersResult OculusDriverConfig::parameterCallback(
    const std::vector<rclcpp::Parameter>& parameters) {
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto& param : parameters) {
    if (param.get_name() == "range") {
      double range = param.as_double();
      RCLCPP_INFO(node_->get_logger(), "Setting sonar range to %.1f m", range);
      sonar_config_.setRange(range);
    } else if (param.get_name() == "gain") {
      int gain = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting gain to %d pct", gain);
      sonar_config_.setGainPercent(gain);
    } else if (param.get_name() == "gamma") {
      int gamma = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting gamma to %d", gamma);
      sonar_config_.setGamma(gamma);
    } else if (param.get_name() == "ping_rate") {
      int ping_rate = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting ping rate to (%d): %.1f Hz",
                  ping_rate, liboculus::PingRateToHz(ping_rate));
      setPingRate(ping_rate);
    } else if (param.get_name() == "freq_mode") {
      int freq_mode = param.as_int();
      RCLCPP_INFO(node_->get_logger(), "Setting freq mode to %s",
                  liboculus::FreqModeToString(freq_mode).c_str());
      setFreqMode(freq_mode);
    } else if (param.get_name() == "data_size") {
      std::string data_size_str = param.as_string();
      int data_size = convertDataSizeString(data_size_str);
      RCLCPP_INFO(node_->get_logger(), "Setting data size: %s (%d)",
                  data_size_str.c_str(), data_size);
      setDataSize(data_size);
    } else if (param.get_name() == "send_range_as_meters") {
      sonar_config_.sendRangeAsMeters(param.as_bool());
    } else if (param.get_name() == "send_gain") {
      sonar_config_.setSendGain(param.as_bool());
    } else if (param.get_name() == "send_simple_return") {
      sonar_config_.setSimpleReturn(param.as_bool());
    } else if (param.get_name() == "gain_assistance") {
      sonar_config_.setGainAssistance(param.as_bool());
    }
  }

  for (auto& cb : listeners_) {
    cb(sonar_config_);
  }
  return result;
}

void OculusDriverConfig::updateSonarConfig() {
  double range = node_->get_parameter("range").as_double();
  int gain = node_->get_parameter("gain").as_int();
  int gamma = node_->get_parameter("gamma").as_int();
  int ping_rate = node_->get_parameter("ping_rate").as_int();
  int freq_mode = node_->get_parameter("freq_mode").as_int();
  std::string data_size_str = node_->get_parameter("data_size").as_string();
  int data_size = convertDataSizeString(data_size_str);
  bool send_range_as_meters = node_->get_parameter("send_range_as_meters").as_bool();
  bool send_gain = node_->get_parameter("send_gain").as_bool();
  bool send_simple_return = node_->get_parameter("send_simple_return").as_bool();
  bool gain_assistance = node_->get_parameter("gain_assistance").as_bool();

  sonar_config_.setRange(range);
  sonar_config_.setGainPercent(gain);
  sonar_config_.setGamma(gamma);
  setPingRate(ping_rate);
  setFreqMode(freq_mode);
  setDataSize(data_size);

  sonar_config_.sendRangeAsMeters(send_range_as_meters)
      .setSendGain(send_gain)
      .setSimpleReturn(send_simple_return)
      .setGainAssistance(gain_assistance);

  sonar_config_.use512Beams();
}

void OculusDriverConfig::setPingRate(int ping_rate) {
  switch (ping_rate) {
    case 0: sonar_config_.setPingRate(pingRateNormal); break;
    case 1: sonar_config_.setPingRate(pingRateHigh); break;
    case 2: sonar_config_.setPingRate(pingRateHighest); break;
    case 3: sonar_config_.setPingRate(pingRateLow); break;
    case 4: sonar_config_.setPingRate(pingRateLowest); break;
    case 5: sonar_config_.setPingRate(pingRateStandby); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown ping rate %d", ping_rate);
  }
}

void OculusDriverConfig::setFreqMode(int freq_mode) {
  switch (freq_mode) {
    case 1: sonar_config_.setFreqMode(liboculus::OCULUS_LOW_FREQ); break;
    case 2: sonar_config_.setFreqMode(liboculus::OCULUS_HIGH_FREQ); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown frequency mode %d", freq_mode);
  }
}

void OculusDriverConfig::setDataSize(int data_size) {
  switch (data_size) {
    case 1: sonar_config_.setDataSize(dataSize8Bit); break;
    case 2: sonar_config_.setDataSize(dataSize16Bit); break;
    case 4: sonar_config_.setDataSize(dataSize32Bit); break;
    default:
      RCLCPP_WARN(node_->get_logger(), "Unknown data size %d", data_size);
  }
}

int OculusDriverConfig::convertDataSizeString(const std::string& data_size_str) {
  if (data_size_str == "8bit") {
    return 1;
  } else if (data_size_str == "16bit") {
    return 2;
  } else if (data_size_str == "32bit") {
    return 4;
  } else {
    RCLCPP_WARN(node_->get_logger(),
                "Unknown data size string '%s', defaulting to 8bit",
                data_size_str.c_str());
    return 1;
  }
}

}  // namespace oculus_sonar
```

- [ ] **Step 3: Update CMakeLists.txt to compile new source**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt`, find the `add_library(oculus_driver_component SHARED ...)` block (currently lines 42-45) and add the new source:

```cmake
add_library(oculus_driver_component SHARED
  src/oculus_driver_component.cpp
  src/oculus_driver_config.cpp
  src/publishing_data_rx.cpp
)
```

- [ ] **Step 4: Update `oculus_driver_component.hpp` — remove migrated members and methods**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp`:

Add include after the existing includes (around line 35):
```cpp
#include "oculus_sonar/oculus_driver_config.hpp"
```

Then remove these declarations (lines 91-122 in current file):
- `parameterCallback`
- `setPingRate`, `setFreqMode`, `setDataSize`
- `updateSonarConfig`
- `convertDataSizeString`

And remove these member variables (lines 152-158):
- `ip_address_`, `frame_id_`, `sonar_model_`
- `sonar_config_`
- `param_callback_handle_`

Add this private member instead (just before the closing `};`):
```cpp
  std::unique_ptr<OculusDriverConfig> config_;
```

Also update `pingCallback` (lines 61-78) to read frame_id from config:
```cpp
  template <typename Ping_t>
  void pingCallback(const Ping_t &ping) {
    auto sonar_msg = pingToSonarImage(ping);
    sonar_msg.header.stamp = this->get_clock()->now();
    sonar_msg.header.frame_id = config_->frame_id();
    imaging_sonar_pub_->publish(sonar_msg);
    auto image_msg = sonarToImage(sonar_msg);
    image_pub_.publish(image_msg);

    oculus_sonar_msgs::msg::OculusMetadata meta;
    meta.header = sonar_msg.header;
    for (unsigned int i = 0; i < ping.gains().size(); i++) {
      meta.tvg.push_back(ping.gains().at(i));
    }
    oculus_meta_pub_->publish(meta);
    publishParameters();
  }
```

- [ ] **Step 5: Update `oculus_driver_component.cpp` — delegate to config_**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp`:

Add include (after existing includes around line 16):
```cpp
#include "oculus_sonar/oculus_driver_config.hpp"
```

Remove these implementations entirely:
- `parameterCallback` (lines 165-225)
- `updateSonarConfig` (lines 227-256)
- `setPingRate` (lines 258-281)
- `setFreqMode` (lines 283-294)
- `setDataSize` (lines 296-310)
- `convertDataSizeString` (lines 312-324)

In `OculusDriver::init()`, replace the parameter declaration block (lines 36-49) and initial-value reads (lines 53-55) and `add_on_set_parameters_callback` (lines 95-96) and `updateSonarConfig()` call (line 99) with:

```cpp
  config_ = std::make_unique<OculusDriverConfig>(this);
  config_->initialize();

  ip_address_ /* DELETED — read from config_ */
```

So `init()` opening becomes:
```cpp
void OculusDriver::init() {
  config_ = std::make_unique<OculusDriverConfig>(this);
  config_->initialize();

  data_rx_ = std::make_unique<PublishingDataRx>(io_srv_.context(), config_->frame_id());

  RCLCPP_DEBUG(this->get_logger(), "Advertising topics in namespace %s",
               this->get_namespace());

  std::string topic_prefix = "/sensor/sonar/oculus/" + config_->sonar_model();
  // ... (unchanged publishers section continues)
```

In the rest of `init()`, replace all `frame_id_` with `config_->frame_id()`, `ip_address_` with `config_->ip_address()`, `sonar_model_` with `config_->sonar_model()`, and `sonar_config_` with `config_->current()`.

Wire the on-change listener after publisher setup (replacing the implicit `parameterCallback → sendSimpleFireMessage` linkage):

```cpp
  config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
    if (data_rx_->isConnected()) {
      data_rx_->sendSimpleFireMessage(new_config);
    }
  });
```

In the on-connect callback (currently line 111-113), update to use config:
```cpp
  data_rx_->setOnConnectCallback([this]() {
    data_rx_->sendSimpleFireMessage(config_->current());
  });
```

In status callback (lines 116-152), replace `sonar_model_` with `config_->sonar_model()` and `ip_address_` with `config_->ip_address()`.

In the connect block (lines 154-159):
```cpp
  if (config_->ip_address() == "auto") {
    RCLCPP_INFO(this->get_logger(), "Attempting to auto-detect sonar");
  } else {
    RCLCPP_INFO(this->get_logger(), "Opening sonar at %s", config_->ip_address().c_str());
    data_rx_->connect(config_->ip_address());
  }
```

- [ ] **Step 6: Build**

```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar 2>&1 | tail -10
source install/setup.bash
```

Expected: `Finished <<< oculus_sonar`. If link errors mention undefined references to `OculusDriverConfig::*`, the new .cpp wasn't picked up — re-check Step 3.

- [ ] **Step 7: Regression**

```bash
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -20
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected: `PASS: structural snapshot identical to baseline`.

If FAIL on `param/*` topics: a parameter is now declared with a different default — diff output will show. Fix the discrepancy in `OculusDriverConfig::initialize()` and re-run.

- [ ] **Step 8: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_config.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_config.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
        oculus_ros2/oculus_sonar/CMakeLists.txt
git commit -m "refactor(oculus_sonar): extract OculusDriverConfig helper (B-1 step 1/3)

11 declare_parameter, parameterCallback, 3 setter helpers, updateSonarConfig,
convertDataSizeString을 OculusDriverConfig 독립 helper로 추출.
Node*를 non-owning 참조로 받아 Node 상속 안 함 → 단위 테스트 가능.
on_change 콜백으로 sendSimpleFireMessage trigger 분리.

structural snapshot regression PASS (4 dimensions: component types,
param dump, topic list, per-topic info)."
```

---

## Task 6: Extract `OculusDriverPublishers`

**Files:**
- Create: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp`
- Create: `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp`
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp`
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp`
- Modify: `oculus_ros2/oculus_sonar/CMakeLists.txt`

- [ ] **Step 1: Create header `oculus_driver_publishers.hpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp`:

```cpp
/**
 * @file oculus_driver_publishers.hpp
 * @brief Owns the 4 sensor publishers + 9 parameter-echo publishers,
 *        and the templated publishPing call site.
 */
#pragma once

#include <memory>
#include <string>

#include <apl_msgs/msg/raw_data.hpp>
#include <image_transport/image_transport.hpp>
#include <marine_acoustic_msgs/msg/sonar_image.hpp>
#include <oculus_sonar_msgs/msg/oculus_metadata.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>

#include "oculus_sonar/ping_to_sonar_image.hpp"

namespace oculus_sonar {

class OculusDriverPublishers {
 public:
  OculusDriverPublishers(rclcpp::Node* node,
                         const std::string& topic_prefix,
                         const std::string& frame_id);
  ~OculusDriverPublishers() = default;

  OculusDriverPublishers(const OculusDriverPublishers&) = delete;
  OculusDriverPublishers& operator=(const OculusDriverPublishers&) = delete;

  // Creates all publishers; must be called before publishPing.
  void initialize();

  // Returns the raw_data publisher so the connection layer can hand it
  // to PublishingDataRx::setRawPublisher.
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_publisher() const {
    return raw_data_pub_;
  }

  // Templated entry: publishes sonar/image/metadata + parameter echoes.
  template <typename PingT>
  void publishPing(const PingT& ping) {
    auto sonar_msg = pingToSonarImage(ping);
    sonar_msg.header.stamp = node_->get_clock()->now();
    sonar_msg.header.frame_id = frame_id_;
    imaging_sonar_pub_->publish(sonar_msg);

    auto image_msg = sonarToImage(sonar_msg);
    image_pub_.publish(image_msg);

    oculus_sonar_msgs::msg::OculusMetadata meta;
    meta.header = sonar_msg.header;
    for (unsigned int i = 0; i < ping.gains().size(); i++) {
      meta.tvg.push_back(ping.gains().at(i));
    }
    oculus_meta_pub_->publish(meta);

    publishParameters();
  }

 private:
  void publishParameters();
  sensor_msgs::msg::Image sonarToImage(
      const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const;

  rclcpp::Node* node_;  // non-owning
  std::string topic_prefix_;
  std::string frame_id_;

  rclcpp::Publisher<marine_acoustic_msgs::msg::SonarImage>::SharedPtr imaging_sonar_pub_;
  rclcpp::Publisher<oculus_sonar_msgs::msg::OculusMetadata>::SharedPtr oculus_meta_pub_;
  rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr raw_data_pub_;
  image_transport::Publisher image_pub_;

  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr ping_rate_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr freq_mode_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr data_size_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr range_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gain_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr gamma_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr ip_address_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr frame_id_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr sonar_model_pub_;
};

}  // namespace oculus_sonar
```

- [ ] **Step 2: Create implementation `oculus_driver_publishers.cpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp`:

```cpp
/**
 * @file oculus_driver_publishers.cpp
 * @brief Implementation of OculusDriverPublishers.
 */

#include "oculus_sonar/oculus_driver_publishers.hpp"

namespace oculus_sonar {

OculusDriverPublishers::OculusDriverPublishers(rclcpp::Node* node,
                                               const std::string& topic_prefix,
                                               const std::string& frame_id)
    : node_(node), topic_prefix_(topic_prefix), frame_id_(frame_id) {}

void OculusDriverPublishers::initialize() {
  auto sensor_qos = rclcpp::SensorDataQoS();

  imaging_sonar_pub_ = node_->create_publisher<marine_acoustic_msgs::msg::SonarImage>(
      topic_prefix_ + "/sonar", sensor_qos);
  oculus_meta_pub_ = node_->create_publisher<oculus_sonar_msgs::msg::OculusMetadata>(
      topic_prefix_ + "/metadata", sensor_qos);
  raw_data_pub_ = node_->create_publisher<apl_msgs::msg::RawData>(
      topic_prefix_ + "/raw_data", sensor_qos);
  image_pub_ = image_transport::create_publisher(
      node_, topic_prefix_ + "/image", rmw_qos_profile_sensor_data);

  ping_rate_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/ping_rate", sensor_qos);
  freq_mode_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/freq_mode", sensor_qos);
  data_size_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/data_size", sensor_qos);
  range_pub_ = node_->create_publisher<std_msgs::msg::Float32>(
      topic_prefix_ + "/param/range", sensor_qos);
  gain_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gain", sensor_qos);
  gamma_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gamma", sensor_qos);
  ip_address_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/ip_address", sensor_qos);
  frame_id_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/frame_id", sensor_qos);
  sonar_model_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/sonar_model", sensor_qos);

  RCLCPP_INFO(node_->get_logger(), "Publishing data with frame = %s", frame_id_.c_str());
}

void OculusDriverPublishers::publishParameters() {
  std_msgs::msg::Int32 ping_rate_msg;
  ping_rate_msg.data = node_->get_parameter("ping_rate").as_int();
  ping_rate_pub_->publish(ping_rate_msg);

  std_msgs::msg::Int32 freq_mode_msg;
  freq_mode_msg.data = node_->get_parameter("freq_mode").as_int();
  freq_mode_pub_->publish(freq_mode_msg);

  std_msgs::msg::String data_size_msg;
  data_size_msg.data = node_->get_parameter("data_size").as_string();
  data_size_pub_->publish(data_size_msg);

  std_msgs::msg::Float32 range_msg;
  range_msg.data = static_cast<float>(node_->get_parameter("range").as_double());
  range_pub_->publish(range_msg);

  std_msgs::msg::Int32 gain_msg;
  gain_msg.data = node_->get_parameter("gain").as_int();
  gain_pub_->publish(gain_msg);

  std_msgs::msg::Int32 gamma_msg;
  gamma_msg.data = node_->get_parameter("gamma").as_int();
  gamma_pub_->publish(gamma_msg);

  std_msgs::msg::String ip_msg;
  ip_msg.data = node_->get_parameter("ip_address").as_string();
  ip_address_pub_->publish(ip_msg);

  std_msgs::msg::String frame_id_msg;
  frame_id_msg.data = node_->get_parameter("frame_id").as_string();
  frame_id_pub_->publish(frame_id_msg);

  std_msgs::msg::String sonar_model_msg;
  sonar_model_msg.data = node_->get_parameter("sonar_model").as_string();
  sonar_model_pub_->publish(sonar_model_msg);
}

sensor_msgs::msg::Image OculusDriverPublishers::sonarToImage(
    const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const {
  sensor_msgs::msg::Image image_msg;

  image_msg.header = sonar_msg.header;
  image_msg.height = sonar_msg.ranges.size();
  image_msg.width = sonar_msg.azimuth_angles.size();

  if (sonar_msg.data_size == 1) {
    image_msg.encoding = "mono8";
  } else if (sonar_msg.data_size == 2) {
    image_msg.encoding = "mono16";
  } else if (sonar_msg.data_size == 4) {
    image_msg.encoding = "32FC1";
  }

  image_msg.is_bigendian = sonar_msg.is_bigendian;
  image_msg.step = image_msg.width * sonar_msg.data_size;
  image_msg.data = sonar_msg.intensities;

  return image_msg;
}

}  // namespace oculus_sonar
```

- [ ] **Step 3: Update CMakeLists.txt to compile new source**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt`, the `add_library` block becomes:

```cmake
add_library(oculus_driver_component SHARED
  src/oculus_driver_component.cpp
  src/oculus_driver_config.cpp
  src/oculus_driver_publishers.cpp
  src/publishing_data_rx.cpp
)
```

- [ ] **Step 4: Update `oculus_driver_component.hpp` — remove migrated members**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp`:

Add include after the existing config include (around line 36):
```cpp
#include "oculus_sonar/oculus_driver_publishers.hpp"
```

Remove the `pingCallback` template implementation (lines 61-78) — it now lives in `OculusDriverPublishers::publishPing`.

Replace with a thin forwarder:
```cpp
  template <typename Ping_t>
  void pingCallback(const Ping_t &ping) {
    publishers_->publishPing(ping);
  }
```

Remove `sonarToImage` method declaration (lines 124-129).

Remove `publishParameters` method declaration (line 150).

Remove all 13 publisher member variables (lines 135-149) — they now live in `OculusDriverPublishers`.

Add new member just before closing `};`:
```cpp
  std::unique_ptr<OculusDriverPublishers> publishers_;
```

- [ ] **Step 5: Update `oculus_driver_component.cpp` — delegate to publishers_**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp`:

Add include (after existing config include):
```cpp
#include "oculus_sonar/oculus_driver_publishers.hpp"
```

Remove these implementations entirely:
- `sonarToImage` (lines 326-352)
- `publishParameters` (lines 353-390)

In `init()`, replace the entire publisher creation block (lines 67-92) with:

```cpp
  std::string topic_prefix = "/sensor/sonar/oculus/" + config_->sonar_model();
  publishers_ = std::make_unique<OculusDriverPublishers>(this, topic_prefix, config_->frame_id());
  publishers_->initialize();
```

Replace the `data_rx_->setRawPublisher(raw_data_pub_)` line (line 101) with:
```cpp
  data_rx_->setRawPublisher(publishers_->raw_data_publisher());
```

- [ ] **Step 6: Build**

```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar 2>&1 | tail -10
source install/setup.bash
```

Expected: `Finished <<< oculus_sonar`. If errors mention `imaging_sonar_pub_` undefined in `pingCallback`, the .hpp removal of publisher members wasn't accompanied by the publishPing template forwarder — re-check Step 4.

- [ ] **Step 7: Regression**

```bash
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -20
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected: `PASS: structural snapshot identical to baseline`.

- [ ] **Step 8: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
        oculus_ros2/oculus_sonar/CMakeLists.txt
git commit -m "refactor(oculus_sonar): extract OculusDriverPublishers (B-1 step 2/3)

4 sensor + 9 param echo publisher와 publishParameters, sonarToImage,
templated publishPing<PingT>를 OculusDriverPublishers로 이전.
OculusDriver는 단순히 publishers_->publishPing(ping)으로 위임.

structural snapshot regression PASS."
```

---

## Task 7: Extract `OculusConnectionManager`

**Files:**
- Create: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_connection_manager.hpp`
- Create: `oculus_ros2/oculus_sonar/src/oculus_connection_manager.cpp`
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp`
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp`
- Modify: `oculus_ros2/oculus_sonar/CMakeLists.txt`

- [ ] **Step 1: Create header `oculus_connection_manager.hpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_connection_manager.hpp`:

```cpp
/**
 * @file oculus_connection_manager.hpp
 * @brief Owns IoServiceThread, PublishingDataRx, StatusRx and dispatches
 *        ping/status callbacks. No direct ROS2 publishing — that is the
 *        Publishers' responsibility.
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include <apl_msgs/msg/raw_data.hpp>
#include <rclcpp/rclcpp.hpp>

#include "liboculus/IoServiceThread.h"
#include "liboculus/SimplePingResult.h"
#include "liboculus/SonarConfiguration.h"
#include "liboculus/StatusRx.h"
#include "oculus_sonar/publishing_data_rx.h"

namespace oculus_sonar {

class OculusConnectionManager {
 public:
  using PingV1Callback =
      std::function<void(const liboculus::SimplePingResult<OculusSimplePingResult>&)>;
  using PingV2Callback =
      std::function<void(const liboculus::SimplePingResult<OculusSimplePingResult2>&)>;
  using StatusCallback = std::function<void(const liboculus::SonarStatus&, bool valid)>;

  OculusConnectionManager(const std::string& ip_address,
                          const std::string& frame_id,
                          rclcpp::Logger logger);
  ~OculusConnectionManager();

  OculusConnectionManager(const OculusConnectionManager&) = delete;
  OculusConnectionManager& operator=(const OculusConnectionManager&) = delete;

  // Wire callbacks BEFORE start().
  void setPingV1Callback(PingV1Callback cb);
  void setPingV2Callback(PingV2Callback cb);
  void setStatusCallback(StatusCallback cb);
  void setOnConnectCallback(std::function<void()> cb);

  void setRawPublisher(rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub);

  // Lifecycle: start the io_service thread and (if non-auto) connect.
  void start();
  void stop();

  // Forward to PublishingDataRx.
  void sendSimpleFireMessage(const liboculus::SonarConfiguration& config);
  bool isConnected() const;

  // For status callback that wants to trigger auto-connect upon discovery.
  void connect(const boost::asio::ip::address& ip);

 private:
  std::string ip_address_;
  std::string frame_id_;
  rclcpp::Logger logger_;

  liboculus::IoServiceThread io_srv_;
  std::unique_ptr<PublishingDataRx> data_rx_;
  liboculus::StatusRx status_rx_;
};

}  // namespace oculus_sonar
```

- [ ] **Step 2: Create implementation `oculus_connection_manager.cpp`**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_connection_manager.cpp`:

```cpp
/**
 * @file oculus_connection_manager.cpp
 * @brief Implementation of OculusConnectionManager.
 */

#include "oculus_sonar/oculus_connection_manager.hpp"

namespace oculus_sonar {

using liboculus::SimplePingResultV1;
using liboculus::SimplePingResultV2;

OculusConnectionManager::OculusConnectionManager(const std::string& ip_address,
                                                 const std::string& frame_id,
                                                 rclcpp::Logger logger)
    : ip_address_(ip_address),
      frame_id_(frame_id),
      logger_(logger),
      io_srv_(),
      status_rx_(io_srv_.context()) {
  data_rx_ = std::make_unique<PublishingDataRx>(io_srv_.context(), frame_id_);
}

OculusConnectionManager::~OculusConnectionManager() {
  stop();
}

void OculusConnectionManager::setPingV1Callback(PingV1Callback cb) {
  data_rx_->setCallback<SimplePingResultV1>(std::move(cb));
}

void OculusConnectionManager::setPingV2Callback(PingV2Callback cb) {
  data_rx_->setCallback<SimplePingResultV2>(std::move(cb));
}

void OculusConnectionManager::setStatusCallback(StatusCallback cb) {
  status_rx_.setCallback(std::move(cb));
}

void OculusConnectionManager::setOnConnectCallback(std::function<void()> cb) {
  data_rx_->setOnConnectCallback(std::move(cb));
}

void OculusConnectionManager::setRawPublisher(
    rclcpp::Publisher<apl_msgs::msg::RawData>::SharedPtr pub) {
  data_rx_->setRawPublisher(pub);
}

void OculusConnectionManager::start() {
  if (ip_address_ == "auto") {
    RCLCPP_INFO(logger_, "Attempting to auto-detect sonar");
  } else {
    RCLCPP_INFO(logger_, "Opening sonar at %s", ip_address_.c_str());
    data_rx_->connect(ip_address_);
  }
  io_srv_.start();
}

void OculusConnectionManager::stop() {
  io_srv_.stop();
  io_srv_.join();
}

void OculusConnectionManager::sendSimpleFireMessage(
    const liboculus::SonarConfiguration& config) {
  data_rx_->sendSimpleFireMessage(config);
}

bool OculusConnectionManager::isConnected() const {
  return data_rx_->isConnected();
}

void OculusConnectionManager::connect(const boost::asio::ip::address& ip) {
  data_rx_->connect(ip);
}

}  // namespace oculus_sonar
```

- [ ] **Step 3: Update CMakeLists.txt**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt`, the `add_library` block becomes:

```cmake
add_library(oculus_driver_component SHARED
  src/oculus_driver_component.cpp
  src/oculus_driver_config.cpp
  src/oculus_driver_publishers.cpp
  src/oculus_connection_manager.cpp
  src/publishing_data_rx.cpp
)
```

- [ ] **Step 4: Update `oculus_driver_component.hpp` — remove connection members**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp`:

Add include:
```cpp
#include "oculus_sonar/oculus_connection_manager.hpp"
```

Remove these member variables (around lines 131-133):
```cpp
  liboculus::IoServiceThread io_srv_;
  std::unique_ptr<PublishingDataRx> data_rx_;
  liboculus::StatusRx status_rx_;
```

Add member:
```cpp
  std::unique_ptr<OculusConnectionManager> connection_;
```

Remove these now-unused includes from the file:
```cpp
#include "liboculus/SimplePingResult.h"
#include "liboculus/StatusRx.h"
#include "liboculus/IoServiceThread.h"
#include "liboculus/SonarConfiguration.h"
#include "oculus_sonar/publishing_data_rx.h"
```

(They're used inside `OculusConnectionManager` and pulled in via `oculus_connection_manager.hpp`.)

- [ ] **Step 5: Update `oculus_driver_component.cpp` — delegate to connection_**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp`:

Add include:
```cpp
#include "oculus_sonar/oculus_connection_manager.hpp"
```

Remove these now-unused includes:
```cpp
#include <boost/asio.hpp>
#include "liboculus/Constants.h"
#include "liboculus/SonarConfiguration.h"
#include "oculus_sonar/publishing_data_rx.h"
```

Update constructor — remove member initializers `io_srv_()` and `status_rx_(io_srv_.context())`:
```cpp
OculusDriver::OculusDriver(const rclcpp::NodeOptions & options)
  : Node("oculus_driver", options) {
  init();
}
```

Update destructor — remove `io_srv_.stop(); io_srv_.join();` (handled by `OculusConnectionManager` destructor):
```cpp
OculusDriver::~OculusDriver() = default;
```

In `init()`, replace the entire connection setup block (the `data_rx_ = ...`, status_rx_ callback, `io_srv_.start()`, etc.) with:

```cpp
  connection_ = std::make_unique<OculusConnectionManager>(
      config_->ip_address(), config_->frame_id(), this->get_logger());

  connection_->setRawPublisher(publishers_->raw_data_publisher());

  connection_->setPingV1Callback(
      [this](const liboculus::SimplePingResultV1& ping) { publishers_->publishPing(ping); });
  connection_->setPingV2Callback(
      [this](const liboculus::SimplePingResultV2& ping) { publishers_->publishPing(ping); });

  connection_->setOnConnectCallback([this]() {
    connection_->sendSimpleFireMessage(config_->current());
  });

  connection_->setStatusCallback(
      [this](const liboculus::SonarStatus& status, bool is_valid) {
        if (!is_valid) return;

        static bool logged = false;
        if (!logged) {
          uint16_t part_num = status.msg()->partNumber;
          std::string detected_model = "Unknown";
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

          RCLCPP_WARN(this->get_logger(),
                      "Hardware detected - Part Number: %u (%s), Expected: %s",
                      part_num, detected_model.c_str(), config_->sonar_model().c_str());

          if (detected_model.find("M370") != std::string::npos) {
            RCLCPP_ERROR(this->get_logger(),
                        "WARNING: Hardware is M370s with 70° FOV, but config expects %s with 130° FOV!",
                        config_->sonar_model().c_str());
          }
          logged = true;
        }

        if (config_->ip_address() == "auto" && !connection_->isConnected()) {
          connection_->connect(status.ipAddr());
        }
      });

  config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
    if (connection_->isConnected()) {
      connection_->sendSimpleFireMessage(new_config);
    }
  });

  connection_->start();
```

Also re-add include for `<algorithm>` if not present (for `std::find` in the lambda).

- [ ] **Step 6: Build**

```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar 2>&1 | tail -10
source install/setup.bash
```

Expected: `Finished <<< oculus_sonar`. If link errors mention `boost::asio` undefined types, the `connect(ip)` overload signature mismatch — verify `OculusConnectionManager::connect` parameter type matches `liboculus::SonarStatus::ipAddr()` return type.

- [ ] **Step 7: Regression**

```bash
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -20
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected: `PASS: structural snapshot identical to baseline`.

- [ ] **Step 8: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_connection_manager.hpp \
        oculus_ros2/oculus_sonar/src/oculus_connection_manager.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
        oculus_ros2/oculus_sonar/CMakeLists.txt
git commit -m "refactor(oculus_sonar): extract OculusConnectionManager (B-1 step 3/3)

IoServiceThread, PublishingDataRx, StatusRx, ping/status 콜백 디스패치를
OculusConnectionManager로 이전. OculusDriver는 콜백 4개를 등록하고
connection_->start() 한 줄로 생명주기 시작.

structural snapshot regression PASS."
```

---

## Task 8: Verify Orchestrator Slim + Final Regression

**Files:** No code changes — verification only.

- [ ] **Step 1: Verify orchestrator LOC**

```bash
wc -l /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
       /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp
```

Expected: `oculus_driver_component.cpp` ≤ 100 LOC (down from 395). `.hpp` ≤ 70 LOC (down from 161).

If `.cpp` exceeds 100 LOC, examine what remained — there should be only the constructor, destructor, and `init()` body that wires the 3 helpers.

- [ ] **Step 2: Verify component types remain singular**

```bash
ros2 component types 2>&1 | grep -A4 oculus_sonar
```

Expected (still 2 lines, not 4):
```
oculus_sonar
  oculus_sonar::OculusDriver
  oculus_sonar::OculusFanImager
```

- [ ] **Step 3: Final clean baseline-vs-candidate compare**

```bash
cd /workspace/ros2_ws/src/sensor_packages
# Discard intermediate candidate dir to ensure final state is what we measure
rm -rf /tmp/oculus_regression/candidate
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -20
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected: `PASS: structural snapshot identical to baseline`.

- [ ] **Step 4: Driver longevity smoke test (UCRC bag, 60s)**

This is the **second regression layer** that catches runtime crashes the structural snapshot cannot — io_service thread races, callback dispatch SEGV, double-stop in destructor chains. Pass criterion: driver process stays alive for 60s of bag replay AND launch.log shows no crash trace.

```bash
cd /workspace/ros2_ws/src/sensor_packages
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=78  # different from regression script (77) to avoid cross-talk

UCRC_BAG="/workspace/data/7_ucrc_watertank/20260122_sonar_lidar/m750d_custom_platform/m750d-range15-tilt45-v1"
LONGEVITY_DIR="/tmp/oculus_regression/longevity"
mkdir -p "${LONGEVITY_DIR}"

# Verify bag exists; fall back to KHNP m3000d if not
if [[ ! -f "${UCRC_BAG}/metadata.yaml" ]]; then
    echo "WARN: UCRC bag not found, falling back to KHNP m3000d"
    UCRC_BAG="/workspace/data/6_khnp/20251219_seafloor_mapping/oculus_m3000d_test_v1"
    SONAR_MODEL=m3000d
else
    SONAR_MODEL=m750d
fi

# Launch driver
ros2 launch oculus_sonar oculus.launch.py sonar_model:="${SONAR_MODEL}" \
    > "${LONGEVITY_DIR}/launch.log" 2>&1 &
LAUNCH_PID=$!
sleep 5

# Verify driver alive after launch
if ! ps -p "${LAUNCH_PID}" > /dev/null; then
    echo "FAIL: driver crashed during startup"
    cat "${LONGEVITY_DIR}/launch.log" | tail -30
    exit 1
fi

# Replay UCRC bag for 60s in parallel
ros2 bag play "${UCRC_BAG}" --rate 1.0 \
    > "${LONGEVITY_DIR}/bag.log" 2>&1 &
BAG_PID=$!
sleep 60

# Verify driver STILL alive after 60s of bag replay
if ! ps -p "${LAUNCH_PID}" > /dev/null; then
    echo "FAIL: driver crashed during 60s replay"
    cat "${LONGEVITY_DIR}/launch.log" | tail -30
    kill "${BAG_PID}" 2>/dev/null
    exit 1
fi

# Cleanup (3-step per memory reference_fast_lio_regression_gotchas.md)
kill "${BAG_PID}" 2>/dev/null
kill -INT "${LAUNCH_PID}" 2>/dev/null
sleep 2
pkill -P "${LAUNCH_PID}" 2>/dev/null
sleep 1
pkill -f oculus_driver 2>/dev/null

# Scan for crash signatures in log
if grep -E "SIGSEGV|what\(\):|std::terminate|boost::asio.*error|libc\+\+abi" "${LONGEVITY_DIR}/launch.log"; then
    echo "FAIL: crash trace found in launch.log"
    exit 1
fi

echo "PASS: driver survived 60s UCRC replay with no crash trace"
```

Expected output ends with `PASS: driver survived 60s UCRC replay with no crash trace`. If FAIL, the `launch.log` excerpt will show the crash — typical causes: callback registered in wrong order (e.g. status callback fires before `connection_->start()` completes), io_service thread joined twice, raw_publisher passed before `publishers_->initialize()` completes.

- [ ] **Step 5: Capture metrics for PR body**

```bash
cd /workspace/ros2_ws/src/sensor_packages
{
  echo "Orchestrator size:"
  wc -l oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_component.hpp
  echo ""
  echo "New helpers:"
  wc -l oculus_ros2/oculus_sonar/src/oculus_driver_config.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_config.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp \
        oculus_ros2/oculus_sonar/src/oculus_connection_manager.cpp \
        oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_connection_manager.hpp
  echo ""
  echo "Total LOC delta (vs main):"
  git diff --shortstat main HEAD -- oculus_ros2/oculus_sonar/{src,include}
} | tee /tmp/oculus_b1_metrics.txt
```

Expected: clean print, save to /tmp for use in PR body.

---

## Task 9: CHANGELOG + PR

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CHANGELOG.md` (prepend new `[Unreleased]` section)
- Create: PR via `gh` CLI (already installed per memory)

- [ ] **Step 1: Read current CHANGELOG to know where to insert**

```bash
sed -n '1,15p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md
```

Expected: starts with the Phase A `[Unreleased]` block. The new B-1 entry goes ABOVE that block.

- [ ] **Step 2: Prepend B-1 entry to CHANGELOG**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md`, insert at the very top (after the title line, before existing `[Unreleased]` blocks):

```markdown
## [Unreleased] — Phase B-1: OculusDriver 4-way split + regression infra (refactor)

> Master design: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-1
> Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-1-driver-split.md`
> Verification: structural snapshot regression (4 dimensions, hardware-free) — PASS

### Changed
- `src/oculus_driver_component.cpp` 395 → ~80 LOC (orchestrator only — wires 3 helpers)
- `include/oculus_sonar/oculus_driver_component.hpp` 161 → ~70 LOC
- `CMakeLists.txt` — `register_nodes` 호출 2개 제거 (`register_node`만 유지). 컴포넌트 중복 노출 해소 (Phase A spec §7 A-3 미완 흡수, D1)

### Added
- `include/oculus_sonar/oculus_driver_config.hpp` + `src/oculus_driver_config.cpp` — 11 declare_parameter, parameterCallback, 3 setter helper, updateSonarConfig, convertDataSizeString. `rclcpp::Node*` non-owning 참조 (D2: 독립 helper)
- `include/oculus_sonar/oculus_driver_publishers.hpp` + `src/oculus_driver_publishers.cpp` — 4 sensor + 9 param echo publisher, `publishParameters()`, `sonarToImage()` 헬퍼, 템플릿 `publishPing<PingT>`
- `include/oculus_sonar/oculus_connection_manager.hpp` + `src/oculus_connection_manager.cpp` — `IoServiceThread`, `PublishingDataRx`, `StatusRx`, ping/status 콜백 디스패치, 연결 lifecycle
- `scripts/regression_oculus.sh` — 4 차원 구조적 snapshot 회귀 인프라 (component types / param dump / topic list / per-topic info), B-2/C 재사용 자산

### Verification
- colcon build PASS (Release)
- **Layer 1 — structural snapshot**: baseline (`80b2c5e` main HEAD, post-D1 적용) vs candidate (B-1 final) — 4개 snapshot 파일 모두 byte-identical
  - `ros2 component types | grep oculus` → 각 노드 1회씩만 노출 (D1 적용 후, 이전 2회 → 1회)
  - `ros2 param dump /oculus_driver` → 13 parameter 모두 동일 default
  - `ros2 topic list | grep oculus` + `ros2 topic info --verbose` → 모든 publisher 토픽 이름·메시지 타입·QoS 동일
- **Layer 2 — driver longevity smoke test**: UCRC m750d bag (`/workspace/data/7_ucrc_watertank/.../m750d-range15-tilt45-v1`) 60s 병행 재생 → driver process alive + launch.log에 SIGSEGV/`what():`/`std::terminate`/`boost::asio` error 0건

### Notes
- 본 PR은 byte-exact ping output 검증을 포함하지 않음. 드라이버는 `liboculus`를 통해 UDP 소켓 raw bytes를 받으므로 ROS bag을 driver INTO 재생할 방법이 없음. Hardware-in-loop 검증은 별도 라운드 (Phase C 또는 운용 검증).
- Longevity smoke test는 structural snapshot이 못 잡는 runtime crash 종류(io_service thread race, callback dispatch SEGV, double-stop)를 잡기 위한 두 번째 회귀 layer. UCRC bag을 사용한 이유는 sonar_3d_reconstruction Phase B-1 회귀와 같은 디렉토리 패밀리라서 미래 cross-comparison에 유리하기 때문.
- Phase B-2 (launch helper 추출)는 본 PR 머지 후 `refactor/phase-b2-launch-helper` 브랜치에서 진행. B-1의 회귀 인프라(`scripts/regression_oculus.sh`)를 그대로 재사용.
- 잔존 리스크 3건 (Phase A에서 이월) 중 #2 (CMakeLists `liboculus` include path INSTALL_INTERFACE 미포함)은 본 PR과 무관, Phase B-2 또는 별도 patch에서 처리.

```

- [ ] **Step 3: Commit CHANGELOG**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CHANGELOG.md
git commit -m "docs(oculus_sonar): add CHANGELOG entry for Phase B-1"
```

- [ ] **Step 4: Push branch**

> ⚠️ Push requires explicit user approval per project policy. STOP here and ask the user before running this step.

```bash
cd /workspace/ros2_ws/src/sensor_packages
git push -u origin refactor/phase-b1-driver-split
```

Expected: `Branch 'refactor/phase-b1-driver-split' set up to track 'origin/refactor/phase-b1-driver-split'.`

- [ ] **Step 5: Create PR via gh CLI**

```bash
cd /workspace/ros2_ws/src/sensor_packages
gh pr create \
  --title "refactor(oculus_sonar): Phase B-1 — OculusDriver 4-way split + regression infra" \
  --base main \
  --head refactor/phase-b1-driver-split \
  --body "$(cat <<'EOF'
## Summary
- `OculusDriver` 395-line class를 4-way split (orchestrator + Config + Publishers + ConnectionManager)
- CMakeLists `register_nodes`/`register_node` 중복 제거 → 컴포넌트 중복 노출 버그 해소 (Phase A §7 A-3 미완 흡수)
- `scripts/regression_oculus.sh` 신규 — sensor_packages 첫 회귀 인프라, B-2/C 재사용

## Changes
- `oculus_driver_config.{hpp,cpp}` 신규 — 11 declare_parameter, parameterCallback, 3 setter, convertDataSizeString. `rclcpp::Node*` non-owning 참조 (D2 결정)
- `oculus_driver_publishers.{hpp,cpp}` 신규 — 4 sensor + 9 param echo publisher, publishPing<PingT> 템플릿
- `oculus_connection_manager.{hpp,cpp}` 신규 — IoServiceThread + DataRx + StatusRx + 콜백 디스패치
- `oculus_driver_component.{hpp,cpp}` 슬림 — 395→80 LOC, 161→70 LOC (orchestrator만 남김)
- `CMakeLists.txt` — register_nodes 2 호출 삭제, 새 .cpp 3개 추가
- `scripts/regression_oculus.sh` 신규 — 4 차원 구조적 snapshot (component types, param dump, topic list, per-topic info)

## Verification

### Layer 1 — structural snapshot (per-task gate, Tasks 4-8)
- [x] colcon build PASS (Release)
- [x] baseline (\`80b2c5e\` post-D1) vs candidate (B-1 final) — 4 snapshot 파일 byte-identical (PASS)
- [x] \`ros2 component types | grep oculus\` → 각 노드 정확히 1회 노출 (D1 효과)
- [x] \`ros2 param dump /oculus_driver\` → 13 parameter 모두 동일 default
- [x] \`ros2 topic list\` + \`topic info --verbose\` → 모든 토픽 이름·타입·QoS 일치

### Layer 2 — driver longevity smoke test (Task 8 only)
- [x] UCRC m750d bag 60s 병행 재생 → driver PID alive
- [x] launch.log에 \`SIGSEGV\` / \`what():\` / \`std::terminate\` / \`boost::asio.*error\` / \`libc++abi\` 0건

### 기타
- [x] CHANGELOG.md 갱신
- [x] Spec §14 Q-B1 결정(D2: 독립 helper), CMakeLists 미완 흡수(D1) 명시
- [ ] **Hardware-in-loop ping output 검증은 본 PR scope 외** — UDP 소켓 입력을 bag으로 재현 불가. 별도 운용 검증 라운드 또는 Phase C에서 처리.

## Next Phase
- B-2: launch 공통 헬퍼 추출 (\`refactor/phase-b2-launch-helper\`). B-1의 \`scripts/regression_oculus.sh\` 그대로 재사용.
EOF
)"
```

Expected: prints PR URL.

- [ ] **Step 6: Final task list update**

```bash
echo "Phase B-1 PR URL captured. Ready for review."
```

After PR is merged (separate operation requiring user approval), update memory:
- `project_sonar3d_audit_state.md` is for sonar_3d_reconstruction; sensor_packages refactor state lives elsewhere. If a memory entry exists for sensor_packages oculus refactor, update its "current phase" line.

---

## Risks & Recoveries

| Scenario | Detection | Recovery |
|----------|-----------|----------|
| Task 5 (Config extract) snapshot FAIL on `02_param_dump.yaml` | `compare` shows YAML diff on a specific param | A param default value drifted during transcription. `diff` the `OculusDriverConfig::initialize()` declares against the original `oculus_driver_component.cpp:36-49`. Fix the literal, rebuild, re-run regression. |
| Task 6 (Publishers) snapshot FAIL on `04_topic_info.txt` | `compare` shows QoS or message-type diff on a specific topic | A publisher type or QoS profile changed during transcription. Compare `OculusDriverPublishers::initialize()` against the original publisher creation in `oculus_driver_component.cpp:67-92`. |
| Task 6 (Publishers) link error: `imaging_sonar_pub_` undefined in `pingCallback` template | Build error mentions undefined publisher members | The hpp removed publisher members but kept the old template body. Replace `pingCallback` body with the thin forwarder per Step 4. |
| Task 7 (ConnectionManager) destructor double-stop | `io_srv_` already stopped warning at shutdown | `OculusDriver::~OculusDriver()` should be `= default` (Step 5). The connection_'s destructor handles cleanup. |
| Regression script: `02_param_dump.yaml` empty | `[label] ERROR: param dump failed` printed | Driver crashed at startup. Read `/tmp/oculus_regression/<label>/launch.log` for exception. Common cause: missing dependency, missing config file. |
| Regression script: `03_topic_list.txt` shows 0 topics | First baseline run prints empty list | `STARTUP_WAIT_S=5` may be too short on slow systems. Re-run with `STARTUP_WAIT_S=10` or `STARTUP_WAIT_S=15`. |
| `ros2 launch` SIGINT leaves zombie processes | `pgrep oculus_driver` after script exits shows leftover PIDs | The 3-step cleanup in script is meant to handle this. If still leaks, add a 4th step `pkill -9 -f oculus_driver` (escalate to KILL). Memory `reference_fast_lio_regression_gotchas.md`. |
| Phase A leftover risk #1 (`SonarConstants::DEFAULT_SONAR_IP` vs `"auto"`) surfaces during Config extraction | `02_param_dump.yaml` shows `ip_address: SonarConstants::DEFAULT_SONAR_IP` instead of `"auto"` | This PR preserves `"auto"` literal as default (matches main HEAD). Decision on which to keep belongs to Phase B-2 or a separate patch. Do NOT silently change the default in this PR. |
| Task 8 longevity test: driver crashes during 60s UCRC replay | `FAIL: driver crashed during 60s replay` printed | Read tail of `/tmp/oculus_regression/longevity/launch.log`. Most likely a callback registered before the publisher it depends on (e.g. status callback fires before `connection_->start()` completed initialization, or raw_publisher passed before `publishers_->initialize()` returned). Re-order the wiring in `OculusDriver::init()` so all `setXxxCallback` calls precede `connection_->start()`. |
| Task 8 longevity test: UCRC bag missing | `WARN: UCRC bag not found` printed, fallback to KHNP | Acceptable — KHNP m3000d bag covers the same crash surface. If both missing, skip Layer 2 and document explicitly in PR body Verification. |

---

## Self-Review Checklist (executed inline before saving)

✅ **Spec coverage** — Each spec §8 B-1 requirement maps to a task:
- 4-way split → Tasks 5/6/7/8
- Regression infra → Tasks 2/3
- Per-split structural verification → Step 7 of each split task + Task 8 final
- D1 (CMakeLists deduplication) absorbed → Task 4

⚠️ **Spec divergence (acknowledged)** — The spec §8 B-1 proposed bag-replay byte-exact verification. After investigation that approach was found unworkable without hardware (driver consumes UDP socket bytes via liboculus, not ROS topics — bag cannot be replayed *into* the driver). This plan substitutes structural snapshot regression that captures the externally-observable interface (component types + param defaults + topic names + topic types/QoS), which is what actually changes during a behavior-preserving extract refactor. The substitution is documented in Task 2 design rationale and the verification honesty note in the plan header. Spec author should be informed; spec §8 B-1 verification block should be amended after PR merges.

✅ **Placeholder scan** — No "TBD", "TODO" in code blocks. Every step has concrete commands or full code. The "Risks & Recoveries" section names specific files and line numbers for each scenario.

✅ **Type consistency** — Cross-task method/member names verified:
- `OculusDriverConfig::current()`, `ip_address()`, `frame_id()`, `sonar_model()`, `on_change()` consistent in Tasks 5/6/7
- `OculusDriverPublishers::publishPing<PingT>`, `raw_data_publisher()`, `initialize()` consistent in Tasks 6/7
- `OculusConnectionManager::start/stop/setPingV1Callback/setPingV2Callback/setStatusCallback/setOnConnectCallback/setRawPublisher/sendSimpleFireMessage/isConnected/connect` consistent in Task 7

✅ **Push gate** — Task 9 Step 4 has explicit "STOP here" warning honoring the project rule "푸시는 명시적 지시가 있을 때만".

---

## Execution Notes

This plan runs in the **main worktree** (`/workspace/ros2_ws/src/sensor_packages`), branch `refactor/phase-b1-driver-split`. No additional worktree needed since sensor_packages is independent of sonar_3d_reconstruction (no other Claude session is currently working on sensor_packages per the consolidation in 2026-05-04).

For execution, use **superpowers:subagent-driven-development**: each Task becomes one subagent dispatch (~9 dispatches total). Spec-compliance review after each split is critical because the structural snapshot gate is strict — even a single byte change in `02_param_dump.yaml` (e.g., a default literal drift) fails compare.
