## Why

完成"双平台交付"第二阶段。算法库需以 Windows DLL 形式交付给 C# 应用（通过 P/Invoke 调用）。当前仅有 Linux 共享库与一个 `extern "C"` 版本种子，缺少可供 C# 调用的、C 兼容的算法接口与 Windows 构建。

## What Changes

- 扩展 C ABI 门面 `eeg_alg_capi.h/.cpp`：为各算法提供 C 兼容包装（`extern "C"` + `EEG_ALG_API` + 原始 `double*` 数组），不暴露 Eigen/`std::vector`，返回错误码而非抛异常。
- 支持 Windows DLL 构建：CMake 增加跨平台依赖查找（Windows 下用 vcpkg 提供 Eigen3/FFTW3/GTest）。
- 新增 C# P/Invoke 测试项目，验证跨语言调用与数据编组。
- 在 Windows 上构建 `eeg_alg.dll` 并运行 C# 测试。

## Capabilities

### New Capabilities
- `c-api`: 为算法库提供稳定、可被 C# 等非 C++ 语言通过原始数组编组调用的 C ABI 接口。

### Modified Capabilities

（无。现有 5 个算法能力行为不变，仅新增 C ABI 门面与 Windows 构建。）

## Impact

- `include/eeg_alg_capi.h`、`src/eeg_alg_capi.cpp`：扩展 C ABI 包装函数。
- `CMakeLists.txt`：跨平台依赖查找（Windows/vcpkg 分支）。
- `tests/csharp/`（新增）：C# P/Invoke 测试项目。
- 依赖：Windows 上需 Eigen3、FFTW3、GTest（经 vcpkg 安装）。
