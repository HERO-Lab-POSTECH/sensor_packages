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
#   SONAR_MODEL    : m3000d|m1200d|m750d (default: m750d)
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
