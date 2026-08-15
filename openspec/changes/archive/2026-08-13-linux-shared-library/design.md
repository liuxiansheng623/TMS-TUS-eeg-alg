## Context

当前 `eeg_alg` 以 STATIC 静态库构建，仅能随宿主程序静态链接，无法作为独立 `.so` 部署到 Jetson/Linux 运行时。本 change 是"双平台交付"的第一阶段，目标是产出可被外部 C++ 应用动态链接、且导出符号可控的 `libeeg_alg.so`，并在 Ubuntu-22.04 WSL 中验证。Windows DLL / C# P/Invoke 属第二阶段，不在本 change 内实现，但导出机制会为其预留。

## Goals / Non-Goals

**Goals:**
- 通过 CMake `BUILD_SHARED_LIBS` 开关产出 `libeeg_alg.so`（默认仍为静态库，保持向后兼容）。
- 引入跨平台导出宏 `EEG_ALG_API`，并标注全部公开 API。
- 以隐藏可见性 + 显式导出控制动态符号表，得到干净 ABI。
- 在 WSL 中完成构建与全量测试。

**Non-Goals:**
- 不实现完整 C API 门面与 C# P/Invoke（仅埋一个 `extern "C"` 版本符号作为种子）。
- 不实现 Windows DLL 构建（后续 change）。
- 不引入 SONAME/ABI 版本化与符号版本脚本（后续部署时再做）。

## Decisions

### Decision 1: 用 `BUILD_SHARED_LIBS` 而非独立双目标

将 `add_library(eeg_alg STATIC ...)` 改为 `add_library(eeg_alg ...)`，并引入 `option(BUILD_SHARED_LIBS "..." OFF)`。这样 `-DBUILD_SHARED_LIBS=ON` 产出 `.so`，默认产出 `.a`。

- 备选：同时维护 `eeg_alg_static` 与 `eeg_alg_shared` 两个目标。被否：目标重复、编译选项与链接库需维护两份，收益低。
- 理由：遵循 CMake 惯用法，单一目标、单一事实来源。

### Decision 2: 跨平台导出宏头 `include/eeg_alg_export.h`

定义 `EEG_ALG_API`：Linux/GCC 展开为 `__attribute__((visibility("default")))`；Windows/MSVC 展开为 `__declspec(dllexport)`（构建库时，由 `EEG_ALG_BUILDING_LIBRARY` 区分）或 `__declspec(dllimport)`（消费库时）。

- 理由：一次标注即可同时覆盖 Linux（当前）与 Windows（第二阶段），避免第二阶段重复改造头文件。

### Decision 3: 隐藏可见性 + 显式导出

对共享库设置 `CXX_VISIBILITY_PRESET hidden` 与 `VISIBILITY_INLINES_HIDDEN YES`，仅导出标注 `EEG_ALG_API` 的符号。

- 备选：默认可见性（导出全部符号）。被否：会泄漏内部实现符号，且与后续 Windows DLL 导出策略不一致。

### Decision 4: 埋一个 `extern "C"` 版本符号

新增 `eeg_alg_abi_version()`（`extern "C"`）作为稳定 C ABI 种子：让 `dlopen`/`dlsym` 冒烟测试无需依赖 C++ 名称修饰即可确定性解析，同时为第二阶段的 C# P/Invoke 预留入口。

### Decision 5: 测试策略

- 复用 `tests/test_algorithms.cpp`：开启共享库后它自然动态链接 `libeeg_alg.so`，即验证"库可加载、算法结果正确"。
- 新增 `tests/test_shared_library.cpp`：`dlopen` + `dlsym` 解析 `eeg_alg_abi_version` 并校验返回值。
- 新增 `scripts/build_and_test_wsl.sh`：配置 `BUILD_SHARED_LIBS=ON` → 构建 → `ctest`，并用 `nm -D` 校验只导出公共符号。

## Risks / Trade-offs

- [C++ 名称修饰导致 dlsym 不稳定] → 用 `extern "C"` 版本符号规避；C++ 消费者仍按常规方式链接。
- [隐藏可见性误伤公共符号] → 显式 `EEG_ALG_API` 标注 + 测试脚本 `nm -D` 校验兜底。
- [公开头文件中的 Eigen 模板依赖消费者也需 Eigen] → 现状已如此，ABI 依赖 Eigen 版本，本 change 不改动，仅记录。
- [消费者需链接 FFTW3/OpenMP] → 由 CMake `PUBLIC`/`INTERFACE` 链接依赖透传，不要求消费者手工指定。

## Migration Plan

无数据迁移。构建方式变更：默认 `BUILD_SHARED_LIBS=OFF` 行为不变；需要 `.so` 时以 `-DBUILD_SHARED_LIBS=ON` 构建。回滚：关闭该选项即回到静态库。

## Open Questions

- Jetson 部署是否需要 SONAME / 符号版本脚本（如 `libeeg_alg.so.1`）？留待部署阶段决策。
