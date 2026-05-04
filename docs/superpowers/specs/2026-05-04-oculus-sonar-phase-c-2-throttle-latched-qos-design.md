# Phase C-2 — Parameter Echo Throttle + Latched QoS (Design)

**Date**: 2026-05-04
**Phase**: C-2 (final mandatory phase of master refactor §9)
**Baseline**: `main @ 2032ef1` (post-C-1 merge)
**Branch**: `refactor/phase-c-2-throttle-latched-qos`
**Master spec**: [`2026-05-03-oculus-sonar-refactor-design.md`](2026-05-03-oculus-sonar-refactor-design.md) §9 C-2

## 1. Goal & Non-goals

### Goal

Reduce wasted publish work on the 9 `param/*` echo topics and improve late-joining-subscriber UX:

- **Throttle**: move `publishParameters()` out of the per-ping hot path into a `wall_timer(1s)` (steady-state ~1 Hz instead of `ping_hz`).
- **Latched QoS**: switch the 9 `std_msgs::msg::*` publishers from `SensorDataQoS` (`BEST_EFFORT + VOLATILE + KEEP_LAST(5)`) to `KEEP_LAST(1) + RELIABLE + TRANSIENT_LOCAL` so a late-connecting subscriber receives the most recent value immediately on `connect()`.
- **On-change immediate publish**: when `OculusDriverConfig` notifies its listeners after a parameter update, publish all 9 echoes once so the latched cache reflects the new state without waiting up to 1 s for the next timer tick.

### Non-goals

- No change to the 4 sensor-data publishers (`/sonar`, `/raw_data`, `/metadata`, `/image`) — they keep `SensorDataQoS`.
- No change to message payloads. Only publish frequency and QoS profile change.
- No new gtest fixtures. Structural snapshot regression catches all observable changes.
- No hardware perf measurement gate. Measurement remains deferred to whenever a real m3000d is available (carry-forward from C-1).
- No analogous refactor in `ping1d_sonar`, `ping360_sonar`, or `sonoptix_ros2`. User explicitly deferred those on 2026-05-04.

## 2. Current State

### `OculusDriverPublishers::initialize()` — 9 publishers with sensor-grade QoS

```cpp
auto sensor_qos = rclcpp::SensorDataQoS();
// ...
ping_rate_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
    topic_prefix_ + "/param/ping_rate", sensor_qos);
freq_mode_pub_ = node_->create_publisher<std_msgs::msg::Int32>(
    topic_prefix_ + "/param/freq_mode", sensor_qos);
data_size_pub_ = node_->create_publisher<std_msgs::msg::String>(
    topic_prefix_ + "/param/data_size", sensor_qos);
range_pub_     = node_->create_publisher<std_msgs::msg::Float32>(
    topic_prefix_ + "/param/range", sensor_qos);
gain_pub_      = node_->create_publisher<std_msgs::msg::Int32>(
    topic_prefix_ + "/param/gain", sensor_qos);
gamma_pub_     = node_->create_publisher<std_msgs::msg::Int32>(
    topic_prefix_ + "/param/gamma", sensor_qos);
ip_address_pub_  = node_->create_publisher<std_msgs::msg::String>(
    topic_prefix_ + "/param/ip_address", sensor_qos);
frame_id_pub_    = node_->create_publisher<std_msgs::msg::String>(
    topic_prefix_ + "/param/frame_id", sensor_qos);
sonar_model_pub_ = node_->create_publisher<std_msgs::msg::String>(
    topic_prefix_ + "/param/sonar_model", sensor_qos);
```

### `publishPing<>()` — calls `publishParameters()` every ping callback

```cpp
template <typename PingT>
void publishPing(const PingT& ping) {
  // ... convert + publish sonar/image/metadata
  publishParameters();  // ← 9 publishes per ping callback
  // C-1 latency instrumentation
}
```

### Quantitative wasted work

- m3000d at 30 Hz: 9 publishers × 30 Hz = **270 msg/s**
- m1200d at 15 Hz: 9 × 15 = 135 msg/s
- m750d at 14 Hz: 9 × 14 = 126 msg/s

After C-2 (steady state), all of these become **9 msg/s** = 1 publishParameters() invocation per second.

### Late-join UX

`ros2 topic echo --once /sensor/sonar/oculus/m750d/param/range` waits up to one ping interval (~70 ms at 14 Hz) for the next publish. With TRANSIENT_LOCAL the answer is immediate (cache replay).

## 3. Target State

### Architecture diagram

```
OculusDriver (component)
├── OculusDriverConfig
│     └── on_change(callback)
│           ├── connection_->sendSimpleFireMessage(new_config)   [existing]
│           └── publishers_->republishParameters()               [NEW]
├── OculusDriverPublishers
│     ├── 4 sensor publishers          [SensorDataQoS, unchanged]
│     ├── 9 param publishers           [KEEP_LAST(1) + RELIABLE + TRANSIENT_LOCAL]   [QoS CHANGED]
│     ├── param_publish_timer_         [wall_timer(1s) → publishParameters()]        [NEW]
│     └── publishPing<>()              [publishParameters() call REMOVED]
└── ConnectionManager                  [unchanged]
```

### Data flow & threading

```
[Param service callback path]                  [I/O thread (boost::asio)]
        │                                              │
        ▼                                              ▼
   parameterCallback()                          DataRx packet ready
        │                                              │
        ▼                                              ▼
   listeners_ run (in registration order)       executor->dispatch
        │                                              │
        ▼                                              ▼
   1) sendSimpleFireMessage                     publishPing<>()
   2) publishers_->republishParameters()         (no longer calls
        │                                          publishParameters)
        ▼
   9 publish → TRANSIENT_LOCAL durability cache

[Single-threaded executor]
   ├── wall_timer(1s) tick → publishParameters()
   └── parameter service callback → on_change → publishParameters()
```

The wall_timer and parameter-service callback both run on the executor thread. Single-threaded executor (rclcpp_components default) implies no concurrency between them. The I/O thread (DataRx) no longer touches `publishParameters()`, so the previous I/O-vs-executor race that existed implicitly through `publishPing → publishParameters` is also eliminated.

## 4. Component Changes

### 4.1 `oculus_driver_publishers.hpp`

```cpp
class OculusDriverPublishers {
 public:
  // ... existing public methods ...

  // Publishes all 9 parameter echoes once. Called by:
  //   (1) wall_timer (steady-state ~1 Hz),
  //   (2) on_change listener after a parameter update (immediate),
  //   (3) initialize() (latched seed for late-join subscribers).
  void republishParameters();

 private:
  void publishParameters();  // existing — same body, now reused

  // ... existing publishers ...

  rclcpp::TimerBase::SharedPtr param_publish_timer_;  // NEW
};
```

`republishParameters()` is a thin public wrapper around the existing private `publishParameters()`. Public visibility is required so `OculusDriver::on_change` can call it without a `friend` declaration.

### 4.2 `oculus_driver_publishers.cpp` — `initialize()` end + `publishPing` body

```cpp
void OculusDriverPublishers::initialize() {
  auto sensor_qos = rclcpp::SensorDataQoS();
  auto latched_qos = rclcpp::QoS(1).transient_local();   // NEW

  // 4 sensor publishers — unchanged (sensor_qos)
  // ...

  // 9 param publishers — latched_qos
  ping_rate_pub_   = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/ping_rate", latched_qos);
  freq_mode_pub_   = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/freq_mode", latched_qos);
  data_size_pub_   = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/data_size", latched_qos);
  range_pub_       = node_->create_publisher<std_msgs::msg::Float32>(
      topic_prefix_ + "/param/range", latched_qos);
  gain_pub_        = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gain", latched_qos);
  gamma_pub_       = node_->create_publisher<std_msgs::msg::Int32>(
      topic_prefix_ + "/param/gamma", latched_qos);
  ip_address_pub_  = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/ip_address", latched_qos);
  frame_id_pub_    = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/frame_id", latched_qos);
  sonar_model_pub_ = node_->create_publisher<std_msgs::msg::String>(
      topic_prefix_ + "/param/sonar_model", latched_qos);

  RCLCPP_INFO(node_->get_logger(), "Publishing data with frame = %s", frame_id_.c_str());

  // Latched seed: subscriber connecting before the first timer tick still
  // gets a value via TRANSIENT_LOCAL replay.
  publishParameters();

  param_publish_timer_ = node_->create_wall_timer(
      std::chrono::seconds(1),
      [this]() { publishParameters(); });
}

// Wrapper for external callers (config on_change listener).
void OculusDriverPublishers::republishParameters() {
  publishParameters();
}

template <typename PingT>
void OculusDriverPublishers::publishPing(const PingT& ping) {
  // ... existing convert + sensor publish ...
  // REMOVED: publishParameters();
  // C-1 latency instrumentation remains as-is.
}
```

### 4.3 `oculus_driver_component.cpp` — wire on_change to the publisher

```cpp
config_->on_change([this](const liboculus::SonarConfiguration& new_config) {
  if (connection_->isConnected()) {
    connection_->sendSimpleFireMessage(new_config);
  }
  publishers_->republishParameters();  // NEW
});
```

### 4.4 `oculus_driver_config.{hpp,cpp}` — no change

Existing `on_change(callback)` plumbing already supports multiple listeners and is reused as-is.

### 4.5 LOC delta

| File | Change |
|------|--------|
| `oculus_driver_publishers.hpp` | +3 (timer member + republishParameters declaration + comment) |
| `oculus_driver_publishers.cpp` | +6, −1 (latched_qos object + 9 QoS substitutions [in-place], initial publish, timer creation, wrapper; removed `publishParameters()` call inside `publishPing`) |
| `oculus_driver_component.cpp` | +1 (republishParameters call) |

Net ~+10 LOC. `initialize()` grows from 30 to 36 lines (PKRC ≤50 LOC limit respected with 14 lines headroom).

## 5. Downstream Compatibility Audit

QoS compatibility rules in ROS 2 (DDS):

- **Reliability**: pub `RELIABLE` + sub `RELIABLE/BEST_EFFORT` = compatible; pub `BEST_EFFORT` + sub `RELIABLE` = INCOMPATIBLE.
- **Durability**: pub `TRANSIENT_LOCAL` + sub `VOLATILE/TRANSIENT_LOCAL` = compatible; pub `VOLATILE` + sub `TRANSIENT_LOCAL` = INCOMPATIBLE.

### Subscriber survey

```bash
grep -rn "param/ping_rate\|param/freq_mode\|param/data_size\|param/range\|param/gain\|param/gamma" \
  /workspace/ros2_ws/src --include="*.cpp" --include="*.py" --include="*.hpp" --include="*.h" \
  | grep -v oculus_sonar
```

Findings (run 2026-05-04):

| File | Topic | Subscriber QoS | Compatible w/ new pub? |
|------|-------|----------------|------------------------|
| `sonar_3d_reconstruction/scripts/3d_mapper_node.py` | `/sensor/sonar/.../param/range` | `KEEP_LAST(10) + reliability dynamic + durability VOLATILE (default)` | **Yes** (pub RELIABLE accepts BEST_EFFORT/RELIABLE sub; pub TRANSIENT_LOCAL is strictly more permissive than sub VOLATILE) |

The `sonoptix_ros2/echo.py` and `ping360_ros2` references in the grep output are **publishers** to their own param topics, not subscribers to oculus param topics, so they are not relevant for this audit.

No subscriber currently demands `TRANSIENT_LOCAL` (which would also be compatible with the new pub). No subscriber currently uses `BEST_EFFORT-only` reliability while pub is also BEST_EFFORT — promoting pub to RELIABLE is non-breaking.

### Conclusion

Approach (A) (latched QoS + throttle) is selected without falling back to (B) (throttle only). No downstream change needed.

## 6. Verification (4-layer)

| Layer | Required | Method |
|-------|----------|--------|
| **1. Build** | Yes | `colcon build --packages-select oculus_sonar` PASS (Release + BUILD_TESTING=ON, default) |
| **2. Byte-exact** | n/a | C-2 is a structural change (timer + QoS), not an algorithm change. Existing 8 C-1 fixtures still pass (no algorithm regression), but no new fixtures added. |
| **3. Structural snapshot** | Yes | `scripts/regression_oculus.sh` baseline=`2032ef1` vs candidate=branch HEAD. Expected diff: 9 param topics' `Reliability QoS` flips `BEST_EFFORT → RELIABLE` and `Durability QoS` flips `VOLATILE → TRANSIENT_LOCAL`. All other dimensions byte-identical. |
| **4. Performance** | Measurement only | No gate. C-1 instrumentation `[c1-perf] pings=N window=W p50=Xms p99=Yms` automatically captures publishPing latency reduction once `publishParameters()` is removed from the hot path. Hardware-only — bag replay does not exercise `publishPing`. |

### Auxiliary check (plan execution step, not a hard gate)

Steady-state throttle verification:

```bash
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d &
sleep 5
timeout 30 ros2 topic hz /sensor/sonar/oculus/m750d/param/range
```

Expected: `average rate: ~1.0`, std dev < 0.05 Hz.

Late-join verification:

```bash
ros2 topic echo --once /sensor/sonar/oculus/m750d/param/range
```

Expected: instant return with the latched value (no waiting for next publish).

## 7. Regression Infrastructure Reuse

`scripts/regression_oculus.sh` (B-1 origin, B-2/C-1 reuse) is consumed unmodified for C-2:

- Same 4-dimensional snapshot (`component_types`, `param_dump`, `topic_list`, `topic_info -v`).
- The QoS flip lives entirely inside `topic_info -v` output for the 9 param topics; the diff makes those changes explicit and is the structural verification.
- DDS gotchas continue to apply (`pkill -f component_container`, `ros2 daemon stop`, `STARTUP_WAIT_S=12-15`, `ROS_DOMAIN_ID=99` exported before sourcing ROS).

No new regression script. The optional `ros2 topic hz` and `ros2 topic echo --once` commands run as one-shot plan steps.

## 8. Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|:--:|:--:|------|
| Hidden subscriber requires RELIABLE while we stayed on BEST_EFFORT | Low | None (we promote to RELIABLE) | n/a |
| Hidden subscriber requires `BEST_EFFORT` only and rejects RELIABLE | None (pub-RELIABLE + sub-BEST_EFFORT is compatible per DDS) | n/a | n/a |
| `wall_timer` + `on_change` publish in the same ms | Common | Idempotent — TRANSIENT_LOCAL `KEEP_LAST(1)` keeps the most recent value | none needed |
| Future `MultiThreadedExecutor` adoption breaks the single-threaded assumption | Possible later | `param_publish_timer_` and `on_change` listener could race | Carry-forward note: add `std::mutex` around `publishParameters()` if `MultiThreadedExecutor` is introduced. Same caveat already documented for C-1 instrumentation. |
| TRANSIENT_LOCAL durability cache memory | Low | Negligible — `KEEP_LAST(1)` × 9 publishers × small `std_msgs` ≈ tens of bytes | none needed |
| Param service rapid-fire updates trigger many `publishParameters()` calls in quick succession | Low | DDS write storm for ~ms | TRANSIENT_LOCAL `KEEP_LAST(1)` deduplicates on the wire and at consumers; no extra throttle (YAGNI) |
| `[c1-perf]` log shows no signal in bag-replay regression env | Already observed in C-1 | No measurement | Carry-forward — measurement happens when hardware is available |

## 9. Branch & Commit Convention

- **Branch**: `refactor/phase-c-2-throttle-latched-qos` (created from `main @ 2032ef1`)
- **Commit history**: small, frequent commits during plan execution; squash-merged via PR (linear history enforced)
- **Squash subject**: `refactor(oculus_sonar): Phase C-2 — parameter echo throttle + latched QoS`
- **CHANGELOG.md** entry appended at the top of `oculus_ros2/oculus_sonar/CHANGELOG.md`, before the existing C-1 section, with the format:
  - `### Changed` — list the QoS flip and the publishPing/initialize edits
  - `### Added` — list the wall_timer and the `republishParameters()` wrapper
  - `### Verification` — record the build, structural snapshot diff summary, and the auxiliary `topic hz` reading
- **Push gate**: explicit user approval at the verification step before `git push origin <branch>` (PKRC convention; same as C-1)
- **No AI attribution** anywhere in commits, PR description, or files

## 10. Out of Scope

- Hardware perf measurement on real m3000d (deferred since C-1, UDP input only).
- Renaming `[c1-perf]` to `[perf]` or splitting into per-stage tags (defer to re-analysis round if needed).
- Adding gtest fixtures for timer / latched behavior (rclcpp test harness overhead exceeds value for a deterministic primitive; structural snapshot already catches QoS regressions).
- `ping1d_sonar`, `ping360_sonar`, `sonoptix_ros2` analogous refactors (user deferred 2026-05-04).
- `MultiThreadedExecutor` adoption.

## 11. Carry-forward to Future Phases

| Item | Origin | Where to land |
|------|--------|---------------|
| Hardware perf measurement (m3000d 30 Hz, p50/p99 baseline vs C-2-head) | C-1 + C-2 | When real sonar available |
| `publishParameters` mutex if MultiThreadedExecutor adopted | C-2 | At the time of the executor change |
| Possible split of `pingToSonarImage` into `fillBearings/fillRanges/copyIntensities` (LOC 133, PKRC ≤50 violation, pre-existing state) | C-1 | Re-analysis round |
| `latency_window_` mutex | C-1 | Same trigger as above |
| Sort-every-ping in `recordLatencyAndLog` moved inside the throttle | C-1 | Re-analysis round |

## 12. Acceptance Criteria

The C-2 PR is ready to merge when:

1. `colcon build --packages-select oculus_sonar` PASS.
2. Existing 8 C-1 gtest fixtures PASS (no algorithm regression).
3. `scripts/regression_oculus.sh` produces a diff that contains **only** the expected 9-topic QoS flip (`BEST_EFFORT → RELIABLE`, `VOLATILE → TRANSIENT_LOCAL`); all other snapshot dimensions byte-identical.
4. `ros2 topic hz /sensor/sonar/oculus/m750d/param/range` reports a steady ≈1 Hz over 30 seconds.
5. `ros2 topic echo --once /sensor/sonar/oculus/m750d/param/range` returns immediately on a freshly launched node (latched cache populated by `initialize()` seed publish).
6. CHANGELOG.md updated with a Phase C-2 section.
7. PR description summarizes 4-layer verification status (Layer 4 marked deferred per C-1 carry-forward).
