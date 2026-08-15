#!/usr/bin/env bash
# WSL 构建与测试脚本：以共享库方式构建 libeeg_alg.so 并运行全部测试。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build_wsl"
NPROC="$(nproc)"

echo "==> Configure (BUILD_SHARED_LIBS=ON)"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON

echo "==> Build (${NPROC} jobs)"
cmake --build "${BUILD_DIR}" -j "${NPROC}"

echo "==> Shared library artifact"
ls -l "${BUILD_DIR}/libeeg_alg.so"
file "${BUILD_DIR}/libeeg_alg.so"

echo "==> Exported dynamic symbols (must include public API)"
nm -D -C --defined-only "${BUILD_DIR}/libeeg_alg.so" | \
    grep -E 'eeg_alg_abi_version|dsp::welch_psd|dsp::band_power|absolute_power::compute|sham_power::compute|phase_locking::compute|pac_asymmetry::compute'

echo "==> Run tests"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
