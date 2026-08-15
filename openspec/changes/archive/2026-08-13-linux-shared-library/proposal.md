## Why

当前 `eeg_alg` 仅以静态库（STATIC）形式构建，无法作为独立动态库部署到 Jetson/Linux 运行时环境供外部进程/组件复用。这是"双平台交付"的第一阶段：先产出 Linux 共享库（`libeeg_alg.so`），后续再交付 Windows DLL 供 C# 通过 P/Invoke 调用。

## What Changes

- 新增共享库构建目标：通过 CMake `BUILD_SHARED_LIBS` 选项产出 `libeeg_alg.so`（默认仍可回退为静态库，保持向后兼容）。
- 新增跨平台导出宏头 `include/eeg_alg_export.h`（`EEG_ALG_API`）：Linux/GCC 使用 `__attribute__((visibility("default")))`，Windows 预留 `__declspec(dllexport/dllimport)`。
- 为全部公开 API 函数标注导出宏，控制共享库导出符号。
- 新增共享库冒烟测试：验证 `.so` 可被加载、导出符号可解析、算法可被外部调用。
- 新增 WSL 构建/测试脚本，并在 Ubuntu-22.04 WSL 中验证。

## Capabilities

### New Capabilities
- `shared-library`: 将算法库构建为 Linux 共享库，并导出稳定、可被外部调用的公共符号。

### Modified Capabilities

（无。本 change 不改变现有 5 个算法能力的规格行为，仅改变构建与导出方式。）

## Impact

- `CMakeLists.txt`：新增 `BUILD_SHARED_LIBS` 选项与共享库构建目标。
- `include/eeg_alg_export.h`（新增）：导出宏定义。
- `include/dsp_utils.h`、`include/absolute_power_alg.h`、`include/sham_power_alg.h`、`include/phase_locking_alg.h`、`include/pac_asymmetry_alg.h`：公开函数标注 `EEG_ALG_API`。
- `tests/`：新增共享库冒烟测试。
- 新增 WSL 构建/测试脚本。
- 依赖：Eigen3、FFTW3、OpenMP、GoogleTest（WSL 已具备）。
