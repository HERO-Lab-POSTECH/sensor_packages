# Phase C-1 — `pingToSonarImage` Zero-Copy Refactor

**Date**: 2026-05-04
**Status**: Brainstorm complete, awaiting plan
**Master design**: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §9 C-1
**Predecessor**: Phase B-2 (PR #3, awaiting merge)

---

## 1. Goal

Replace the per-element `push_back` hot loop in `pingToSonarImage` with a single-allocation `memcpy` path, preserving byte-exact output. Effect on the highest-rate driver path (m3000d 30Hz, ~512×~1024×4 bytes ≈ 2 MB per ping):

- **Allocations**: O(N×M) `push_back` realloc churn → 1 `resize`
- **Loop body**: 3-way `if (dataSize == 1/2/4)` branch per inner iteration → branch hoisted out of loop
- **Copy**: N×M individual byte writes with bit shifts → 1 `memcpy` for fast path, row-by-row `memcpy` for stride path

## 2. Scope

### In scope
- Modify `liboculus/include/liboculus/ImageData.h` (vendored — full edit rights confirmed; no `.git` directory)
- Modify `oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp` hot loop only
- Add `oculus_sonar/test/test_ping_to_sonar_image.cpp` (new gtest — first unit test in this package)
- Add latency instrumentation to `OculusDriverPublishers::publishPing` (chrono-based, RCLCPP_INFO at intervals)
- Reuse B-1's `scripts/regression_oculus.sh` unchanged

### Out of scope
- Phase C-2 (parameter echo throttle + latched QoS) — separate PR
- Polar-to-cartesian (`polar_to_cartesian.cpp`) — out of scope per master spec §13
- Fan imager hot path — separate code path, not part of C-1
- liboculus broader API surface — only the 4 accessors needed

## 3. Architecture

### File structure (locked)

| File | Status | Responsibility |
|------|--------|---------------|
| `oculus_ros2/liboculus/include/liboculus/ImageData.h` | Modify | Add 4 public `const` accessors: `data()`, `stride()`, `offset()`, `numBytes()` |
| `oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp` | Modify | Replace hot loop (lines 126-143) with hoisted-branch + memcpy path |
| `oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp` | Create | gtest fixture: synthetic SimplePingResult bytes, byte-comparison vs golden output |
| `oculus_ros2/oculus_sonar/CMakeLists.txt` | Modify | Add `ament_add_gtest` block for the new test |
| `oculus_ros2/oculus_sonar/package.xml` | Modify | Add `<test_depend>ament_cmake_gtest</test_depend>` if missing |
| `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp` | Modify | Add chrono-based ping callback latency instrumentation |

### Component contracts

#### `liboculus::ImageData` accessors (new)

```cpp
const uint8_t* data() const noexcept { return _data; }
size_t stride() const noexcept { return _stride; }
size_t offset() const noexcept { return _offset; }
uint32_t numBytes() const noexcept { return _imageSize; }
```

All `const`, all `noexcept`, all return non-owning views. `data()` may return `nullptr` (caller must check before deref) — matches existing `at_uint8/16/32` nullptr behavior.

#### `pingToSonarImage` hot path (rewritten)

```cpp
const auto& img = ping.image();
const size_t element_size = ping.dataSize();
const size_t row_bytes = num_bearings * element_size;
const size_t total_bytes = num_ranges * row_bytes;

sonar_image.intensities.resize(total_bytes);

if (img.data() == nullptr || total_bytes == 0) {
    sonar_image.intensities.clear();
    return sonar_image;
}

const uint8_t* src = img.data() + img.offset();

if (img.stride() == row_bytes) {
    // Fast path: contiguous memory, single memcpy
    std::memcpy(sonar_image.intensities.data(), src, total_bytes);
} else {
    // Stride path: row-by-row memcpy (gain prefix or padding)
    uint8_t* dst = sonar_image.intensities.data();
    for (size_t r = 0; r < num_ranges; ++r) {
        std::memcpy(dst + r * row_bytes, src + r * img.stride(), row_bytes);
    }
}
```

Branch on `dataSize` is no longer needed inside the loop because little-endian byte storage is identical for 1/2/4-byte data when `is_bigendian = false` (as set on line 123 of current code).

#### Test fixture contract

```cpp
TEST(PingToSonarImage, ByteExact_8bit_FastPath)  { /* dataSize=1, stride=nBeams */ }
TEST(PingToSonarImage, ByteExact_16bit_FastPath) { /* dataSize=2, stride=2*nBeams */ }
TEST(PingToSonarImage, ByteExact_32bit_FastPath) { /* dataSize=4, stride=4*nBeams */ }
TEST(PingToSonarImage, ByteExact_8bit_StridePath)  { /* dataSize=1, stride=nBeams+4 (gain prefix) */ }
TEST(PingToSonarImage, ByteExact_16bit_StridePath) { /* dataSize=2, stride=2*nBeams+4 */ }
TEST(PingToSonarImage, ByteExact_32bit_StridePath) { /* dataSize=4, stride=4*nBeams+4 */ }
TEST(PingToSonarImage, ZeroSizeImage) { /* nRanges=0 → empty intensities */ }
TEST(PingToSonarImage, NullDataPointer) { /* _data=nullptr → empty intensities */ }
```

8 fixtures covering both code paths (fast × 3 element sizes, stride × 3 element sizes) + 2 edge cases. Each fixture:
1. Constructs synthetic `SimplePingResult<PingT>` with deterministic byte pattern (e.g., `byte[i] = i & 0xFF`)
2. Runs old `pingToSonarImage` (frozen copy in test file) and new `pingToSonarImage`
3. Asserts `EXPECT_EQ(old.intensities, new.intensities)` element-by-element

## 4. Data Flow

### Fast path (default — no gain prefix)

```
UDP raw bytes (ping packet)
  └─→ liboculus::SimplePingResult<PingT>::_image
        ├── _data: const uint8_t* (raw bytes from UDP)
        ├── _offset: 0
        ├── _stride: nBeams * dataSize
        └── _imageSize: total bytes
  └─→ pingToSonarImage(ping):
        intensities.resize(total_bytes)        # 1 allocation
        memcpy(intensities.data(), src, total) # 1 copy
  └─→ marine_acoustic_msgs::SonarImage
        └─→ publish to /sensor/sonar/oculus/{model}/sonar
```

### Stride path (gain prefix per row)

```
UDP raw bytes
  └─→ liboculus::SimplePingResult<PingT>::_image
        ├── _data: pointer to first row's gain prefix
        ├── _offset: 4 (skip gain bytes)
        ├── _stride: nBeams * dataSize + 4
        └── _imageSize: includes gain prefixes
  └─→ pingToSonarImage(ping):
        intensities.resize(num_ranges * num_bearings * dataSize)
        for r in 0..num_ranges:
            memcpy(dst + r*row_bytes, src + r*stride, row_bytes)  # nRanges copies
  └─→ identical SonarImage as before
```

## 5. Verification Strategy (4 layers)

| Layer | Tool | Pass criterion |
|-------|------|---------------|
| 1. Build | `colcon build --packages-select oculus_sonar` | `Finished <<< oculus_sonar` |
| 2. Unit (byte-exact) | `colcon test --packages-select oculus_sonar` | All 7 gtest fixtures PASS |
| 3. Structural snapshot | `bash regression_oculus.sh compare` (B-1's script reused) | 4 snapshot files byte-identical to baseline |
| 4. Performance (data only) | chrono instrumentation in driver + `pidstat -p <PID> 1 60` | Recorded; PR description tabulates baseline vs candidate |

Layer 4 is data-collection, NOT a fail gate (per master spec §9 risks: measurement noise can exceed perf gain).

### Performance metric definitions

| Metric | Method | Baseline | Candidate |
|--------|--------|----------|-----------|
| ping callback p50 ms | `chrono::steady_clock` before/after `publishPing()`, RCLCPP_INFO every 100 pings with sliding window p50 | TBD (measure) | TBD (measure) |
| ping callback p99 ms | Same instrument, p99 of last 100 pings | TBD | TBD |
| `oculus_driver` %CPU | `pidstat -p $(pgrep oculus_m3000d_container) 1 60` average | TBD | TBD |

m3000d at 30Hz is the highest-amplification target (largest data per ping × highest rate). m750d at 10Hz is secondary.

## 6. Pre-flight Verification (already done in brainstorming)

| Question | Answer | Source |
|----------|--------|--------|
| Is `liboculus` vendored or external? | **Vendored**. No `.git` directory. Full edit rights. | `ls liboculus/.git` returned not-found |
| Does `ImageData` expose raw byte pointer? | **No (private)**. Need to add 4 accessors. | `ImageData.h` lines 127-131 |
| Storage layout? | **Row-major** by `[rangeBin][beam]`. `index = rangeBin * stride + beam` for 1-byte. | `at_uint8` line 80 |
| Does the loop order match storage? | **Yes**. `for (r) for (b)` is row-major. | `ping_to_sonar_image.hpp:126-127` |
| Output endianness? | **Little-endian** (`is_bigendian = false`). Matches input direct copy. | `ping_to_sonar_image.hpp:123` |
| Is there a stride/offset case? | **Yes**. `SimplePingResult.h:122` constructs `ImageData` with `strideBytes` and `offsetBytes` (gain prefix per row). Default-constructed variant (line 131) has stride=0, offset=0. | grep on `SimplePingResult.h` |

## 7. Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Stride/offset path produces different byte output than current loop | Unit fixture `ByteExact_*_StridePath` enforces byte-for-byte match |
| Measurement noise > performance gain (no improvement detectable) | 60s sample + p50/p99 + 3 separate runs averaged. PR description reports raw + delta. Layer 4 is informational, not a gate. |
| `_data == nullptr` edge case | Test `NullDataPointer` fixture exercises early-return path |
| New gtest infra (none currently in oculus_sonar) | Add minimal `ament_add_gtest` block — same pattern as other ROS2 packages in workspace; reference: `lidar_slam/cartographer_ros/test/` |
| `chrono::steady_clock` instrumentation perturbs the very metric it measures | Use `RCLCPP_INFO_THROTTLE(1000ms, ...)` to log only every 1s — sub-microsecond chrono overhead is negligible vs ms-scale callback |
| Vendored liboculus modification breaks downstream consumers | Confirmed: only `oculus_sonar` package consumes `liboculus` headers in this workspace. `grep -rn "liboculus/" --include='*.{cpp,hpp,h}'` from workspace root returns only oculus_sonar matches. |

## 8. Open Questions Resolved in Brainstorming

| Q | Resolution |
|---|-----------|
| C-1 + C-2 in one PR or split? | **Split**. C-1 first, C-2 separate PR after C-1 merge. |
| liboculus `raw_ptr()` upstream PR or vendored edit? | **Vendored edit**. liboculus has no `.git` here — fully owned. |
| byte-exact verification method? | **Synthetic gtest fixtures**. Bag-replay impossible (UDP input). |
| Performance gate or data-collection? | **Data-collection**. Measurement noise risk; record but don't gate merge. |

## 9. Implementation Phases (handed to writing-plans)

The plan will decompose into:

1. **Pre-flight** (already done — this spec)
2. **Add ImageData accessors** (small, vendored edit + smoke build)
3. **Refactor pingToSonarImage** with TDD: write 8 gtest fixtures FIRST, watch them PASS against current code (golden), then rewrite hot path, watch them PASS against new code (byte-exact)
4. **Add latency instrumentation** to `publishPing` (chrono + RCLCPP_INFO_THROTTLE)
5. **Capture baseline measurements** (m3000d 30Hz bag, 60s sample, p50/p99/CPU)
6. **Capture candidate measurements** (same bag, same duration)
7. **Run structural snapshot regression** (Layer 3) — must PASS byte-identical
8. **CHANGELOG + PR**

The plan-writer will output exact files to touch, exact code blocks, exact commands per step.

## 10. Notes

- This is the FIRST unit test in `oculus_sonar`. The CMake/package.xml additions establish the gtest infrastructure for future C-2 and beyond.
- Phase C-2 (parameter echo throttle) gets a separate brainstorm round after C-1 merge. The C-1 instrumentation infrastructure (chrono in `publishPing`) will inform how C-2 measures publisher load.
- All performance measurements should use `ROS_DOMAIN_ID=99` per the regression DDS gotcha (memory `reference_oculus_regression_dds_gotcha.md`).
- This is the FIRST PR that modifies vendored `liboculus`. CMakeLists.txt of liboculus does not need touching — header-only addition.

## 11. Out-of-Scope (deferred)

Per master spec §13:
- GPU acceleration of polar-to-cartesian
- Lifecycle node migration
- Auto-reconnect on UDP disconnect
- Dual-sonar simultaneous operation
- Status metadata expansion (temperature, heading, voltage)
- Fan imager hot path (separate code path)
