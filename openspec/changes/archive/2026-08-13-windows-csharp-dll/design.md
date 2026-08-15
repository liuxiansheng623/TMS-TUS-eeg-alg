## Context

第一阶段已产出 Linux 共享库 `libeeg_alg.so` 与跨平台导出宏 `EEG_ALG_API`。第二阶段需将同一算法库交付为 Windows DLL，供 C# 通过 P/Invoke 调用。当前 Windows 侧具备 MSVC（VS 2026 Community）、CMake 与 .NET 10，但 Eigen3/FFTW3/GTest 未安装。

## Goals / Non-Goals

**Goals:**
- 提供稳定、C 兼容的 ABI 接口（`extern "C"` + 原始 `double*`），使 C# 无需了解 Eigen/`std::vector`。
- 用 MSVC 构建 `eeg_alg.dll` 并导出 C 符号。
- 提供 C# P/Invoke 测试，验证跨语言编组与结果正确性。

**Non-Goals:**
- 不实现 .NET COM 互操作、NativeAOT 或 NuGet 打包（后续再做）。
- 不改动现有算法行为与 Linux 构建。

## Decisions

### Decision 1: C ABI 而非 C++ ABI

C# P/Invoke 只能按未修饰符号名解析，而 C++ 名称修饰随编译器/版本变化。因此以 `extern "C"` 包装全部公开算法，用原始数组而非 `Eigen::MatrixXd`/`std::vector` 传参。

### Decision 2: 错误码而非异常

C# 无法捕获 C++ 异常；跨 ABI 边界统一返回 `int` 错误码（`0` 成功）。非法参数（如 `absolute_power` 的 `fs < 600`）映射为错误码。

### Decision 3: 数据布局约定

- 输入：sample-major，`data[sample * num_channels + channel]`。
- `phase_locking` 输入：trial-major 扁平数组，`epochs[trial * num_samples * num_channels + sample * num_channels + channel]`。
- 输出：feature-major，`features_out[feature * num_channels + channel]`，由调用方预分配。

### Decision 4: 调用约定 `__cdecl`

导出函数使用默认 `__cdecl`；C# 侧显式 `CallingConvention = CallingConvention.Cdecl`（x64 下仅一种约定，但显式声明保证 x86 下一致）。

### Decision 5: 工具链 MSVC + vcpkg

选 MSVC 而非 MinGW：C#/.NET 在 Windows 上的 ABI 与 CRT 约定以 MSVC 为准，DLL 互操作更干净。依赖经 vcpkg（GitHub 仓库）安装 Eigen3、FFTW3、GTest。

### Decision 6: FFTW 以动态库随附

`eeg_alg.dll` 依赖 `libfftw3-3.dll`，测试时需将其置于可解析路径；是否静态链接 FFTW 留作部署期决策。

## Risks / Trade-offs

- [C# 数组编组错误导致越界/结果错] → 统一约定 sample-major/feature-major 并用 C# 测试断言结果与 C++ 参考一致。
- [FFTW DLL 缺失导致加载失败] → 测试脚本把 FFTW DLL 复制到输出目录。
- [x86/x64 不一致] → 明确目标 x64。
- [vcpkg 需联网下载与编译] → 依赖安装作为独立任务，失败即暂停并报告。

## Migration Plan

纯增量变更：新增 C ABI 门面与 C# 测试，不改动现有算法与 Linux 构建。回滚即删除新增文件。

## Open Questions

- FFTW 最终是动态随附还是静态链接进 `eeg_alg.dll`（部署期决定）。
