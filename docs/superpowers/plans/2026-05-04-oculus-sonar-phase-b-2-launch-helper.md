# Phase B-2: Launch Helper Extraction — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract `oculus.launch.py` boilerplate into `launch/_oculus_common.py` (3 pure helpers) and slim the entry-point launch file from 323 LOC → ≤150 LOC, preserving every advertised topic, QoS, and parameter byte-identically.

**Architecture:** Three pure helper functions live in `launch/_oculus_common.py` (internal to oculus_sonar — NOT cross-package). `oculus.launch.py` keeps `DeclareLaunchArgument` blocks and a 40-line `launch_setup` that reads context, assembles two override dicts, and delegates container construction to `make_sonar_container()`. Duplicate documentation (90-line module docstring + 46-line trailing comment block) is dropped because Phase A's README is the canonical source.

**Tech Stack:** ROS2 Humble launch system, Python 3, ComposableNodeContainer (`launch_ros.actions`).

---

## Pre-Flight (Decisions & References)

- **Branch baseline**: `adc79cc` — sensor_packages main HEAD immediately after Phase B-1 merge (PR #2 squash). Confirm with `git log --oneline -1 main` before starting.
- **Spec section**: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-2 (lines 190-198).
- **Helper location decision**: `launch/_oculus_common.py` inside `oculus_sonar` package. NOT a new shared `sensor_packages/launch_utils/` — that decision is deferred until ping360_sonar refactor establishes a second consumer.
- **Helper API** (3 functions, exact signatures):
  - `sonar_model_to_config_path(model: str) -> str` — resolves config YAML, raises `FileNotFoundError` with model whitelist if missing.
  - `topic_namespace(model: str) -> str` — returns `/sensor/sonar/oculus/{model}` (no trailing slash).
  - `make_sonar_container(model, with_fan, driver_overrides, fan_imager_overrides, *, container_name=None) -> ComposableNodeContainer` — assembles driver + optional fan_imager composable nodes.
- **Regression infra reuse**: B-1's `scripts/regression_oculus.sh` runs unmodified. Baseline snapshot is RECAPTURED at `adc79cc` (B-1 baseline at `80b2c5e` is too old).
- **Verification scope** (per spec §8 B-2 "Python launch라 회귀 위험 매우 낮음"):
  - Layer 1 (Tasks 4 + 6 gate): `regression_oculus.sh compare` byte-identical for default `SONAR_MODEL=m750d`.
  - Layer 2 (Task 5 only): `ros2 launch --show-args` proves all 11 args declared; `with_fan:=true` actual launch proves the alternate code path doesn't crash.
- **Documentation dedup**: Module docstring lines 1-90 → 10 lines pointing to README. Trailing comment block lines 278-323 → deleted entirely.

---

## File Structure (Locked)

| File | Status | Responsibility |
|------|--------|---------------|
| `oculus_ros2/oculus_sonar/launch/_oculus_common.py` | Create | 3 pure helpers (no LaunchConfiguration access — receives plain str/bool/dict) |
| `oculus_ros2/oculus_sonar/launch/oculus.launch.py` | Modify (323→≤150) | 11 `DeclareLaunchArgument` + `launch_setup` glue (read context → build dicts → delegate) |

The helper module receives ONLY plain Python types (`str`, `bool`, `dict`). LaunchConfiguration `.perform(context)` calls remain in `launch_setup` — this keeps the helper unit-testable without a launch context.

---

## Verification Strategy

### Layer 1 — Structural snapshot (per-task gate, Tasks 4 + 6)

Reuse B-1's `scripts/regression_oculus.sh` (4 dimensions: component types, param dump, topic list, per-topic info).
- Baseline captured at `adc79cc` (Task 2).
- Candidate captured after each refactor commit (Tasks 4, 6).
- PASS criterion: 4 snapshot files byte-identical.

### Layer 2 — Functional sanity (Task 5 only)

Catches launch-file-specific regressions the structural snapshot misses (e.g. dropped `DeclareLaunchArgument`, broken `with_fan` path).
- `ros2 launch oculus_sonar oculus.launch.py --show-args` returns all 11 declared args (count + names match baseline).
- `ros2 launch oculus_sonar oculus.launch.py with_fan:=true` runs 5 seconds without crashing AND `ros2 topic list | grep fan_image` returns 1 entry.

---

## Task 1: Branch + Baseline Build

**Files:** No code changes — environment setup only.

- [ ] **Step 1: Verify on main HEAD `adc79cc`**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git log --oneline -1 main
```

Expected: `adc79cc refactor(oculus_sonar): Phase B-1 — OculusDriver 4-way split + regression infra (#2)`. If the SHA differs, pull main first: `git checkout main && git pull --ff-only origin main`.

- [ ] **Step 2: Verify on the B-2 feature branch**

The branch `refactor/phase-b2-launch-helper` was created by the controller and the plan file was committed as the first commit (matches B-1 pattern). Verify state:

```bash
cd /workspace/ros2_ws/src/sensor_packages
git checkout refactor/phase-b2-launch-helper
git log --oneline -3
git status
```

Expected:
- HEAD line: `<sha> docs(oculus_sonar): add Phase B-2 implementation plan`
- 1 commit ahead of main
- Clean working tree

If the branch doesn't exist, the controller failed to create it — do it now:
```bash
git checkout -b refactor/phase-b2-launch-helper main
git add docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-2-launch-helper.md
git commit -m "docs(oculus_sonar): add Phase B-2 implementation plan"
```

- [ ] **Step 3: Build oculus_sonar from main HEAD (baseline binaries)**

```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar 2>&1 | tail -5
source install/setup.bash
```

Expected: `Finished <<< oculus_sonar [Xs]`. The `share/oculus_sonar/launch/oculus.launch.py` install reflects current main (323 LOC).

---

## Task 2: Capture B-2 Baseline Snapshot

**Files:** No code changes — generates `/tmp/oculus_regression/baseline/` from main HEAD.

- [ ] **Step 1: Stop ros2 daemon (avoid stale-state flake)**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ros2 daemon stop 2>&1 | tail -1
```

Expected: `The daemon has been stopped`.

- [ ] **Step 2: Capture baseline**

```bash
cd /workspace/ros2_ws/src/sensor_packages
rm -rf /tmp/oculus_regression/baseline
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh baseline 2>&1 | tail -10
```

Expected output ends with:
```
[baseline] saved to /tmp/oculus_regression/baseline
# oculus_sonar structural snapshot
label: baseline
sonar_model: m750d
captured_at: 2026-05-04T...
...
03_topic_list.txt  lines=16  hash=<sha256>
04_topic_info.txt  lines=304  hash=<sha256>
```

- [ ] **Step 3: Sanity-check baseline file count**

```bash
ls /tmp/oculus_regression/baseline/
```

Expected: 4 snapshot files + `summary.txt` + `launch.log`:
```
01_component_types.txt
02_param_dump.yaml
03_topic_list.txt
04_topic_info.txt
launch.log
summary.txt
```

If any of `01-04*` are missing, the baseline is incomplete — re-run Step 2 after fixing the cause (most often: ros2 daemon hadn't fully stopped, or oculus_sonar wasn't installed).

---

## Task 3: Create `_oculus_common.py` Helper Module

**Files:**
- Create: `oculus_ros2/oculus_sonar/launch/_oculus_common.py`

This task adds the helper module WITHOUT wiring it up. The launch file still contains all original logic — both old and new code coexist after this task. The actual switch happens in Task 4.

- [ ] **Step 1: Create the helper file**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/launch/_oculus_common.py`:

```python
"""
Internal launch helpers for oculus_sonar.

Used by oculus.launch.py. Not part of the public API — the leading underscore
indicates module-private status. Callers pass plain Python types only (no
LaunchConfiguration objects); the launch entry-point is responsible for
resolving launch arguments via .perform(context).
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

_VALID_MODELS = ('m750d', 'm1200d', 'm3000d')


def sonar_model_to_config_path(model: str) -> str:
    """Resolve the config YAML path for the given sonar model.

    Raises FileNotFoundError (with the valid model whitelist in the message)
    when the YAML is missing — typically because the user passed a typo'd
    model name like 'm700d'.
    """
    config_file = os.path.join(
        get_package_share_directory('oculus_sonar'),
        'config',
        f'{model}_params.yaml',
    )
    if not os.path.exists(config_file):
        raise FileNotFoundError(
            f"Config file not found: {config_file}. "
            f"Valid models: {', '.join(_VALID_MODELS)}"
        )
    return config_file


def topic_namespace(model: str) -> str:
    """Return the per-model topic prefix, e.g. '/sensor/sonar/oculus/m750d'."""
    return f'/sensor/sonar/oculus/{model}'


def make_sonar_container(
    model: str,
    with_fan: bool,
    driver_overrides: dict,
    fan_imager_overrides: dict,
    *,
    container_name: str = None,
) -> ComposableNodeContainer:
    """Build the ComposableNodeContainer holding the OculusDriver and
    (optionally) the OculusFanImager.

    Args:
        model: One of m750d/m1200d/m3000d. Caller must validate.
        with_fan: When True, adds OculusFanImager composable node.
        driver_overrides: Driver param overrides. Empty dict means use
            config-file defaults only.
        fan_imager_overrides: Fan imager param overrides. Ignored when
            with_fan is False; required keys when True: 'input_topic',
            'output_topic', 'apply_colormap', 'colormap', 'qos_reliability'.
            Optional: 'freq_mode'.
        container_name: Override the auto-generated container node name.
            Defaults to f'oculus_{model}_container'.
    """
    config_file = sonar_model_to_config_path(model)

    driver_params = [config_file]
    if driver_overrides:
        driver_params.append(driver_overrides)

    composable_nodes = [
        ComposableNode(
            package='oculus_sonar',
            plugin='oculus_sonar::OculusDriver',
            name=model,
            namespace='oculus',
            parameters=driver_params,
        ),
    ]

    if with_fan:
        composable_nodes.append(
            ComposableNode(
                package='oculus_sonar',
                plugin='oculus_sonar::OculusFanImager',
                name='fan_imager',
                namespace='oculus',
                parameters=[config_file, fan_imager_overrides],
            )
        )

    return ComposableNodeContainer(
        name=container_name or f'oculus_{model}_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=composable_nodes,
        output='screen',
    )
```

- [ ] **Step 2: Build to verify install copies the new file**

```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar 2>&1 | tail -5
source install/setup.bash
ls install/oculus_sonar/share/oculus_sonar/launch/
```

Expected: `Finished <<< oculus_sonar`. The `ls` output includes BOTH `oculus.launch.py` AND `_oculus_common.py`.

If `_oculus_common.py` is missing from share/, oculus_sonar's `CMakeLists.txt` `install(DIRECTORY launch ...)` block isn't picking up new files. Inspect with:
```bash
grep -n "install.*launch" /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CMakeLists.txt
```
A line like `install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})` should already exist (Phase A unified launch). If it does, the missing file means colcon needs `--cmake-clean-cache` once.

- [ ] **Step 3: Smoke-import the helper from outside the launch context**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
python3 -c "
import sys, os
sys.path.insert(0, os.path.join(
    os.environ['AMENT_PREFIX_PATH'].split(':')[0],
    'share/oculus_sonar/launch'
))
from _oculus_common import sonar_model_to_config_path, topic_namespace, make_sonar_container
print('topic_namespace m750d ->', topic_namespace('m750d'))
print('config m750d ->', sonar_model_to_config_path('m750d'))
try:
    sonar_model_to_config_path('m700d')
except FileNotFoundError as e:
    print('whitelist error OK:', str(e)[:80], '...')
"
```

Expected output:
```
topic_namespace m750d -> /sensor/sonar/oculus/m750d
config m750d -> /workspace/ros2_ws/install/oculus_sonar/share/oculus_sonar/config/m750d_params.yaml
whitelist error OK: Config file not found: ... Valid models: m750d, m1200d, m3000d ...
```

If `AMENT_PREFIX_PATH` isn't set, you forgot to source `install/setup.bash` (every Bash invocation needs both `source /opt/ros/humble/setup.bash && source /workspace/ros2_ws/install/setup.bash` — the env doesn't persist across calls).

- [ ] **Step 4: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/launch/_oculus_common.py
git commit -m "feat(oculus_sonar): add _oculus_common launch helpers (B-2 step 1/2)

3 pure helpers for launch composition (no LaunchConfiguration access):
sonar_model_to_config_path (with model whitelist FileNotFoundError),
topic_namespace, make_sonar_container (assembles driver + optional
fan_imager composable nodes into a container).

Module-private (leading underscore). Switching the entry-point launch
file to use these helpers happens in step 2/2."
```

---

## Task 4: Refactor `oculus.launch.py` to Use Helpers

**Files:**
- Modify: `oculus_ros2/oculus_sonar/launch/oculus.launch.py` (323 LOC → expected ≤150)

This task is the "live wire" cutover. After Step 4 of this task, `oculus.launch.py` calls helpers exclusively. Step 5 confirms the structural snapshot stays byte-identical.

- [ ] **Step 1: Replace the entire file contents**

Write to `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/launch/oculus.launch.py` (overwrites all 323 lines):

```python
#!/usr/bin/env python3
"""
oculus.launch.py — Unified ROS2 launch entry point for all Oculus sonar models.

See `oculus_sonar/README.md` for the full argument table, examples, and topic
list. This file declares launch arguments and delegates container assembly to
helpers in `_oculus_common.py`.
"""

import os
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration

# Make sibling _oculus_common importable when this file is executed by ros2 launch
# (ros2 launch sets __file__ to the install/share/.../launch/ path).
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
from _oculus_common import make_sonar_container, topic_namespace  # noqa: E402


def _collect_driver_overrides(context) -> dict:
    """Read driver-param launch arguments and return a dict of only the
    non-empty ones, with appropriate type conversion.

    Empty string ('') means "fall back to config YAML default".

    The (name, converter) order below MIRRORS the original launch file
    lines 124-151 for code-review traceability — change with care.
    """
    args_with_type = (
        ('ip_address', str),
        ('range',      float),
        ('gain',       int),
        ('gamma',      int),
        ('ping_rate',  int),
        ('freq_mode',  int),
        ('data_size',  str),
    )
    overrides = {}
    for name, conv in args_with_type:
        v = LaunchConfiguration(name).perform(context)
        if v:
            overrides[name] = conv(v)
    return overrides


def _collect_fan_imager_overrides(context, model: str) -> dict:
    """Build the fan_imager param dict using launch args + the resolved model
    name. Always includes input/output topic, colormap, and qos_reliability;
    optionally includes freq_mode if the launch-arg version is non-empty.
    """
    apply_colormap = LaunchConfiguration('apply_colormap').perform(context)
    colormap = LaunchConfiguration('colormap').perform(context)
    qos_reliability = LaunchConfiguration('qos_reliability').perform(context)
    freq_mode = LaunchConfiguration('freq_mode').perform(context)

    ns = topic_namespace(model)
    overrides = {
        'input_topic': f'{ns}/sonar',
        'output_topic': f'{ns}/fan_image',
        'apply_colormap': apply_colormap.lower() == 'true' if apply_colormap else True,
        'colormap': colormap if colormap else 'turbo',
        'qos_reliability': qos_reliability if qos_reliability else 'reliable',
    }
    if freq_mode:
        overrides['freq_mode'] = int(freq_mode)
    return overrides


def launch_setup(context, *args, **kwargs):
    model = LaunchConfiguration('sonar_model').perform(context)
    with_fan = LaunchConfiguration('with_fan').perform(context).lower() == 'true'

    driver_overrides = _collect_driver_overrides(context)
    fan_imager_overrides = _collect_fan_imager_overrides(context, model) if with_fan else {}

    container = make_sonar_container(
        model=model,
        with_fan=with_fan,
        driver_overrides=driver_overrides,
        fan_imager_overrides=fan_imager_overrides,
    )
    return [container]


def generate_launch_description():
    return LaunchDescription([
        # Model selection
        DeclareLaunchArgument('sonar_model', default_value='m750d',
                              description='Sonar model: m750d, m1200d, m3000d'),
        DeclareLaunchArgument('with_fan', default_value='false',
                              description='Enable fan image converter: true/false'),

        # Driver parameter overrides (empty = use config YAML)
        DeclareLaunchArgument('ip_address', default_value='',
                              description='Sonar IP address (empty = use config)'),
        DeclareLaunchArgument('range', default_value='',
                              description='Sonar range in meters (empty = use config)'),
        DeclareLaunchArgument('gain', default_value='',
                              description='Gain percentage (empty = use config)'),
        DeclareLaunchArgument('gamma', default_value='',
                              description='Gamma correction (empty = use config)'),
        DeclareLaunchArgument('ping_rate', default_value='',
                              description='Ping rate (empty = use config)'),
        DeclareLaunchArgument('freq_mode', default_value='',
                              description='Frequency mode: 1=low, 2=high (empty = use config)'),
        DeclareLaunchArgument('data_size', default_value='',
                              description='Data size: 8bit, 16bit, 32bit (empty = use config)'),

        # Fan imager parameters
        DeclareLaunchArgument('apply_colormap', default_value='true',
                              description='Apply colormap to fan image: true/false'),
        DeclareLaunchArgument('colormap', default_value='turbo',
                              description='Colormap: jet, hot, viridis, turbo, plasma, magma, '
                                          'inferno, ocean, rainbow, etc.'),
        DeclareLaunchArgument('qos_reliability', default_value='best_effort',
                              description='QoS reliability for subscriber: reliable or best_effort'),

        OpaqueFunction(function=launch_setup),
    ])
```

- [ ] **Step 2: Verify line count**

```bash
wc -l /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/launch/oculus.launch.py
```

Expected: ≤120 LOC (target ~110). If significantly over (e.g. 200+), you accidentally kept the old `launch_setup` body or trailing comment block — re-check Step 1.

- [ ] **Step 3: Rebuild so the install/share copy reflects the change**

```bash
cd /workspace/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select oculus_sonar 2>&1 | tail -5
source install/setup.bash
diff /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/launch/oculus.launch.py \
     /workspace/ros2_ws/install/oculus_sonar/share/oculus_sonar/launch/oculus.launch.py
```

Expected: build PASS, `diff` produces no output (source and install copies identical).

- [ ] **Step 4: Sanity launch (no snapshot — that's Step 5)**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=77
ros2 daemon stop 2>&1 | tail -1
timeout 5 ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d 2>&1 | grep -E "ERROR|Traceback|ImportError" | head -10
```

Expected: no `ERROR`, no `Traceback`, no `ImportError`. The `timeout 5` kills the launch after 5s (driver tries to connect to a sonar that doesn't exist — that's expected and not an error). Empty grep output = PASS. If you see `ImportError: No module named '_oculus_common'`, the `sys.path.insert` line is wrong or `_oculus_common.py` isn't in the same install dir.

- [ ] **Step 5: Structural snapshot regression**

```bash
cd /workspace/ros2_ws/src/sensor_packages
rm -rf /tmp/oculus_regression/candidate
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -10
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected last line: `PASS: structural snapshot identical to baseline`.

If FAIL, the diff output above shows exactly which dimension drifted. Most likely failure modes:
- **02_param_dump.yaml diff**: A driver param was renamed or its default changed. Check `_collect_driver_overrides` keys against the original lines 124-151.
- **03_topic_list.txt diff**: Topic prefix or node name changed. Check `topic_namespace()` output and `make_sonar_container()` `name=` arguments match the originals (`name=model` for driver, `name='fan_imager'` for fan).
- **04_topic_info.txt diff**: A QoS profile changed. Likely `transient_local`/`reliable` flag drift in fan_imager or driver.

- [ ] **Step 6: Commit**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/launch/oculus.launch.py
git commit -m "refactor(oculus_sonar): switch oculus.launch.py to _oculus_common helpers (B-2 step 2/2)

323 LOC → ~110 LOC. Behavior preserved: structural snapshot regression
(component types / param dump / topic list / per-topic info) byte-identical
to baseline.

Removed 90-line module docstring + 46-line trailing comment block (duplicate
of Phase A README). Override-collection logic moved to two private helpers
(_collect_driver_overrides, _collect_fan_imager_overrides) inside the launch
file — they need LaunchConfiguration access so they stay here, not in
_oculus_common."
```

---

## Task 5: Functional Verification — `--show-args` and `with_fan` Path

**Files:** No code changes — independent verification.

This task catches launch-file regressions the structural snapshot misses (e.g. a `DeclareLaunchArgument` accidentally dropped, the `with_fan:=true` code path broken).

- [ ] **Step 1: Verify `--show-args` lists every declared argument by name**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ros2 launch oculus_sonar oculus.launch.py --show-args > /tmp/oculus_b2_show_args.txt 2>&1
echo "--- output ---"
cat /tmp/oculus_b2_show_args.txt
echo "--- presence check ---"
EXPECTED_ARGS="sonar_model with_fan ip_address range gain gamma ping_rate freq_mode data_size apply_colormap colormap qos_reliability"
MISSING=""
for arg in ${EXPECTED_ARGS}; do
    grep -q "'${arg}'" /tmp/oculus_b2_show_args.txt || MISSING="${MISSING} ${arg}"
done
if [[ -n "${MISSING}" ]]; then
    echo "FAIL: missing args:${MISSING}"
    exit 1
fi
echo "PASS: all 12 expected args declared"
```

Expected ending: `PASS: all 12 expected args declared`. The `--show-args` output format quotes argument names as `'name'` regardless of ros2 humble vs jazzy version (humble: `'name' (default: ...)`, jazzy: `Argument: 'name' ...`) — both match the `'${arg}'` grep pattern.

If FAIL, the named arg in `MISSING` was dropped during refactor — re-add the `DeclareLaunchArgument` block to `oculus.launch.py` and rebuild.

- [ ] **Step 2: Verify the `with_fan:=true` code path launches without crashing**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=78
ros2 daemon stop 2>&1 | tail -1
ros2 launch oculus_sonar oculus.launch.py sonar_model:=m750d with_fan:=true \
    > /tmp/oculus_b2_fan_launch.log 2>&1 &
LAUNCH_PID=$!
sleep 5
if ! ps -p "${LAUNCH_PID}" > /dev/null; then
    echo "FAIL: with_fan:=true launch crashed"
    tail -30 /tmp/oculus_b2_fan_launch.log
    exit 1
fi
ros2 topic list | grep -E "oculus/m750d/(sonar|fan_image)" | sort
kill -INT "${LAUNCH_PID}" 2>/dev/null; sleep 2
pkill -P "${LAUNCH_PID}" 2>/dev/null; sleep 1
pkill -f "oculus_m750d_container" 2>/dev/null
```

Expected: launch alive after 5s, and the `ros2 topic list` filter returns BOTH topics:
```
/sensor/sonar/oculus/m750d/fan_image
/sensor/sonar/oculus/m750d/sonar
```

If `fan_image` is missing, `make_sonar_container`'s `with_fan=True` path didn't add the OculusFanImager composable node — check that `with_fan` boolean is being passed through correctly.

- [ ] **Step 3: No commit — this task records verification only**

Report PASS for both steps. No git activity in this task.

---

## Task 6: Final Clean Regression

**Files:** No code changes — clean repeat of Task 4 Step 5 to confirm reproducibility.

The Task 4 candidate snapshot was captured immediately after the launch refactor. This task confirms a fresh run still produces byte-identical results — guards against environmental flake (e.g. ros2 daemon held stale state from with_fan testing in Task 5).

- [ ] **Step 1: Discard intermediate candidate**

```bash
rm -rf /tmp/oculus_regression/candidate
```

- [ ] **Step 2: Stop daemon, recapture, compare**

```bash
source /opt/ros/humble/setup.bash
source /workspace/ros2_ws/install/setup.bash
ros2 daemon stop 2>&1 | tail -1
cd /workspace/ros2_ws/src/sensor_packages
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh candidate 2>&1 | tail -10
bash oculus_ros2/oculus_sonar/scripts/regression_oculus.sh compare
```

Expected last line: `PASS: structural snapshot identical to baseline`.

If FAIL on this clean repeat after Task 4 PASSED, the with_fan testing in Task 5 left residual state in the workspace (unlikely — ROS2 nodes are cleaned by SIGINT). Check:
- `pkill -f oculus` — any straggler processes?
- `ros2 daemon stop && sleep 2` — give DDS time to flush.
- Try once more — first attempt after ROS_DOMAIN_ID switch occasionally races.

---

## Task 7: CHANGELOG + PR

**Files:**
- Modify: `oculus_ros2/oculus_sonar/CHANGELOG.md` (prepend new `[Unreleased]` section above the existing B-1 entry)
- Create: PR via `gh` CLI

- [ ] **Step 1: Read current CHANGELOG to confirm insertion point**

```bash
sed -n '1,15p' /workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md
```

Expected: starts with the title `# Changelog`, then the metadata header, then the B-1 `[Unreleased]` block. The new B-2 entry goes ABOVE that B-1 block, IMMEDIATELY after the header lines.

- [ ] **Step 2: Prepend B-2 entry**

In `/workspace/ros2_ws/src/sensor_packages/oculus_ros2/oculus_sonar/CHANGELOG.md`, insert the following block ABOVE the existing `## [Unreleased] — Phase B-1: ...` line (i.e. between line 6 blank line and line 7's B-1 heading):

```markdown
## [Unreleased] — Phase B-2: Launch helper extraction (refactor)

> Master design: `docs/superpowers/specs/2026-05-03-oculus-sonar-refactor-design.md` §8 B-2
> Plan: `docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-2-launch-helper.md`
> Verification: structural snapshot regression PASS + functional --show-args + with_fan path

### Changed
- `launch/oculus.launch.py` 323 LOC → ~110 LOC. `launch_setup` body delegates container assembly to `_oculus_common.make_sonar_container`. Override-collection logic moved to two file-private helpers (`_collect_driver_overrides`, `_collect_fan_imager_overrides`) since they need `LaunchConfiguration` access.
- 90-line module docstring → 5-line pointer to README. 46-line trailing comment block deleted (duplicate of Phase A README content).

### Added
- `launch/_oculus_common.py` (≤90 LOC) — 3 pure helpers (no LaunchConfiguration access, unit-testable):
  - `sonar_model_to_config_path(model: str) -> str` — config YAML resolution + model whitelist FileNotFoundError
  - `topic_namespace(model: str) -> str` — `/sensor/sonar/oculus/{model}` prefix
  - `make_sonar_container(model, with_fan, driver_overrides, fan_imager_overrides) -> ComposableNodeContainer`

### Verification
- colcon build PASS (Release)
- **Layer 1 — structural snapshot**: baseline (`adc79cc` post-B-1 main HEAD) vs candidate (B-2 final) — 4 snapshot files byte-identical
  - `ros2 component types` → unchanged (still 2 oculus_sonar components)
  - `ros2 param dump /oculus/m750d` → all parameter defaults unchanged
  - `ros2 topic list` + `topic info --verbose` → all topic names · message types · QoS unchanged
- **Layer 2 — functional sanity**: `ros2 launch --show-args` returns 12 args (unchanged); `with_fan:=true` actual launch runs 5s without crash and publishes both `/sensor/sonar/oculus/m750d/sonar` AND `/sensor/sonar/oculus/m750d/fan_image`

### Notes
- Helper module location is INSIDE `oculus_sonar/launch/` (not a cross-package `sensor_packages/launch_utils/`). When ping360_sonar refactor introduces a second consumer with overlapping helper needs, that's the trigger to revisit cross-package extraction. Until then, the underscore-prefix internal-only convention keeps coupling minimal.
- `_collect_driver_overrides` and `_collect_fan_imager_overrides` deliberately stay in the launch file (not in `_oculus_common.py`) because they call `LaunchConfiguration(...).perform(context)` — keeping them in the file means `_oculus_common` remains pure and unit-testable without a launch context.
- Phase B-1's `scripts/regression_oculus.sh` reused unchanged. Per spec §10 the regression infra cost amortizes across B-1, B-2, and C.
```

- [ ] **Step 3: Commit CHANGELOG**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git add oculus_ros2/oculus_sonar/CHANGELOG.md
git commit -m "docs(oculus_sonar): add CHANGELOG entry for Phase B-2"
git log --oneline main..HEAD
```

Expected: 3 commits ahead of main (helper add, launch refactor, CHANGELOG).

- [ ] **Step 4: STOP — Push gate**

> ⚠️ Push requires explicit user approval per project policy (`git-workflow.md`: "푸시는 명시적 지시가 있을 때만"). DO NOT run `git push` without confirming with the user first. Output a status summary and wait.

Status to report to user:
```
Phase B-2 ready to push:
- Branch: refactor/phase-b2-launch-helper (3 commits)
- Build: PASS, Structural snapshot: PASS, Functional verify: PASS
- LOC change: oculus.launch.py 323 → ~110 (-66%), new _oculus_common.py ~90 LOC
- Net file delta: 8 LOC added (helper-add larger than launch-shrink? No: -213 + 90 = -123 net)
Push approval requested.
```

- [ ] **Step 5: Push branch (after user approval)**

```bash
cd /workspace/ros2_ws/src/sensor_packages
git push -u origin refactor/phase-b2-launch-helper
```

Expected: `Branch 'refactor/phase-b2-launch-helper' set up to track 'origin/refactor/phase-b2-launch-helper'.`

- [ ] **Step 6: Create PR via gh CLI**

```bash
cd /workspace/ros2_ws/src/sensor_packages
gh pr create \
  --title "refactor(oculus_sonar): Phase B-2 — launch helper extraction" \
  --base main \
  --head refactor/phase-b2-launch-helper \
  --body "$(cat <<'EOF'
## Summary
- `oculus.launch.py` 323 LOC → ~110 LOC (-66%) by extracting 3 pure helpers to `launch/_oculus_common.py`.
- Removed 90-line module docstring + 46-line trailing comment block (duplicate of Phase A README).
- Override-collection logic stays in launch file (needs LaunchConfiguration); container assembly moves to helper.

## Changes
- `launch/_oculus_common.py` (NEW, ~90 LOC) — 3 helpers, no LaunchConfiguration access:
  - \`sonar_model_to_config_path(model)\` — config YAML resolution with model whitelist FileNotFoundError
  - \`topic_namespace(model)\` — `/sensor/sonar/oculus/{model}` prefix
  - \`make_sonar_container(model, with_fan, driver_overrides, fan_imager_overrides)\` — ComposableNodeContainer factory
- \`launch/oculus.launch.py\` — \`launch_setup\` body shrinks to: read context → build 2 override dicts → \`make_sonar_container(...)\`. Two file-private helpers (\`_collect_driver_overrides\`, \`_collect_fan_imager_overrides\`) handle the verbose per-arg conditional unwrap.

## Verification

### Layer 1 — structural snapshot (Tasks 4 + 6)
- [x] colcon build PASS (Release)
- [x] baseline (\`adc79cc\` post-B-1) vs candidate (B-2 final) — 4 snapshot files byte-identical
- [x] component types unchanged
- [x] param defaults unchanged
- [x] topic list + per-topic QoS unchanged

### Layer 2 — functional sanity (Task 5)
- [x] \`ros2 launch --show-args\` → 12 declared args (unchanged from main)
- [x] \`with_fan:=true\` launches without crash and publishes both \`/sensor/sonar/oculus/m750d/sonar\` and \`/sensor/sonar/oculus/m750d/fan_image\`

## Notes
- Helper module location is \`oculus_sonar/launch/_oculus_common.py\` (INTERNAL). Cross-package shared \`sensor_packages/launch_utils/\` deferred until ping360_sonar refactor establishes a second consumer.
- Phase B-1's \`scripts/regression_oculus.sh\` reused unchanged.

## Next Phase
- C: \`pingToSonarImage\` zero-copy + parameter echo throttle + latched QoS (\`refactor/phase-c-hotpath-perf\`). Algorithmic side-effects → measurement-required gate.
EOF
)"
```

Expected: prints PR URL.

---

## Self-Review Checklist (executed inline before finalizing)

1. **Spec coverage check**: spec §8 B-2 lists 4 deliverables — (a) `_oculus_common.py` with 3 functions ✅ Task 3, (b) `oculus.launch.py` slim ✅ Task 4, (c) launch + topic verification ✅ Tasks 5 + 6, (d) "Phase A와 동일" Python-launch low-risk verification ✅ structural + functional layers.
2. **Placeholder scan**: No "TBD", "TODO", "implement later", "fill in details", "Add appropriate error handling". Every code block is complete.
3. **Type/name consistency**:
   - `sonar_model_to_config_path` used in Task 3 (definition) and Task 4 implicitly via `make_sonar_container`. Same name, same signature.
   - `topic_namespace` defined Task 3, used Task 4 (`_collect_fan_imager_overrides`). Same signature.
   - `make_sonar_container` defined Task 3 with kwargs `model, with_fan, driver_overrides, fan_imager_overrides, *, container_name`. Called Task 4 with all 4 positional kwargs. Match.
   - `_collect_driver_overrides` / `_collect_fan_imager_overrides` defined and used only in Task 4 launch file body. Match.
4. **Path correctness**: All paths absolute under `/workspace/ros2_ws/src/sensor_packages/`. Spec confirms.

---

## Risks & Recoveries

| Risk | Likelihood | Recovery |
|------|------------|----------|
| `_oculus_common.py` not installed by colcon | Low (existing `install(DIRECTORY launch ...)` block picks up new files) | `colcon build --packages-select oculus_sonar --cmake-clean-cache` once. |
| `from _oculus_common import` fails at launch time | Low | The `sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))` in oculus.launch.py covers ros2 launch's execution context. If still failing, replace with absolute `from ament_index_python.packages import get_package_share_directory; sys.path.insert(0, os.path.join(get_package_share_directory('oculus_sonar'), 'launch'))`. |
| Structural snapshot drift caused by overrides dict ordering | Low (Python 3.7+ dicts preserve insertion order, ros2 dump is deterministic) | If `02_param_dump.yaml` differs only in key order, the regression script's plain `diff` will catch it. Resolution: ensure `_collect_driver_overrides` builds keys in the same order as the original launch file (str → int → float). |
| Newer ros2 distro changes `--show-args` output format | Low (verified humble/jazzy both quote names as `'name'`) | The Task 5 Step 1 grep pattern `'${arg}'` is format-agnostic. If a future ros2 release changes quoting, switch to: `ros2 launch ... --show-args 2>&1 \| grep -oE "'[a-z_]+'" \| sort -u \| wc -l` and compare to 12. |
| with_fan launch fails because fan_imager_overrides dict differs from original | Medium | The original built the dict on lines 174-198 of pre-refactor launch. Task 4 `_collect_fan_imager_overrides` must produce the IDENTICAL keys/values. If structural snapshot Task 6 PASSES (default config doesn't trigger with_fan), but Task 5 with_fan launch FAILS, the diff is in the with_fan-only override keys — re-inspect `apply_colormap`/`colormap`/`qos_reliability`/`freq_mode` handling. |

---

## Execution Notes

- **Push gate is hard**: Task 7 Step 4 requires user approval. Do not auto-progress past Step 4.
- **Build step is required after every launch file edit**: Edits to `src/.../launch/*.py` don't take effect until `colcon build` re-installs to `share/`. Skipping this leads to phantom snapshot diffs (you're testing the OLD installed file against the NEW source).
- **`ros2 daemon stop` before EVERY snapshot capture**: The B-1 implementer hit a node-discovery race when the daemon held stale state from a prior run. Selecting `ROS_DOMAIN_ID=77` for snapshots and `=78` for with_fan testing isolates them.
- **One agent per task**: Per `subagent-driven-development`, dispatch a fresh subagent for each Task 3, 4, 5, 6, 7. Tasks 1 + 2 are environment setup — controller can do them directly without a subagent. Task 5 + 6 are verification-only and can also be done by controller.

---

## Plan complete and saved to `/workspace/ros2_ws/src/sensor_packages/docs/superpowers/plans/2026-05-04-oculus-sonar-phase-b-2-launch-helper.md`.

Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using `executing-plans`, batch execution with checkpoints.

Which approach?
