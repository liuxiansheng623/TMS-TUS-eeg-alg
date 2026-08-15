## ADDED Requirements

### Requirement: Linux 共享库构建目标

构建系统 SHALL 支持在 Linux 上产出共享库 `libeeg_alg.so`：当 CMake 选项 `BUILD_SHARED_LIBS=ON` 时，`eeg_alg` 目标 SHALL 构建为共享库；默认（`BUILD_SHARED_LIBS=OFF`）时仍构建为静态库 `libeeg_alg.a`。

#### Scenario: 开启共享库构建
- **WHEN** 以 `cmake -DBUILD_SHARED_LIBS=ON ...` 配置并构建
- **THEN** 产出 `libeeg_alg.so`

#### Scenario: 默认静态库构建
- **WHEN** 未指定 `BUILD_SHARED_LIBS` 配置并构建
- **THEN** 产出 `libeeg_alg.a`，且现有测试可链接通过

### Requirement: 跨平台导出宏

系统 SHALL 提供头文件 `include/eeg_alg_export.h`，其中定义宏 `EEG_ALG_API`：在 Linux/GCC 上展开为 `__attribute__((visibility("default")))`；在 Windows/MSVC 上展开为 `__declspec(dllexport)`（构建库时）或 `__declspec(dllimport)`（消费库时）。

#### Scenario: Linux/GCC 导出语义
- **WHEN** 在 Linux/GCC 下编译
- **THEN** `EEG_ALG_API` 等价于 `__attribute__((visibility("default")))`

#### Scenario: Windows 导出语义（预留）
- **WHEN** 在 Windows 下编译且定义了构建库宏
- **THEN** `EEG_ALG_API` 等价于 `__declspec(dllexport)`
- **AND** 消费库时等价于 `__declspec(dllimport)`

### Requirement: 公共 API 符号标注

所有公开自由函数 SHALL 使用 `EEG_ALG_API` 标注，包括 `dsp` 命名空间下的全部工具函数，以及 `absolute_power::compute` / `absolute_power::compute_indiv`、`sham_power::compute`、`phase_locking::compute`、`pac_asymmetry::compute` 与 `pac_asymmetry::detail::tort_mi_from_phase_amplitude`。

#### Scenario: 公开函数全部可导出
- **WHEN** 构建共享库后检查导出符号表
- **THEN** 上述每个公开函数均出现在 `libeeg_alg.so` 的导出符号中

### Requirement: 受控符号可见性

共享库 SHALL 以 `-fvisibility=hidden` 编译，并仅导出经 `EEG_ALG_API` 标注的符号；未标注的内部符号 SHALL 不出现在动态符号表中。

#### Scenario: 内部符号不可见
- **WHEN** 构建共享库后执行 `nm -D --defined-only libeeg_alg.so`
- **THEN** 动态导出符号仅包含标注过的公共 API，不含实现内部符号

### Requirement: 动态加载与外部调用

共享库 SHALL 可被外部进程通过 `dlopen` 加载，并通过 `dlsym` 解析导出的 C++ 符号后正确调用；调用结果 SHALL 与静态链接时一致。

#### Scenario: dlopen/dlsym 冒烟测试
- **WHEN** 冒烟测试程序 `dlopen("libeeg_alg.so")` 并 `dlsym` 稳定的 C ABI 入口 `eeg_alg_abi_version` 后调用
- **THEN** 加载成功、符号可解析，且返回非空版本字符串

### Requirement: WSL 可构建与可测试

在 Ubuntu-22.04 WSL 环境中，依赖 Eigen3、FFTW3、OpenMP、GoogleTest SHALL 可完成共享库构建，测试套件 SHALL 构建并全部通过。

#### Scenario: WSL 全量测试
- **WHEN** 在 Ubuntu-22.04 WSL 中以 `BUILD_SHARED_LIBS=ON` 配置、构建并运行 `ctest`
- **THEN** 所有测试（含新增共享库冒烟测试）通过
