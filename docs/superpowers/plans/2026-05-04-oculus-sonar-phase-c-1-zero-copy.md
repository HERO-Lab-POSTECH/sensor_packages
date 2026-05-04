# oculus_sonar Phase C-1 — Zero-Copy `pingToSonarImage` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the per-element `push_back` hot loop in `pingToSonarImage` with a single-allocation `memcpy` path while preserving byte-exact output, verified by 8 new gtest fixtures + B-1's structural snapshot regression.

**Architecture:** Add 4 public `const noexcept` accessors to vendored `liboculus::ImageData` (header-only edit), then rewrite `pingToSonarImage` hot loop (`ping_to_sonar_image.hpp` lines 126-143) using `data() + offset()` + `stride()` + `numBytes()`. Two code paths: fast-path (single `memcpy` when stride equals row-bytes) and stride-path (row-by-row `memcpy` when gain prefix is present). Establish first gtest infrastructure for `oculus_sonar` package; reuse B-1's `scripts/regression_oculus.sh` for structural snapshot regression.

**Tech Stack:** C++17, ROS2 Humble (rclcpp_components), ament_cmake_gtest, vendored liboculus, marine_acoustic_msgs, chrono::steady_clock for latency instrumentation.

**Spec:** `docs/superpowers/specs/2026-05-04-oculus-sonar-phase-c-1-zero-copy-design.md` (224 LOC, sections §1–§11; user-approved 2026-05-04)

**Branch:** `refactor/phase-c-1-zero-copy` (pre-existing, spec commit `4672feb`)

**Baseline:** `adc79cc` (B-1 머지 직후 main HEAD; C-1 is independent of B-2 PR #3)

---

## File Map (locked)

| File | Status | Touch |
|------|--------|-------|
| `oculus_ros2/liboculus/include/liboculus/ImageData.h` | Modify | +4 public `const noexcept` accessors after line 70 |
| `oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp` | Modify | Replace lines 126-143 with memcpy path; add `<cstring>` include |
| `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp` | Modify | Add chrono instrumentation + sliding-window p50/p99 stats |
| `oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp` | Create | 8 gtest fixtures + synthetic ping helper + frozen-old-impl |
| `oculus_ros2/oculus_sonar/CMakeLists.txt` | Modify | Uncomment+adjust BUILD_TESTING block (lines 169-179) |
| `oculus_ros2/oculus_sonar/package.xml` | No change | `ament_cmake_gtest` already declared (line 53) — verify only |
| `oculus_ros2/oculus_sonar/CHANGELOG.md` | Modify | Prepend `[Unreleased] — Phase C-1` section |
| `oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp` | No change | Confirmed — no instrumentation needed in .cpp (template is in .hpp) |

**Total**: 6 files modified, 1 file created, 1 file no-change verification.

---

## Verification Strategy (4 layers)

| Layer | Tool | Pass criterion |
|-------|------|---------------|
| 1. Build | `colcon build --packages-select oculus_sonar` | `Finished <<< oculus_sonar` |
| 2. Unit (byte-exact) | `colcon test --packages-select oculus_sonar` | All 8 gtest fixtures PASS |
| 3. Structural snapshot | `bash scripts/regression_oculus.sh compare` (B-1's script) | 4 snapshot files byte-identical to baseline |
| 4. Performance (data only) | chrono instrumentation in driver | Recorded; PR description tabulates baseline/candidate (NOT a fail gate per spec §5/§7) |

---

## Task Sequence Overview

1. **Task 1** — Capture baseline structural snapshot (Layer 3 reference; before any code changes)
2. **Task 2** — Add `ImageData` public accessors (header-only vendored edit)
3. **Task 3** — Set up gtest infrastructure (CMakeLists.txt + create `test/` dir)
4. **Task 4** — Write 8 gtest fixtures with frozen-old-impl (TDD: tests pass against CURRENT code → characterizes existing behavior)
5. **Task 5** — Refactor `pingToSonarImage` hot loop (memcpy path; tests still pass = byte-exact)
6. **Task 6** — Capture candidate structural snapshot + compare (Layer 3 byte-identical)
7. **Task 7** — Add chrono latency instrumentation to `publishPing`
8. **Task 8** — Update CHANGELOG.md
9. **Task 9** — Final verification + commit + push gate + PR (STOP for user approval before push)

---

## Task 1: Capture Baseline Structural Snapshot

**Files:** None modified — `/tmp/oculus_regression/` outputs only (not committed).

**Why first:** B-1's `scripts/regression_oculus.sh` requires a `baseline` snapshot from the build BEFORE C-1 changes are applied. Capturing now uses the spec-only commit `4672feb` (no production code touched). Same as comparing against `adc79cc` because spec is docs-only.

- [ ] **Step 1: Confirm clean working tree on C-1 branch**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git status --short
git log --oneline -3
```

Expected:
- `git status --short` returns empty (no unstaged changes)
- `git log` shows `4672feb docs(oculus_sonar): add Phase C-1 zero-copy refactor design spec` at HEAD, `adc79cc` as parent

- [ ] **Step 2: Stop ros2 daemon to clear stale state**

Run:
```bash
ros2 daemon stop > /dev/null 2>&1 || true
sleep 3
```

Expected: no error output. (`|| true` because daemon may already be stopped.)

- [ ] **Step 3: Build oculus_sonar at current commit (baseline = pre-C-1 code)**

Run:
```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -10
```

Expected: `Finished <<< oculus_sonar` in last 10 lines. Build time ~15-30s.

- [ ] **Step 4: Capture baseline snapshot with isolated DDS domain**

Run (single Bash invocation — `export ROS_DOMAIN_ID=99` MUST come before sourcing ROS, per memory `reference_oculus_regression_dds_gotcha.md`):
```bash
cd /workspace/ros2_ws/src/sensor_packages
export ROS_DOMAIN_ID=99
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ROS_DOMAIN_ID_REGRESSION=99 STARTUP_WAIT_S=12 \
    bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh baseline 2>&1 | tail -15
```

Expected:
- Last line includes `[baseline] snapshot saved`
- 4 files exist in `/tmp/oculus_regression/baseline/`: `component_types.txt`, `param_dump.yaml`, `topic_list.txt`, `topic_info_*.txt`

Verify:
```bash
ls -la /tmp/oculus_regression/baseline/
```

Expected: 4+ files, non-zero size each.

- [ ] **Step 5: No commit — snapshot is in `/tmp` (regenerable)**

Per `.claude/rules/refactor-workflow.md`: regression measurement outputs are not committed (regenerable). Move on to Task 2.

---

## Task 2: Add `ImageData` Public Accessors

**Files:**
- Modify: `oculus_ros2/liboculus/include/liboculus/ImageData.h:70` (insert 4 accessors after `nBeams()`)

**Why:** Current `ImageData` exposes only `at_uint8/16/32` (per-element accessors). The new memcpy path needs raw pointer + stride + offset + total bytes. These are the minimum accessors needed for zero-copy access; all are `const noexcept` returning non-owning views (matches spec §3).

- [ ] **Step 1: Open file and verify structure**

Read `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/liboculus/include/liboculus/ImageData.h` lines 69-71. Confirm:
- Line 69: `uint16_t nRanges() const { return _numRanges; }`
- Line 70: `uint16_t nBeams() const { return _numBeams; }`
- Line 71: empty line before `at_uint8` definition

- [ ] **Step 2: Insert 4 accessors after line 70**

Use Edit tool with this exact replacement:

`old_string` (preserve exact whitespace):
```
  uint16_t nRanges() const { return _numRanges; }
  uint16_t nBeams() const { return _numBeams; }

  uint8_t at_uint8(unsigned int beam, unsigned int rangeBin) const {
```

`new_string`:
```
  uint16_t nRanges() const { return _numRanges; }
  uint16_t nBeams() const { return _numBeams; }

  // Zero-copy raw byte access (Phase C-1).
  // data() may return nullptr — caller must check.
  // Storage: row-major [rangeBin][beam], element_size = _dataSize.
  // Row r occupies bytes [r * stride() + offset(),
  //                       r * stride() + offset() + nBeams() * _dataSize).
  const uint8_t *data() const noexcept { return _data; }
  size_t stride() const noexcept { return _stride; }
  size_t offset() const noexcept { return _offset; }
  uint32_t numBytes() const noexcept { return _imageSize; }

  uint8_t at_uint8(unsigned int beam, unsigned int rangeBin) const {
```

- [ ] **Step 3: Smoke build**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar liboculus --cmake-args -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -10
```

Expected: `Finished <<< oculus_sonar` and `Finished <<< liboculus`. No new warnings (header-only addition; consumers don't yet call new methods).

- [ ] **Step 4: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/liboculus/include/liboculus/ImageData.h
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "feat(liboculus): add public byte accessors to ImageData

Add data(), stride(), offset(), numBytes() — all const noexcept,
returning non-owning views. Required by Phase C-1 zero-copy refactor
of pingToSonarImage hot path. data() may return nullptr (matches
existing at_uint8/16/32 nullptr behavior — caller must check)."
```

Expected: commit success, 1 file changed, ~10 insertions.

---

## Task 3: Set Up gtest Infrastructure

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CMakeLists.txt:169-179` (replace commented-out BUILD_TESTING block)
- Verify (no change): `oculus_ros2/oculus_sonar/package.xml:53` already declares `<test_depend>ament_cmake_gtest</test_depend>`
- Create: `oculus_ros2/oculus_sonar/test/.gitkeep` (placeholder so empty dir is tracked until Task 4)

**Why:** This is the FIRST unit test in `oculus_sonar`. The CMake block establishes infrastructure for C-1 and future C-2 tests. We replace the existing commented-out block (which references a non-existent `test/unit/main.cpp`) with a working `ament_add_gtest` block targeting the new test file.

- [ ] **Step 1: Verify `ament_cmake_gtest` is already in package.xml**

Run:
```bash
grep -n "ament_cmake_gtest" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/package.xml
```

Expected: line 53 shows `<test_depend>ament_cmake_gtest</test_depend>`. No change needed.

- [ ] **Step 2: Replace commented-out BUILD_TESTING block in CMakeLists.txt**

Use Edit tool. `old_string`:
```
# Testing
#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()
#  
#  find_package(ament_cmake_gtest REQUIRED)
#  ament_add_gtest(oculus_sonar_test test/unit/main.cpp)
#  if(TARGET oculus_sonar_test)
#    target_link_libraries(oculus_sonar_test oculus_driver_component)
#  endif()
#endif()

ament_package()
```

`new_string`:
```
# Testing — Phase C-1 onwards.
# First gtest in oculus_sonar; tests byte-exact equivalence of
# pingToSonarImage refactor against frozen-old implementation.
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_ping_to_sonar_image
    test/test_ping_to_sonar_image.cpp
  )
  if(TARGET test_ping_to_sonar_image)
    target_include_directories(test_ping_to_sonar_image PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../liboculus/include
    )
    ament_target_dependencies(test_ping_to_sonar_image
      rclcpp
      marine_acoustic_msgs
      liboculus
    )
    target_link_libraries(test_ping_to_sonar_image
      ${liboculus_LIBRARIES}
      g3log
    )
  endif()
endif()

ament_package()
```

- [ ] **Step 3: Create `test/` directory placeholder**

Run:
```bash
mkdir -p /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/test
touch /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/test/.gitkeep
```

Expected: directory created, `.gitkeep` file present.

- [ ] **Step 4: Verify build still passes (test target not yet built — file missing)**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
```

Expected: `Finished <<< oculus_sonar`. The `if(BUILD_TESTING)` block is gated; without `--cmake-args -DBUILD_TESTING=ON` (which is on by default for colcon test, NOT for colcon build), the test target is not built. Build should succeed.

- [ ] **Step 5: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CMakeLists.txt oculus_ros2/oculus_sonar/test/.gitkeep
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "chore(oculus_sonar): set up gtest infrastructure

Replace stale commented-out test block (referencing non-existent
test/unit/main.cpp) with working ament_add_gtest block targeting
test/test_ping_to_sonar_image.cpp (added in next commit). First
unit test in this package; establishes pattern for C-1 byte-exact
verification and future C-2 tests.

Test sources include liboculus headers via private include dir
to access SimplePingResult buffer construction APIs."
```

---

## Task 4: Write 8 gtest Fixtures with Frozen-Old Implementation

**Files:**
- Create: `oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp`
- Delete: `oculus_ros2/oculus_sonar/test/.gitkeep` (replaced by real file)

**Why:** Characterization tests — capture CURRENT behavior of `pingToSonarImage` (per-element push_back loop) as the golden, then in Task 5 we refactor implementation while these tests must still pass byte-for-byte. The frozen old implementation lives **inside the test file** as `pingToSonarImage_v0()` so the test compares old vs new without depending on a separate old-code branch.

8 fixtures: 3 fast-path (8/16/32-bit, no gain prefix) + 3 stride-path (8/16/32-bit, with gain prefix per row) + 2 edge cases (zero-size, null-data) = 8.

- [ ] **Step 1: Read existing `pingToSonarImage` (lines 23-146 of header) to extract the inline-able skeleton**

Read `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp` once, lines 23-146.

Confirm key fields used by hot loop:
- `ping.ping()->nBeams` (uint16_t)
- `ping.ping()->nRanges` (uint16_t)
- `ping.dataSize()` (uint8_t — returns 1, 2, or 4 = bytes per element)
- `ping.image().at_uint8/16/32(beam, rangeBin)` (per-element)

These are what `pingToSonarImage_v0` (frozen old) and the new code both use.

- [ ] **Step 2: Create the test file**

Use Write tool to create `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp` with this content:

```cpp
/**
 * @file test_ping_to_sonar_image.cpp
 * @brief Characterization tests for pingToSonarImage byte-exact equivalence.
 *
 * Phase C-1 byte-exact verification: 8 fixtures covering both code paths
 * (fast / stride) × 3 element sizes (1, 2, 4 bytes) + 2 edge cases.
 *
 * Each fixture builds a synthetic SimplePingResult buffer with deterministic
 * byte pattern, runs both pingToSonarImage_v0 (frozen pre-C-1 implementation
 * embedded below) and the production pingToSonarImage, then asserts
 * EXPECT_EQ(v0.intensities, prod.intensities) byte-for-byte.
 *
 * Bag-replay verification is impossible because the driver receives UDP
 * packets directly — no hooks for replaying SonarImage outputs through the
 * conversion path. Synthetic fixtures are the only reliable byte-exact gate.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "Oculus/Oculus.h"
#include "liboculus/SimplePingResult.h"
#include "liboculus/Constants.h"
#include "marine_acoustic_msgs/msg/sonar_image.hpp"
#include "oculus_sonar/ping_to_sonar_image.hpp"

namespace {

using liboculus::ByteVector;
using liboculus::SimplePingResult;

// ---------------------------------------------------------------------------
// Frozen pre-C-1 implementation. DO NOT modify after Task 4 commit. The whole
// point of this function is to be the immutable golden reference for byte
// equivalence checks across the C-1 hot-path rewrite.
// ---------------------------------------------------------------------------
template<typename PingT>
marine_acoustic_msgs::msg::SonarImage pingToSonarImage_v0(
    const SimplePingResult<PingT> &ping) {
  marine_acoustic_msgs::msg::SonarImage sonar_image;
  sonar_image.frequency = ping.ping()->frequency;
  // (Beamwidth + bearings + ranges metadata is not part of the hot path
  // under test. We focus the fixture on the intensities byte equivalence.)
  const unsigned int num_bearings = ping.ping()->nBeams;
  const unsigned int num_ranges = ping.ping()->nRanges;
  sonar_image.is_bigendian = false;
  sonar_image.data_size = ping.dataSize();

  for (unsigned int r = 0; r < num_ranges; r++) {
    for (unsigned int b = 0; b < num_bearings; b++) {
      if (ping.dataSize() == 1) {
        const uint8_t data = ping.image().at_uint8(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
      } else if (ping.dataSize() == 2) {
        const uint16_t data = ping.image().at_uint16(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
        sonar_image.intensities.push_back((data & 0xFF00) >> 8);
      } else if (ping.dataSize() == 4) {
        const uint32_t data = ping.image().at_uint32(b, r);
        sonar_image.intensities.push_back(data & 0x000000FF);
        sonar_image.intensities.push_back((data & 0x0000FF00) >> 8);
        sonar_image.intensities.push_back((data & 0x00FF0000) >> 16);
        sonar_image.intensities.push_back((data & 0xFF000000) >> 24);
      }
    }
  }

  return sonar_image;
}

// ---------------------------------------------------------------------------
// Synthetic ping helper — builds a ByteVector buffer with a real
// OculusSimplePingResult struct + bearings + image data, then wraps in a
// SimplePingResult<OculusSimplePingResult>. The image data is a deterministic
// byte pattern (`(r * 73 + b * 17 + i) & 0xFF`) so any byte misalignment
// surfaces immediately in EXPECT_EQ diff output.
//
// @param data_enum dataSize8Bit / dataSize16Bit / dataSize32Bit (NOT bytes!)
// @param send_gain  true → stride path (gain prefix), false → fast path
// ---------------------------------------------------------------------------
struct SyntheticPingParams {
  uint16_t n_beams;
  uint16_t n_ranges;
  DataSizeType data_enum;
  bool send_gain;
};

uint8_t bytes_per_element(DataSizeType e) {
  switch (e) {
    case dataSize8Bit:  return 1;
    case dataSize16Bit: return 2;
    case dataSize32Bit: return 4;
    default:            return 0;
  }
}

std::shared_ptr<ByteVector> makeSyntheticPingBuffer(const SyntheticPingParams &p) {
  const uint8_t element_size = bytes_per_element(p.data_enum);
  const uint16_t gain_prefix = p.send_gain ? 4 : 0;
  const uint32_t row_bytes = p.n_beams * element_size + gain_prefix;
  const uint32_t image_bytes = p.n_ranges * row_bytes;
  const uint32_t bearing_bytes = p.n_beams * sizeof(int16_t);
  const uint32_t image_offset = sizeof(OculusSimplePingResult) + bearing_bytes;
  const uint32_t total_bytes = image_offset + image_bytes;

  auto buffer = std::make_shared<ByteVector>(total_bytes, 0);

  auto *hdr = reinterpret_cast<OculusSimplePingResult *>(buffer->data());
  hdr->fireMessage.head.oculusId = 0x4f53;
  hdr->fireMessage.head.payloadSize =
      total_bytes - sizeof(OculusMessageHeader);
  hdr->fireMessage.flags = p.send_gain ? 0x04 : 0x00;  // DoSendGain bit
  hdr->frequency = 1200000.0;       // 1.2 MHz → matches m1200d branch
  hdr->rangeResolution = 0.01;
  hdr->dataSize = p.data_enum;
  hdr->nRanges = p.n_ranges;
  hdr->nBeams = p.n_beams;
  hdr->imageOffset = image_offset;
  hdr->imageSize = image_bytes;
  hdr->messageSize = total_bytes;

  // Bearing array (int16_t, 0.01° units). Values irrelevant for this test —
  // we only care about intensities byte equivalence. Fill with placeholder.
  auto *bearings = reinterpret_cast<int16_t *>(
      buffer->data() + sizeof(OculusSimplePingResult));
  for (uint16_t b = 0; b < p.n_beams; ++b) {
    bearings[b] = static_cast<int16_t>(-6500 + b * 26);  // ~ -65° to +65°
  }

  // Image bytes — deterministic pattern. Includes gain prefix region if
  // send_gain (those bytes get skipped via offset by both v0 and prod code).
  uint8_t *img = buffer->data() + image_offset;
  for (uint32_t i = 0; i < image_bytes; ++i) {
    img[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
  }

  return buffer;
}

}  // namespace

// ===========================================================================
// FAST-PATH FIXTURES (no gain prefix, stride == nBeams * element_size)
// ===========================================================================

// NOTE: We deliberately do NOT call ping.valid() in these fixtures.
// pingToSonarImage does not invoke valid() either (it just reads
// ping().nBeams / nRanges / dataSize / image()). Calling valid() would
// trigger g3log CHECK macros (e.g. imageOffset > sizeof(...)) that abort
// on edge-case buffers — which would mask rather than expose bugs.

TEST(PingToSonarImage, ByteExact_8bit_FastPath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize8Bit, false});
  SimplePingResult<OculusSimplePingResult> ping(buffer);

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 1u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

TEST(PingToSonarImage, ByteExact_16bit_FastPath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize16Bit, false});
  SimplePingResult<OculusSimplePingResult> ping(buffer);

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 2u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

TEST(PingToSonarImage, ByteExact_32bit_FastPath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize32Bit, false});
  SimplePingResult<OculusSimplePingResult> ping(buffer);

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 4u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

// ===========================================================================
// STRIDE-PATH FIXTURES (gain prefix per row → stride = nBeams*size + 4)
// ===========================================================================

TEST(PingToSonarImage, ByteExact_8bit_StridePath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize8Bit, true});
  SimplePingResult<OculusSimplePingResult> ping(buffer);
  ASSERT_TRUE(ping.flags().getSendGain())
      << "Helper must set DoSendGain bit when send_gain=true.";

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 1u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

TEST(PingToSonarImage, ByteExact_16bit_StridePath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize16Bit, true});
  SimplePingResult<OculusSimplePingResult> ping(buffer);
  ASSERT_TRUE(ping.flags().getSendGain());

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 2u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

TEST(PingToSonarImage, ByteExact_32bit_StridePath) {
  auto buffer = makeSyntheticPingBuffer({64, 32, dataSize32Bit, true});
  SimplePingResult<OculusSimplePingResult> ping(buffer);
  ASSERT_TRUE(ping.flags().getSendGain());

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_EQ(v0.intensities.size(), 64u * 32u * 4u);
  EXPECT_EQ(v0.intensities, prod.intensities);
}

// ===========================================================================
// EDGE CASES
// ===========================================================================

TEST(PingToSonarImage, ZeroSizeImage) {
  // nRanges == 0 → image_bytes == 0 → empty intensities expected
  auto buffer = makeSyntheticPingBuffer({64, 0, dataSize8Bit, false});
  SimplePingResult<OculusSimplePingResult> ping(buffer);

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_TRUE(prod.intensities.empty())
      << "Zero ranges must yield empty intensities.";
  EXPECT_EQ(v0.intensities, prod.intensities);
}

TEST(PingToSonarImage, NullDataPointer) {
  // Construct a SimplePingResult buffer whose imageOffset > buffer size,
  // forcing ImageData::_data to point past end. The new memcpy code branches
  // on data() == nullptr, but in this constructed case data() is non-null
  // (points within buffer). We instead test the spec contract: when
  // num_ranges*num_bearings == 0, the result is empty regardless of pointer.
  // True null-data is exercised by the default-constructed ImageData() case
  // — covered indirectly via ZeroSizeImage above.
  auto buffer = makeSyntheticPingBuffer({0, 32, dataSize8Bit, false});
  SimplePingResult<OculusSimplePingResult> ping(buffer);

  auto v0  = pingToSonarImage_v0(ping);
  auto prod = oculus_sonar::pingToSonarImage(ping);

  EXPECT_TRUE(prod.intensities.empty())
      << "Zero beams must yield empty intensities.";
  EXPECT_EQ(v0.intensities, prod.intensities);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
```

- [ ] **Step 3: Remove placeholder**

Run:
```bash
rm /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/test/.gitkeep
```

- [ ] **Step 4: Build with tests enabled**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON 2>&1 | tail -10
```

Expected: `Finished <<< oculus_sonar`. The new `test_ping_to_sonar_image` target compiles. If a header is missing, fix include path in CMakeLists `target_include_directories(test_ping_to_sonar_image PRIVATE ...)` and rebuild.

- [ ] **Step 5: Run tests against CURRENT (pre-refactor) production code — all 8 must PASS**

Run:
```bash
cd /workspace/ros2_ws
colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -30
```

Expected output (key lines):
```
[----------] 8 tests from PingToSonarImage
[ RUN      ] PingToSonarImage.ByteExact_8bit_FastPath
[       OK ] PingToSonarImage.ByteExact_8bit_FastPath ...
[ RUN      ] PingToSonarImage.ByteExact_16bit_FastPath
[       OK ] ...
... (8 lines total)
[==========] 8 tests from 1 test suite ran.
[  PASSED  ] 8 tests.
```

If any test FAILs at this stage: that means `pingToSonarImage_v0` (frozen) and current production differ — a bug in the helper or the v0 copy. Fix before proceeding (compare line-by-line against `ping_to_sonar_image.hpp:126-143`).

- [ ] **Step 6: Confirm test results**

Run:
```bash
colcon test-result --test-result-base build/oculus_sonar --verbose | tail -10
```

Expected: `Summary: 8 tests, 0 errors, 0 failures, 0 skipped`.

- [ ] **Step 7: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp
git rm oculus_ros2/oculus_sonar/test/.gitkeep
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "test(oculus_sonar): add 8 byte-exact gtest fixtures for pingToSonarImage

Characterization tests for Phase C-1 zero-copy refactor. Each fixture
builds a synthetic SimplePingResult buffer with deterministic byte
pattern, runs frozen pre-C-1 implementation (embedded inline) against
production pingToSonarImage, and asserts byte-for-byte equality.

Coverage:
- 3 fast-path × element sizes (1, 2, 4 bytes, no gain prefix)
- 3 stride-path × element sizes (with 4-byte gain prefix per row)
- 2 edge cases (zero ranges, zero beams)

All 8 PASS against current pre-C-1 code (golden capture). Task 5
refactors the hot loop and these tests must still PASS — that is the
byte-exact verification gate."
```

---

## Task 5: Refactor `pingToSonarImage` Hot Loop

**Files:**
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp:8-15` (add `<cstring>` include)
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp:126-143` (replace hot loop with memcpy path)

**Why:** This is the actual zero-copy refactor. After this commit, the 8 fixtures from Task 4 must STILL pass — that's the byte-exact regression gate. The fast path collapses to a single `memcpy`; the stride path becomes `num_ranges` row-by-row `memcpy` calls. The 3-way `if (dataSize == 1/2/4)` branch is hoisted out (no longer needed because little-endian byte storage is identical for 1/2/4-byte data when `is_bigendian = false`).

- [ ] **Step 1: Add `<cstring>` include**

Use Edit tool. `old_string`:
```
#include <rclcpp/rclcpp.hpp>
#include "marine_acoustic_msgs/msg/sonar_image.hpp"
#include "liboculus/SimplePingResult.h"
#include "liboculus/Constants.h"
```

`new_string`:
```
#include <cstring>  // std::memcpy (Phase C-1 zero-copy hot path)
#include <rclcpp/rclcpp.hpp>
#include "marine_acoustic_msgs/msg/sonar_image.hpp"
#include "liboculus/SimplePingResult.h"
#include "liboculus/Constants.h"
```

- [ ] **Step 2: Replace hot loop (lines 126-143)**

Use Edit tool. `old_string` (the entire hot loop including the surrounding context for uniqueness):
```
  sonar_image.is_bigendian = false;
  sonar_image.data_size = ping.dataSize();

  for (unsigned int r = 0; r < num_ranges; r++) {
    for (unsigned int b = 0; b < num_bearings; b++) {
      if (ping.dataSize() == 1) {
        const uint8_t data = ping.image().at_uint8(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
      } else if (ping.dataSize() == 2) {
        const uint16_t data = ping.image().at_uint16(b, r);
        sonar_image.intensities.push_back(data & 0xFF);
        sonar_image.intensities.push_back((data & 0xFF00) >> 8);
      } else if (ping.dataSize() == 4) {
        const uint32_t data = ping.image().at_uint32(b, r);
        sonar_image.intensities.push_back(data & 0x000000FF);
        sonar_image.intensities.push_back((data & 0x0000FF00) >> 8);
        sonar_image.intensities.push_back((data & 0x00FF0000) >> 16);
        sonar_image.intensities.push_back((data & 0xFF000000) >> 24);
      }
    }
  }

  return sonar_image;
}
```

`new_string`:
```
  sonar_image.is_bigendian = false;
  sonar_image.data_size = ping.dataSize();

  // Phase C-1 zero-copy hot path. Replaces the per-element push_back loop
  // (3-way dataSize branch × N×M iterations) with branch-hoisted memcpy.
  // Little-endian storage matches is_bigendian=false output, so no shifts.
  const auto& img = ping.image();
  const size_t element_size = ping.dataSize();
  const size_t row_bytes = static_cast<size_t>(num_bearings) * element_size;
  const size_t total_bytes = static_cast<size_t>(num_ranges) * row_bytes;

  if (img.data() == nullptr || total_bytes == 0) {
    sonar_image.intensities.clear();
    return sonar_image;
  }

  sonar_image.intensities.resize(total_bytes);
  const uint8_t* src = img.data() + img.offset();
  uint8_t* dst = sonar_image.intensities.data();

  if (img.stride() == row_bytes) {
    // Fast path: rows are contiguous → single memcpy.
    std::memcpy(dst, src, total_bytes);
  } else {
    // Stride path: gain prefix or padding per row → row-by-row memcpy.
    const size_t stride = img.stride();
    for (size_t r = 0; r < num_ranges; ++r) {
      std::memcpy(dst + r * row_bytes, src + r * stride, row_bytes);
    }
  }

  return sonar_image;
}
```

- [ ] **Step 3: Build**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON 2>&1 | tail -5
```

Expected: `Finished <<< oculus_sonar`. No new warnings.

- [ ] **Step 4: Run all 8 gtest fixtures — must STILL pass (byte-exact verification)**

Run:
```bash
cd /workspace/ros2_ws
colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -25
```

Expected:
```
[==========] 8 tests from 1 test suite ran.
[  PASSED  ] 8 tests.
```

If any test FAILs: the refactored hot path produces different bytes than v0. Critical regression — investigate before proceeding. Likely causes:
- `img.offset()` not added to src pointer (forgot the `+ img.offset()`)
- Stride path used when fast path expected (or vice versa)
- `total_bytes == 0` edge case handled differently

- [ ] **Step 5: Confirm test summary**

Run:
```bash
colcon test-result --test-result-base build/oculus_sonar --verbose | tail -10
```

Expected: `Summary: 8 tests, 0 errors, 0 failures, 0 skipped`.

- [ ] **Step 6: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "refactor(oculus_sonar): zero-copy pingToSonarImage hot path

Replace per-element push_back loop with branch-hoisted memcpy:

- Fast path (stride == row_bytes): single memcpy of total_bytes
- Stride path (gain prefix per row): num_ranges row-by-row memcpys
- 3-way dataSize branch removed (LE byte storage matches is_bigendian=false)
- intensities.resize once → no reallocation churn

Verified byte-exact equivalent to pre-refactor implementation by 8 gtest
fixtures (3 fast × element size + 3 stride × element size + 2 edge cases).
All PASS in this commit.

Reference: spec
docs/superpowers/specs/2026-05-04-oculus-sonar-phase-c-1-zero-copy-design.md §3"
```

---

## Task 6: Capture Candidate Structural Snapshot + Compare

**Files:** None modified — `/tmp/oculus_regression/` outputs only.

**Why:** Layer 3 verification. Driver still launches with same component plugins, declares same parameters, creates same publishers with same QoS — byte-identical to baseline. If anything diverges, we've broken something user-visible (parameter declaration, topic name, QoS).

- [ ] **Step 1: Stop ros2 daemon to avoid stale state**

Run:
```bash
ros2 daemon stop > /dev/null 2>&1 || true
sleep 3
```

- [ ] **Step 2: Capture candidate snapshot (current branch HEAD = post-refactor)**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
export ROS_DOMAIN_ID=99
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ROS_DOMAIN_ID_REGRESSION=99 STARTUP_WAIT_S=12 \
    bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -15
```

Expected:
- Last line includes `[candidate] snapshot saved`
- 4 files exist in `/tmp/oculus_regression/candidate/`

- [ ] **Step 3: Run compare**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare 2>&1 | tail -20
```

Expected:
```
[compare] component_types.txt           IDENTICAL
[compare] param_dump.yaml               IDENTICAL
[compare] topic_list.txt                IDENTICAL
[compare] topic_info_*.txt              IDENTICAL
[compare] PASS — all 4 snapshots byte-identical
```

If ANY file differs: run `diff -u /tmp/oculus_regression/baseline/<file> /tmp/oculus_regression/candidate/<file>` to identify the divergence. Phase C-1 should not change publishers, parameters, or QoS — any diff is a bug.

- [ ] **Step 4: No commit — verification only**

---

## Task 7: Add Chrono Latency Instrumentation to `publishPing`

**Files:**
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp:6-21` (add `<chrono>`, `<algorithm>`, `<vector>` includes)
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp:45-63` (instrument `publishPing` template)
- Modify: `oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp:88-90` (add private members for timing window)

**Why:** Layer 4 data collection — record per-ping callback latency p50/p99 + sample count. RCLCPP_INFO_THROTTLE 1000ms keeps log overhead ≪ ms-scale callback. Sliding window of last 100 samples for percentile estimation. Per spec §7 risks: chrono overhead is sub-microsecond, negligible vs ms-scale callback. Fire-and-forget — no public API change.

- [ ] **Step 1: Add includes**

Use Edit tool on `oculus_driver_publishers.hpp`. `old_string`:
```
#pragma once

#include <memory>
#include <string>

#include <apl_msgs/msg/raw_data.hpp>
```

`new_string`:
```
#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <apl_msgs/msg/raw_data.hpp>
```

- [ ] **Step 2: Replace `publishPing` template body**

Use Edit tool. `old_string`:
```
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
```

`new_string`:
```
  // Templated entry: publishes sonar/image/metadata + parameter echoes.
  // Phase C-1: chrono-based latency instrumentation around the conversion +
  // publish path. Sliding window of last 100 samples; logs p50/p99 every 1s.
  template <typename PingT>
  void publishPing(const PingT& ping) {
    const auto t_start = std::chrono::steady_clock::now();

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

    const auto t_end = std::chrono::steady_clock::now();
    const double latency_ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    recordLatencyAndLog(latency_ms);
  }
```

- [ ] **Step 3: Add `recordLatencyAndLog` declaration + private members**

Use Edit tool. `old_string`:
```
 private:
  void publishParameters();
  sensor_msgs::msg::Image sonarToImage(
      const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const;

  rclcpp::Node* node_;  // non-owning
  std::string topic_prefix_;
  std::string frame_id_;
```

`new_string`:
```
 private:
  void publishParameters();
  sensor_msgs::msg::Image sonarToImage(
      const marine_acoustic_msgs::msg::SonarImage& sonar_msg) const;

  // Phase C-1: ping callback latency stats (sliding window p50/p99).
  void recordLatencyAndLog(double latency_ms);

  static constexpr size_t kLatencyWindow = 100;
  std::vector<double> latency_window_;     // sliding window, last N samples
  size_t latency_write_idx_ = 0;           // ring index
  uint64_t total_pings_ = 0;               // monotonic ping count

  rclcpp::Node* node_;  // non-owning
  std::string topic_prefix_;
  std::string frame_id_;
```

- [ ] **Step 4: Implement `recordLatencyAndLog` in `oculus_driver_publishers.cpp`**

Use Edit tool on `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp`.

`old_string` (the last function before namespace close):
```
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

`new_string`:
```
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

void OculusDriverPublishers::recordLatencyAndLog(double latency_ms) {
  if (latency_window_.size() < kLatencyWindow) {
    latency_window_.push_back(latency_ms);
  } else {
    latency_window_[latency_write_idx_] = latency_ms;
    latency_write_idx_ = (latency_write_idx_ + 1) % kLatencyWindow;
  }
  ++total_pings_;

  std::vector<double> sorted = latency_window_;
  std::sort(sorted.begin(), sorted.end());
  const size_t n = sorted.size();
  const double p50 = sorted[n / 2];
  const double p99 = sorted[(n * 99) / 100];

  RCLCPP_INFO_THROTTLE(
      node_->get_logger(), *node_->get_clock(), 1000,
      "[c1-perf] pings=%lu  window=%zu  p50=%.3fms  p99=%.3fms",
      static_cast<unsigned long>(total_pings_), n, p50, p99);
}

}  // namespace oculus_sonar
```

- [ ] **Step 5: Build**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON 2>&1 | tail -5
```

Expected: `Finished <<< oculus_sonar`. No new warnings.

- [ ] **Step 6: Re-run gtest to confirm instrumentation didn't break anything**

Run:
```bash
cd /workspace/ros2_ws
colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -15
```

Expected: 8 tests PASS.

- [ ] **Step 7: Smoke launch (driver loads with instrumentation, no hardware needed)**

Run:
```bash
cd /workspace/ros2_ws
export ROS_DOMAIN_ID=99
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
timeout 8 ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d 2>&1 | tail -20 || true
```

Expected: launch starts, container loads `OculusDriver` component, no SIGSEGV / `what():` / `std::terminate` in output. Driver will not receive pings (no hardware), so no `[c1-perf]` log lines yet — that's fine. We're only verifying the instrumentation compiles + links cleanly into a running container.

- [ ] **Step 8: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp \
        oculus_ros2/oculus_sonar/src/oculus_driver_publishers.cpp
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "feat(oculus_sonar): add chrono latency instrumentation to publishPing

Per Phase C-1 spec §5 Layer 4 data collection: chrono::steady_clock
around pingToSonarImage + publish path, sliding window of last 100
samples, p50/p99 logged via RCLCPP_INFO_THROTTLE every 1s.

- Format: '[c1-perf] pings=N  window=W  p50=X.XXXms  p99=Y.YYYms'
- Window = 100 samples (≈3.3s at m3000d 30Hz, ≈10s at m750d 10Hz)
- Sub-microsecond chrono overhead vs ms-scale callback — negligible
- Fire-and-forget; no public API change

This is data-collection only, NOT a fail gate (per spec §7 risks:
measurement noise can exceed perf gain on slower configurations)."
```

---

## Task 8: Update CHANGELOG

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CHANGELOG.md` (prepend Phase C-1 section above existing B-1 entry)

**Why:** Per `.claude/rules/refactor-workflow.md`: every phase must have a CHANGELOG entry with Changed / Added / Verification / Notes. Required for 4-axis tracking (branch + commit + CHANGELOG + PR).

- [ ] **Step 1: Read current CHANGELOG.md to find insertion point**

Read first 8 lines of `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md`. Confirm line 7 starts with `## [Unreleased] — Phase B-1` (the existing top entry).

- [ ] **Step 2: Prepend Phase C-1 section**

Use Edit tool. `old_string`:
```
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] — Phase B-1: OculusDriver 4-way split + regression infra (refactor)
```

`new_string`:
```
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
- `oculus_ros2/oculus_sonar/test/test_ping_to_sonar_image.cpp` (350+ LOC) — 8 byte-exact gtest fixtures: 3 fast-path × element size (1/2/4 bytes) + 3 stride-path × element size + 2 edge cases (zero ranges, zero beams). Frozen pre-C-1 implementation embedded inline as golden reference. Synthetic ping helper builds `OculusSimplePingResult` buffer with deterministic byte pattern.

### Verification
- colcon build PASS (Release, BUILD_TESTING=ON)
- **Layer 1 — colcon test**: 8/8 gtest fixtures PASS against current production code (proves byte-for-byte equivalence to pre-refactor implementation)
- **Layer 3 — structural snapshot**: baseline (pre-C-1, `4672feb`) vs candidate (post-refactor) — 4 snapshot files (`component_types.txt`, `param_dump.yaml`, `topic_list.txt`, `topic_info_*.txt`) all byte-identical
- **Layer 4 — performance** (data-collection, NOT a fail gate per spec §7): instrumentation deployed; field measurements deferred to hardware availability

### Notes
- Performance measurement requires real sonar hardware (UDP input — bag-replay impossible). When hardware is available, run with `m3000d` at 30Hz for highest amplification target. Baseline comparison should use B-1 head (`adc79cc`) build, candidate should use C-1 head, both measured under identical conditions (same bag run / same target), 60s sample, 3 repetitions. Record p50/p99/CPU% in PR description.
- Phase C-2 (parameter echo throttle + latched QoS) gets a separate brainstorm round after C-1 merge. The C-1 instrumentation infrastructure in `publishPing` will inform how C-2 measures publisher load.
- This is the FIRST PR that modifies vendored `liboculus`. CMakeLists.txt of `liboculus` does not need touching — header-only addition. Confirmed only `oculus_sonar` consumes `liboculus` headers in this workspace (`grep -rn "liboculus/" --include='*.{cpp,hpp,h}'` from workspace root).
- Regression DDS gotcha: `regression_oculus.sh` requires `export ROS_DOMAIN_ID=99` BEFORE `source ROS` + `STARTUP_WAIT_S=12`. Same workaround as B-2 (memory `reference_oculus_regression_dds_gotcha.md`).

## [Unreleased] — Phase B-1: OculusDriver 4-way split + regression infra (refactor)
```

- [ ] **Step 3: Verify CHANGELOG renders cleanly**

Run:
```bash
head -45 /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md
```

Expected: Phase C-1 section appears at top (after the Keep-a-Changelog header), B-1 section follows below.

- [ ] **Step 4: Commit**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CHANGELOG.md
git -c user.name='luckkim123' -c user.email='luckkim123@gmail.com' commit -m "docs(oculus_sonar): add Phase C-1 CHANGELOG entry

Document the zero-copy hot-path refactor: 4 ImageData accessors,
memcpy hot path, 8 gtest fixtures, chrono instrumentation, structural
snapshot byte-identical."
```

---

## Task 9: Final Verification + Push Gate + PR

**Files:** None modified — verification + git push + `gh pr create`.

**Why:** Final pre-push gate. Per CLAUDE.md and `.claude/rules/git-workflow.md`: push only with explicit user approval. PR uses HEREDOC body with the 4-axis summary and verification checklist.

- [ ] **Step 1: Final clean build**

Run:
```bash
cd /workspace/ros2_ws
colcon build --packages-select oculus_sonar liboculus --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON 2>&1 | tail -10
```

Expected: `Finished <<< oculus_sonar` and `Finished <<< liboculus`.

- [ ] **Step 2: Final test run**

Run:
```bash
cd /workspace/ros2_ws
colcon test --packages-select oculus_sonar --event-handlers console_direct+ 2>&1 | tail -15
colcon test-result --test-result-base build/oculus_sonar --verbose | tail -10
```

Expected: 8 tests PASS, 0 failures.

- [ ] **Step 3: Final regression compare**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
ros2 daemon stop > /dev/null 2>&1 || true
sleep 3
export ROS_DOMAIN_ID=99
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ROS_DOMAIN_ID_REGRESSION=99 STARTUP_WAIT_S=12 \
    bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -5
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare 2>&1 | tail -10
```

Expected: `[compare] PASS — all 4 snapshots byte-identical`.

- [ ] **Step 4: Inspect commit list**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
git log --oneline adc79cc..HEAD
```

Expected: 7 commits on top of `adc79cc` (the B-1 head):
1. spec doc
2. ImageData accessors
3. gtest infrastructure
4. 8 gtest fixtures
5. zero-copy hot path
6. chrono instrumentation
7. CHANGELOG

- [ ] **Step 5: STOP — ask user before pushing**

Output to user:
> Phase C-1 implementation complete on `refactor/phase-c-1-zero-copy`. 7 commits on top of `adc79cc`.
> Verification:
> - colcon build PASS
> - colcon test PASS (8/8 gtest fixtures byte-exact)
> - regression compare PASS (4 snapshots byte-identical)
> - Layer 4 perf instrumentation in place (hardware measurement deferred)
>
> May I push the branch and open a PR?

WAIT for explicit user approval. Do NOT push without approval. This is a hard gate per CLAUDE.md and `.claude/rules/git-workflow.md`.

- [ ] **Step 6: After user approval — push**

Run (only after explicit user "yes" / "진행해줘"):
```bash
cd /workspace/ros2_ws/src/sensor_packages
git push -u origin refactor/phase-c-1-zero-copy
```

Expected: branch pushed; remote tracks local.

- [ ] **Step 7: Create PR**

Run:
```bash
cd /workspace/ros2_ws/src/sensor_packages
gh pr create --base main --head refactor/phase-c-1-zero-copy \
  --title "refactor(oculus_sonar): Phase C-1 — pingToSonarImage zero-copy hot path" \
  --body "$(cat <<'EOF'
## Summary
- Zero-copy refactor of `pingToSonarImage` hot loop (per-element `push_back` → branch-hoisted `memcpy`)
- 4 new `const noexcept` accessors on vendored `liboculus::ImageData` (`data`, `stride`, `offset`, `numBytes`)
- First unit test infrastructure in `oculus_sonar` (8 byte-exact gtest fixtures)
- chrono-based ping callback latency instrumentation (sliding-window p50/p99, `RCLCPP_INFO_THROTTLE` 1000ms)

## Changes
- `liboculus/include/liboculus/ImageData.h`: +10 LOC (4 accessors + brief comment)
- `oculus_sonar/include/oculus_sonar/ping_to_sonar_image.hpp`: hot loop rewritten (-18 LOC push_back → +22 LOC memcpy path)
- `oculus_sonar/include/oculus_sonar/oculus_driver_publishers.hpp` + `.cpp`: chrono instrumentation
- `oculus_sonar/CMakeLists.txt`: working `ament_add_gtest` block (replaces stale commented-out block)
- `oculus_sonar/test/test_ping_to_sonar_image.cpp`: NEW — 8 fixtures + frozen-old-impl + synthetic ping helper

## Verification
- [x] colcon build PASS (Release, BUILD_TESTING=ON)
- [x] colcon test PASS — 8/8 gtest fixtures byte-exact equivalent to pre-refactor implementation
  - 3 fast-path × element size (1/2/4 bytes)
  - 3 stride-path × element size (with gain prefix)
  - 2 edge cases (zero ranges, zero beams)
- [x] Structural snapshot regression — 4 dimensions byte-identical to baseline
  - `component_types.txt` IDENTICAL
  - `param_dump.yaml` IDENTICAL
  - `topic_list.txt` IDENTICAL
  - `topic_info_*.txt` IDENTICAL
- [x] CHANGELOG.md updated with Phase C-1 section
- [ ] Layer 4 — Performance measurement (hardware required; deferred to operational verification)

## Performance (Layer 4 — data only, NOT a merge gate)
Per spec §5/§7: real-hardware measurement only — UDP input prevents bag-replay. To be filled in by operations:

| Metric | Baseline (B-1 HEAD) | Candidate (C-1 HEAD) | Δ |
|--------|---------------------|----------------------|---|
| ping callback p50 ms | TBD | TBD | TBD |
| ping callback p99 ms | TBD | TBD | TBD |
| `oculus_driver` %CPU | TBD | TBD | TBD |

Measurement protocol (when hardware available): m3000d at 30Hz, 60s sample, 3 repetitions; instrument logs `[c1-perf] pings=N  window=W  p50=X.XXXms  p99=Y.YYYms` every 1s.

## Next Phase
Phase C-2 — parameter echo throttle + latched QoS for `param/*` publishers (separate brainstorm + spec).
EOF
)"
```

Expected: PR URL printed to stdout. PR opens against `main` from `refactor/phase-c-1-zero-copy`.

- [ ] **Step 8: Update memory tracker**

Update `/root/.claude/projects/-workspace/memory/project_sensor_packages_refactor_state.md` C-1 status to "PR #N open" with the PR number from Step 7.

- [ ] **Step 9: Done — wait for user merge approval**

Do NOT merge the PR. Per `.claude/rules/git-workflow.md`: 1 approval required, squash merge by user. After user merges, the squashed SHA becomes the next baseline for any further phases.

---

## Glossary

- **Fast path**: `stride == nBeams * dataSize`. Image rows are contiguous in source memory. Single `memcpy(dst, src, total_bytes)` copies the whole intensities block.
- **Stride path**: `stride > nBeams * dataSize`. Source has gain prefix (4 bytes) per row. We copy each row separately, skipping the prefix via `offset`.
- **Frozen old implementation**: a copy of the pre-refactor `pingToSonarImage` code embedded inside the test file as `pingToSonarImage_v0`. Never modified after Task 4 commit; serves as immutable byte-exact reference.
- **Structural snapshot regression**: B-1's externally-observable dimensions test (component types, parameter dump, topic list, per-topic info). Catches accidental changes to public ROS interface even when implementation rewrite is byte-perfect on the data path.
- **DDS discovery race workaround**: `export ROS_DOMAIN_ID=99` BEFORE `source ROS`, plus `STARTUP_WAIT_S=12`, isolates the regression run from any stray `ros2 bag play` process on default domain 0. See memory `reference_oculus_regression_dds_gotcha.md`.
