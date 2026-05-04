# Phase C-2 — Parameter Echo Throttle + Latched QoS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the 9 `param/*` echo publishers out of the per-ping hot path into a 1 Hz `wall_timer`, switch their QoS to `KEEP_LAST(1) + RELIABLE + TRANSIENT_LOCAL` (latched), and trigger an immediate publish on each parameter update.

**Architecture:** Three-file edit (`oculus_driver_publishers.{hpp,cpp}` + `oculus_driver_component.cpp`). The existing `OculusDriverConfig::on_change` listener mechanism is reused to wire parameter updates into a public `republishParameters()` wrapper on the publisher. A `wall_timer(1s)` lives as a member of `OculusDriverPublishers`. An initial `publishParameters()` call at the end of `initialize()` seeds the TRANSIENT_LOCAL durability cache so subscribers connecting before the first timer tick still receive a value.

**Tech Stack:** ROS 2 Humble, rclcpp, rclcpp_components, std_msgs, ament_cmake, gtest (existing C-1 fixtures only — no new tests).

**Spec:** `/workspace/ros2_ws/src/sensor_packages/docs/superpowers/specs/2026-05-04-oculus-sonar-phase-c-2-throttle-latched-qos-design.md` (commit `1621718`)

**Branch:** `refactor/phase-c-2-throttle-latched-qos`
**Baseline:** `main @ 2032ef1` (post-C-1 merge)

---

## File Map

| File | Action | Net LOC |
|------|--------|---------|
| `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp` | Modify | +3 |
| `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp` | Modify | +6, −1 |
| `oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp` | Modify | +1 |
| `oculus_ros2/oculus_sonar/CHANGELOG.md` | Prepend C-2 section | +~30 |

No new files. No new test fixtures. No regression script edits — `scripts/regression_oculus.sh` is reused unmodified.

---

## Conventions

- All commands assume `cwd = /workspace/ros2_ws/src/sensor_packages` unless stated otherwise.
- Build/launch commands assume ROS 2 Humble sourced once at session start: `source /opt/ros/humble/setup.bash`.
- Workspace must be sourced after each colcon build: `source /workspace/ros2_ws/install/setup.bash`.
- Commit messages follow Conventional Commits. **No AI attribution** anywhere (commits, PR, files).
- Push gate: explicit user approval at Task 10 Step 5 before `git push`.

---

## Task 1: Capture baseline structural snapshot at `main @ 2032ef1`

**Files:**
- Read-only: `scripts/regression_oculus.sh`
- Output: `/tmp/oculus_regression/baseline/`

**Why first:** The candidate snapshot (Task 8) compares against this. Capturing baseline before any code change avoids re-builds later. The 9 param topics' QoS rows in `04_topic_info.txt` are the diff anchor — they must be `RELIABILITY=BEST_EFFORT` + `DURABILITY=VOLATILE` in baseline.

- [ ] **Step 1: Pre-clean any stale ROS daemon and component containers**

```bash
pkill -f component_container 2>/dev/null || true
ros2 daemon stop 2>/dev/null || true
sleep 5
```

Expected: silent (no errors). This avoids the DDS race documented in `reference_oculus_regression_dds_gotcha.md`.

- [ ] **Step 2: Build current branch (already at 2032ef1 + spec-only commit; same binary as main)**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar
```

Expected: `Summary: 1 package finished` PASS in ~10 s.

- [ ] **Step 3: Source workspace then run baseline snapshot**

```bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=99
cd /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar
STARTUP_WAIT_S=12 bash scripts/regression_oculus.sh baseline
```

Expected: ends with `[baseline] saved to /tmp/oculus_regression/baseline` followed by 4 sha256 lines for `01_component_types.txt`, `02_param_dump.yaml`, `03_topic_list.txt`, `04_topic_info.txt`.

- [ ] **Step 4: Verify baseline param topic QoS is BEST_EFFORT + VOLATILE**

```bash
grep -A4 "/param/range\b" /tmp/oculus_regression/baseline/04_topic_info.txt | head -20
```

Expected output contains:
```
Reliability QoS: BEST_EFFORT
Durability QoS: VOLATILE
```

If those two lines are not present for `/param/range`, STOP and investigate — baseline is wrong and Task 8 diff will be meaningless.

- [ ] **Step 5: Record baseline summary hashes**

```bash
cat /tmp/oculus_regression/baseline/summary.txt
```

Expected: 4 sha256 lines. Save these — Task 8 will compare against them. No commit (snapshot files are not checked in per refactor-workflow rule).

---

## Task 2: Add timer member and `republishParameters()` declaration to publishers header

**Files:**
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp`

- [ ] **Step 1: Add `republishParameters()` public declaration**

In `oculus_driver_publishers.hpp`, locate the public section (after the existing `publishPing` template, before the `private:` keyword). Add:

```cpp
  // Publishes all 9 parameter echoes once. Called by:
  //   1) wall_timer (steady-state ~1 Hz),
  //   2) on_change listener after a parameter update (immediate latch update),
  //   3) initialize() (latched seed for late-join subscribers).
  void republishParameters();
```

The exact insertion point is just before `private:`. The existing `publishPing<>()` template definition stays public.

- [ ] **Step 2: Add `param_publish_timer_` private member**

In the private section of the same class, after `image_transport::Publisher image_pub_;` and before the `rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr ping_rate_pub_;` declaration, add:

```cpp
  rclcpp::TimerBase::SharedPtr param_publish_timer_;
```

- [ ] **Step 3: Header-only compile check**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar 2>&1 | tail -5
```

Expected: build still PASSes (header changes alone don't break linking — `republishParameters` is declared but not yet defined, that's fine until callers exist).

Wait — a declaration without a definition will fail link if anything calls it. At this point nothing does (Task 6 wires the call). So pure header edit links fine.

- [ ] **Step 4: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp
git commit -m "refactor(oculus_sonar): add republishParameters declaration and timer member

Header-only step toward Phase C-2: declares the public wrapper that
the on_change listener will call, and reserves the wall_timer slot
that initialize() will populate. No behavior change yet."
```

---

## Task 3: Implement latched QoS, seed publish, and timer in `initialize()`

**Files:**
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp` (function `initialize()` only, lines 15–47)

- [ ] **Step 1: Add `latched_qos` after the existing `sensor_qos` line**

Open the file. Locate `auto sensor_qos = rclcpp::SensorDataQoS();` (currently line 16). Add immediately below it:

```cpp
  auto latched_qos = rclcpp::QoS(1).transient_local();
```

- [ ] **Step 2: Replace `sensor_qos` with `latched_qos` for the 9 param publishers**

The 9 param publisher creation calls span lines 27–44. For each `create_publisher<...>` call whose topic begins with `topic_prefix_ + "/param/"`, change the second argument from `sensor_qos` to `latched_qos`. The 4 sensor-data publishers (`/sonar`, `/metadata`, `/raw_data`, `/image`) keep `sensor_qos` and the image_transport `rmw_qos_profile_sensor_data` respectively.

After the edit, lines 27–44 read:

```cpp
  ping_rate_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/ping_rate", latched_qos);
  freq_mode_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/freq_mode", latched_qos);
  data_size_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/data_size", latched_qos);
  range_pub_ = node_->create_publisher<std_msgs::msg::Float32>(
      topic_prefix_ + "/param/range", latched_qos);
  gain_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gain", latched_qos);
  gamma_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gamma", latched_qos);
  ip_address_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/ip_address", latched_qos);
  frame_id_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/frame_id", latched_qos);
  sonar_model_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/sonar_model", latched_qos);
```

- [ ] **Step 3: Add seed publish + timer at end of `initialize()`**

After the existing `RCLCPP_INFO(node_->get_logger(), "Publishing data with frame = %s", frame_id_.c_str());` line and before the closing `}` of `initialize()`, add:

```cpp

  // Seed the TRANSIENT_LOCAL durability cache so subscribers connecting
  // before the first timer tick still receive a value via late-join replay.
  publishParameters();

  param_publish_timer_ = node_->create_wall_timer(
      std::chrono::seconds(1),
      [this]() { publishParameters(); });
```

The `<chrono>` header is already included via `oculus_driver_publishers.hpp`.

- [ ] **Step 4: Build PASS**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar 2>&1 | tail -5
```

Expected: `Summary: 1 package finished` PASS. If link error mentions `republishParameters` → it's declared (Task 2) but still undefined; that's expected until Task 4.

If link error mentions `republishParameters` is undefined: that link error only surfaces if some translation unit calls it. Nothing calls it yet, so the link should still succeed. If it doesn't, re-check Task 2 for a stray call.

- [ ] **Step 5: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp
git commit -m "refactor(oculus_sonar): switch param publishers to latched QoS + timer

In OculusDriverPublishers::initialize():
- Add rclcpp::QoS(1).transient_local() (RELIABLE + TRANSIENT_LOCAL +
  KEEP_LAST(1)) for the 9 /param/* publishers.
- Seed the TRANSIENT_LOCAL durability cache by calling
  publishParameters() once at end of initialize() so late-joining
  subscribers get a value via DDS replay.
- Register a 1 Hz wall_timer that re-invokes publishParameters().

Sensor-data publishers (/sonar, /metadata, /raw_data, /image)
keep SensorDataQoS — only the param echo channel changes."
```

---

## Task 4: Remove `publishParameters()` from `publishPing` hot path and define `republishParameters()` wrapper

**Files:**
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp` (the `publishPing` template body)
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp` (definition of `republishParameters`)

The `publishPing<>()` definition is in the **header** (it's a template). Editing the body of a template requires a header edit, not a source edit.

- [ ] **Step 1: Remove the `publishParameters()` line from `publishPing<>()`**

In `oculus_driver_publishers.hpp`, locate the `publishPing<>()` template (around lines 50–75). Find the line:

```cpp
    publishParameters();
```

(currently at hpp line 69, between the `oculus_meta_pub_->publish(meta);` line and the `const auto t_end = ...;` line).

Delete that one line entirely. After the edit, the relevant block of `publishPing<>()` reads:

```cpp
    oculus_sonar_msgs::msg::OculusMetadata meta;
    meta.header = sonar_msg.header;
    for (unsigned int i = 0; i < ping.gains().size(); i++) {
      meta.tvg.push_back(ping.gains().at(i));
    }
    oculus_meta_pub_->publish(meta);

    const auto t_end = std::chrono::steady_clock::now();
```

- [ ] **Step 2: Define `republishParameters()` in the source file**

In `oculus_driver_publishers.cpp`, after the existing `publishParameters()` definition (currently lines 49–85) and before `sonarToImage` (currently at line 87), add:

```cpp
void OculusDriverPublishers::republishParameters() {
  publishParameters();
}
```

This thin wrapper is the public entry point used by the on_change listener (Task 5). Keeping `publishParameters()` private preserves encapsulation while exposing the safe public surface.

- [ ] **Step 3: Build PASS**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar 2>&1 | tail -5
```

Expected: `Summary: 1 package finished` PASS.

- [ ] **Step 4: Run existing C-1 gtest fixtures (smoke check; no algorithm change so all 8 must still PASS)**

```bash
cd /workspace/ros2_ws && colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -15
```

Expected: `8 tests from PingToSonarImage` reported as PASS, summary `[ PASSED ] 8 tests` and `0 errors`.

If any fixture fails: STOP. C-2 is not supposed to touch `pingToSonarImage` — investigate immediately.

- [ ] **Step 5: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp
git commit -m "refactor(oculus_sonar): drop param publish from publishPing hot path

In publishPing<>() (header template), removed the
publishParameters() call. Per-ping work for the 9 /param/* echoes
is gone — they are now driven by the wall_timer registered in
initialize() (Task 3) and by the on_change listener wiring (Task 5).

Defined the public republishParameters() wrapper in the source
file so the component can call it without a friend declaration.

Existing 8 gtest fixtures from C-1 still PASS (no algorithm change)."
```

---

## Task 5: Wire `on_change` to call `republishParameters()` in component

**Files:**
- Modify: `oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp` (around line 93–97)

- [ ] **Step 1: Edit the on_change lambda**

Locate the existing `config_->on_change(...)` block (currently lines 93–97). The current lambda body has only the `sendSimpleFireMessage` call. Add `publishers_->republishParameters()` after the closing brace of the `if`:

```cpp
  config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
    if (connection_->isConnected()) {
      connection_->sendSimpleFireMessage(new_config);
    }
    publishers_->republishParameters();
  });
```

The new line publishes the 9 param echoes immediately on every parameter update so the latched cache stays current without waiting up to 1 s for the next timer tick.

- [ ] **Step 2: Build PASS**

```bash
cd /workspace/ros2_ws && colcon build --packages-select oculus_sonar 2>&1 | tail -5
```

Expected: `Summary: 1 package finished` PASS.

- [ ] **Step 3: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/src/oculus_driver_component.cpp
git commit -m "refactor(oculus_sonar): publish param echoes on parameter change

OculusDriver::init() registers an on_change listener with
OculusDriverConfig. Extended that lambda to call
publishers_->republishParameters() after the existing
sendSimpleFireMessage so the TRANSIENT_LOCAL durability cache
reflects the new parameter value within ~1 ms of the
parameter service callback returning, instead of waiting up
to 1 s for the next wall_timer tick."
```

---

## Task 6: Final clean-build + cumulative gtest verification

**Files:**
- None modified. This is a verification gate.

- [ ] **Step 1: Clean rebuild**

```bash
cd /workspace/ros2_ws
rm -rf build/oculus_sonar install/oculus_sonar
colcon build --packages-select oculus_sonar 2>&1 | tail -10
```

Expected: `Summary: 1 package finished` PASS, no warnings about unresolved external symbols.

- [ ] **Step 2: Run all oculus_sonar tests**

```bash
cd /workspace/ros2_ws
colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -30
```

Expected output excerpt:
```
[ PASSED ] 8 tests.
...
Summary: 1 package finished
  test: 1 ok
```

- [ ] **Step 3: Confirm test result file**

```bash
cat /workspace/ros2_ws/build/oculus_sonar/Testing/Temporary/LastTest.log | tail -40
```

Expected: 8 individual `Passed` lines for the C-1 fixtures (`ByteExact_8bit_FastPath`, `ByteExact_16bit_FastPath`, …, `ZeroSizeImage`, `NullDataPointer`).

- [ ] **Step 4: No commit** — verification only.

---

## Task 7: Capture candidate structural snapshot and diff against baseline

**Files:**
- Read-only: `scripts/regression_oculus.sh`
- Output: `/tmp/oculus_regression/candidate/` and stdout diff

- [ ] **Step 1: Pre-clean DDS daemon (same as Task 1 Step 1)**

```bash
pkill -f component_container 2>/dev/null || true
ros2 daemon stop 2>/dev/null || true
sleep 5
```

- [ ] **Step 2: Source workspace and capture candidate snapshot**

```bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=99
cd /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar
STARTUP_WAIT_S=12 bash scripts/regression_oculus.sh candidate
```

Expected: ends with `[candidate] saved to /tmp/oculus_regression/candidate` + 4 sha256 lines.

- [ ] **Step 3: Run compare**

```bash
bash scripts/regression_oculus.sh compare 2>&1 | tee /tmp/oculus_regression/c2_diff.txt
```

Expected: the script output begins with diffs for `04_topic_info.txt` (only) and ends with `FAIL: structural snapshot mismatch (see diffs above)`. The fail is **expected** because the QoS flip is intentional. The other 3 dimensions must be silent (no diff).

- [ ] **Step 4: Validate the diff is exactly the QoS flip on 9 param topics, nothing else**

```bash
grep -E "^[+-]" /tmp/oculus_regression/c2_diff.txt | grep -v "^---\|^+++" | head -60
```

Expected output: pairs of `-`/`+` lines for **only** the 9 param topics (`/param/ping_rate`, `/param/freq_mode`, `/param/data_size`, `/param/range`, `/param/gain`, `/param/gamma`, `/param/ip_address`, `/param/frame_id`, `/param/sonar_model`). Each topic shows two flipped lines:

```
- Reliability QoS: BEST_EFFORT
+ Reliability QoS: RELIABLE
- Durability QoS: VOLATILE
+ Durability QoS: TRANSIENT_LOCAL
```

If any other line differs (including the 4 sensor-data topics or any line in dimensions 1–3), STOP and investigate. Unexpected diff = unintended regression.

- [ ] **Step 5: Verify dimensions 1, 2, 3 are byte-identical**

```bash
diff /tmp/oculus_regression/baseline/01_component_types.txt /tmp/oculus_regression/candidate/01_component_types.txt && echo "01 IDENTICAL"
diff /tmp/oculus_regression/baseline/02_param_dump.yaml      /tmp/oculus_regression/candidate/02_param_dump.yaml      && echo "02 IDENTICAL"
diff /tmp/oculus_regression/baseline/03_topic_list.txt       /tmp/oculus_regression/candidate/03_topic_list.txt       && echo "03 IDENTICAL"
```

Expected: three `IDENTICAL` lines. Topic list must not change (no topics added or removed; only QoS profile changes inside `04_topic_info.txt`).

- [ ] **Step 6: Save the diff for the PR description (no commit)**

```bash
ls -la /tmp/oculus_regression/c2_diff.txt
```

This file gets attached/quoted in the PR description; it is not committed to the repo.

---

## Task 8: Auxiliary verification — `topic hz` (~1 Hz) and `topic echo --once` (instant)

**Files:**
- None modified. Verification only.

These checks confirm runtime behavior that the structural snapshot can't observe (publish frequency and late-join replay).

- [ ] **Step 1: Pre-clean DDS daemon**

```bash
pkill -f component_container 2>/dev/null || true
ros2 daemon stop 2>/dev/null || true
sleep 5
```

- [ ] **Step 2: Launch the driver in the background**

```bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=99
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d > /tmp/oculus_c2_launch.log 2>&1 &
LAUNCH_PID=$!
sleep 12
```

The driver will fail to connect to a sonar (no UDP source). That's fine — the param publishers and timer don't depend on the sonar connection.

- [ ] **Step 3: Verify steady-state ~1 Hz throttle on /param/range**

```bash
timeout 30 ros2 topic hz /sensor/sonar/oculus/m750d/param/range 2>&1 | tee /tmp/oculus_c2_hz.txt
```

Expected: log lines like `average rate: 1.0XX min: 0.99X max: 1.01X std dev: 0.00XXX`. Acceptance: `0.95 ≤ average ≤ 1.05`.

If average is dramatically off (e.g., > 5 Hz, suggesting publishParameters is still hot-pathed): STOP and investigate.

- [ ] **Step 4: Verify late-join replay via TRANSIENT_LOCAL**

```bash
timeout 5 ros2 topic echo --once /sensor/sonar/oculus/m750d/param/range 2>&1 | tee /tmp/oculus_c2_echo.txt
```

Expected: a single `data: <number>` line printed within ~1 second (typically <100 ms) — the value comes from the TRANSIENT_LOCAL durability cache populated by the seed `publishParameters()` call at end of `initialize()`.

If the command times out or returns no value: STOP. Either the seed call (Task 3 Step 3) didn't run, or the QoS profile didn't actually flip — re-check Task 3 and Task 7.

- [ ] **Step 5: Cleanup launch**

```bash
kill -INT "$LAUNCH_PID" 2>/dev/null || true
sleep 2
pkill -f oculus_m750d_container 2>/dev/null || true
ros2 daemon stop 2>/dev/null || true
```

- [ ] **Step 6: No commit** — the `tee`'d files (`hz.txt`, `echo.txt`) get summarized in the PR description, not committed.

---

## Task 9: Update CHANGELOG

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CHANGELOG.md` (prepend Phase C-2 section before existing C-1)

- [ ] **Step 1: Read the current top of CHANGELOG**

```bash
head -40 oculus_ros2/oculus_sonar/CHANGELOG.md
```

Expected: starts with `# Changelog` header, then the most recent entry is Phase C-1 (`## [Unreleased] — Phase C-1 …`).

- [ ] **Step 2: Insert C-2 section above the C-1 section**

Edit `oculus_ros2/oculus_sonar/CHANGELOG.md`. Find the line `## [Unreleased] — Phase C-1` (or whatever the most recent C-1 heading reads). Insert the following block immediately above it (preserve a blank line between sections):

```markdown
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

```

- [ ] **Step 3: Verify the file builds (markdown-only, no code)**

```bash
head -55 oculus_ros2/oculus_sonar/CHANGELOG.md
```

Expected: the new C-2 block appears at the top, followed by the existing C-1 block. Section ordering: C-2 → C-1 → B-2 → B-1 → A.

- [ ] **Step 4: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CHANGELOG.md
git commit -m "docs(oculus_sonar): record Phase C-2 in CHANGELOG"
```

---

## Task 10: Push gate, push, and open PR

**Files:**
- None modified. Coordination + push.

- [ ] **Step 1: Final repo status check**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git status
git log --oneline main..HEAD
```

Expected: working tree clean. Commits ahead of main:
1. spec doc (`1621718` — already on branch)
2. publishers.hpp header changes
3. publishers.cpp QoS+timer+seed
4. publishers.{hpp,cpp} publishPing edit + republishParameters definition
5. component.cpp on_change wire
6. CHANGELOG

(Total ~6 commits; squash merge will collapse them into one C-2 commit on main.)

- [ ] **Step 2: Print squash-merge subject for the PR title**

```bash
echo "refactor(oculus_sonar): Phase C-2 — parameter echo throttle + latched QoS"
```

This is the PR title. Use it verbatim.

- [ ] **Step 3: Compose PR body**

Save the PR body (you'll pass it to `gh pr create` in Step 6) — include:

```markdown
## Summary
Phase C-2 of the oculus_sonar refactor (master spec §9 C-2). Move
`publishParameters()` out of the per-ping hot path and switch the
9 `/param/*` echo publishers to TRANSIENT_LOCAL + RELIABLE +
KEEP_LAST(1) so late-joining subscribers receive the most recent
value via DDS replay. Wire `OculusDriverConfig::on_change` to call
`republishParameters()` so parameter updates land in the latched
cache within ~1 ms.

## Changes
- `OculusDriverPublishers::initialize()` — latched QoS for 9 param
  publishers + seed publish + 1 Hz wall_timer.
- `OculusDriverPublishers::publishPing<>()` — removed publishParameters call.
- `OculusDriverPublishers::republishParameters()` — new public wrapper.
- `OculusDriver::init()` — on_change listener calls republishParameters.
- `CHANGELOG.md` — Phase C-2 section.

## Verification
- [x] colcon build PASS (clean rebuild)
- [x] 8/8 C-1 gtest fixtures PASS (no algorithm regression)
- [x] Structural snapshot diff: only 9 param topics' QoS lines differ
      (`BEST_EFFORT→RELIABLE`, `VOLATILE→TRANSIENT_LOCAL`); other 3
      dimensions byte-identical.
- [x] `ros2 topic hz /…/param/range` 30 s → average ~1.0 Hz.
- [x] `ros2 topic echo --once /…/param/range` returns immediately.
- [ ] Hardware perf measurement (m3000d 30 Hz, p50/p99 baseline vs
      C-2-head) — deferred per C-1 carry-forward (UDP only, bag-replay
      does not exercise publishPing).

## Downstream Compatibility
`3d_mapper_node.py` subscribes to `/param/range` with VOLATILE +
dynamic reliability. New pub (RELIABLE + TRANSIENT_LOCAL) is strictly
more permissive — non-breaking.

## Spec / Plan
- Spec: `docs/superpowers/specs/2026-05-04-oculus-sonar-phase-c-2-throttle-latched-qos-design.md`
- Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-c-2-throttle-latched-qos.md`

## Next Phase
- Re-analysis round (post-A→C cumulative review) — optional, user judgment.
- Hardware perf measurement when m3000d is available.
- ping1d/ping360 analogue refactors — deferred (user decision 2026-05-04).
```

- [ ] **Step 4: Confirm push gate**

**STOP**. Do not push without explicit user approval.

Print this exact prompt to the user:

```
Phase C-2 ready to push:
- branch: refactor/phase-c-2-throttle-latched-qos
- ahead of main: 6 commits
- verification: build PASS, 8/8 gtest PASS, structural diff matches expected QoS flip only, hz ~1.0, echo --once instant.
- push? (Y/N)
```

WAIT for explicit "Y" / "진행" / "push" / "yes". Anything else → no push.

- [ ] **Step 5: On user approval, push and open PR**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git push -u origin refactor/phase-c-2-throttle-latched-qos
```

Expected: `Branch 'refactor/phase-c-2-throttle-latched-qos' set up to track remote branch ...`.

```bash
gh pr create --title "refactor(oculus_sonar): Phase C-2 — parameter echo throttle + latched QoS" \
             --body-file /tmp/oculus_c2_pr_body.md \
             --base main
```

(Where `/tmp/oculus_c2_pr_body.md` contains the body text from Step 3 — write it with `cat <<'EOF' > /tmp/oculus_c2_pr_body.md` first.)

Expected: a PR URL is printed.

- [ ] **Step 6: Capture PR URL and report back**

Print the PR URL and a final 5-line summary to the user:

```
PR opened: <URL>
Verification: build PASS, 8/8 gtest PASS, structural diff = expected QoS flip only.
Topic hz: ~1.0 Hz over 30s. Late-join echo --once: instant.
Branch: refactor/phase-c-2-throttle-latched-qos (6 commits ahead of main).
Ready for review.
```

---

## Self-Review Notes (controller-only — do not include in tasks)

**Spec coverage check:**

| Spec §                     | Plan task                              |
|----------------------------|----------------------------------------|
| §1 Goal — throttle         | Task 3 (timer) + Task 4 (publishPing remove) |
| §1 Goal — latched QoS      | Task 3 Step 2 (QoS swap)              |
| §1 Goal — on_change immediate | Task 5                            |
| §3 Architecture            | All implementation tasks (2–5)        |
| §4.1 hpp changes           | Task 2 + Task 4 Step 1                 |
| §4.2 cpp `initialize()`    | Task 3                                 |
| §4.2 cpp `republishParameters` | Task 4 Step 2                       |
| §4.2 cpp `publishPing` remove | Task 4 Step 1                       |
| §4.3 component.cpp wiring  | Task 5                                 |
| §6 Layer 1 build           | Task 6                                 |
| §6 Layer 2 byte-exact      | Task 4 Step 4 + Task 6 Step 2 (existing 8 gtest) |
| §6 Layer 3 structural snapshot | Task 1 (baseline) + Task 7 (candidate + diff) |
| §6 Layer 4 perf            | covered by C-1 instrumentation; CHANGELOG notes deferred (Task 9) |
| §6 Auxiliary topic hz      | Task 8 Step 3                          |
| §6 Auxiliary echo --once   | Task 8 Step 4                          |
| §7 Regression script reuse | Task 1 + Task 7 (no script edit)       |
| §9 Branch / commit conv.   | Task 2–5 commit messages, Task 10 PR title |
| §9 Push gate               | Task 10 Step 4                         |
| §12 Acceptance criteria    | All verification tasks (1, 6, 7, 8) + CHANGELOG (9) |

**Type / signature consistency:**
- `republishParameters()` declared (Task 2) and defined (Task 4) — same signature, no params, void return.
- `param_publish_timer_` declared (Task 2) and assigned (Task 3) — same type `rclcpp::TimerBase::SharedPtr`.
- `latched_qos` is a local in `initialize()` only; not stored as a member.

**Placeholder scan:** no TBD/TODO/FIXME. All file paths absolute; all commands runnable.
