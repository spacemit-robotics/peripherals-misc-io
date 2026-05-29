#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/misc_io/${SROBOTIS_TEST_NAME:-misc-io-gpio-hardware-smoke}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
log_file="$log_dir/misc_io_gpio_hardware_smoke.log"

timeout_s="${MISC_IO_HW_SMOKE_TIMEOUT_S:-30}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] timeout_s=$timeout_s"

    cmake -S "$module_root" -B "$build_dir" -DBUILD_TESTS=ON
    cmake --build "$build_dir" --target test_misc_io -j"$(nproc)"
    echo "[info] waiting for a misc_io callback event"
    set +e
    LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}" \
        stdbuf -oL -eL "$build_dir/test_misc_io" &
    test_pid=$!
    seen=0
    for _ in $(seq 1 "$timeout_s"); do
        if grep -q "\[cb\]" "$log_file"; then
            seen=1
            break
        fi
        if ! kill -0 "$test_pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    if kill -0 "$test_pid" 2>/dev/null; then
        kill "$test_pid" 2>/dev/null
        wait "$test_pid" 2>/dev/null
    else
        wait "$test_pid" 2>/dev/null
    fi
    set -e
    if [ "$seen" -ne 1 ]; then
        echo "[error] no misc_io callback event observed within ${timeout_s}s"
        exit 1
    fi
} 2>&1 | tee "$log_file"

grep -q "\\[cb\\]" "$log_file"
