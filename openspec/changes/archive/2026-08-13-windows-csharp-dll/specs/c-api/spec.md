## ADDED Requirements

### Requirement: C ABI 稳定入口与错误码契约

系统 SHALL 提供 `eeg_alg_abi_version()` 返回库版本字符串；所有 C ABI 算法入口 SHALL 返回 `int` 错误码（`0` 表示成功，非 `0` 表示失败），不得跨越 ABI 边界抛出 C++ 异常。

#### Scenario: 版本入口可解析
- **WHEN** 通过 `dlsym`（Linux）或 `GetProcAddress`（Windows）解析 `eeg_alg_abi_version`
- **THEN** 符号可解析，调用返回非空版本字符串

#### Scenario: 非法输入返回错误码而非抛异常
- **WHEN** 传入非法采样率（如 `absolute_power` 的 `fs < 600`）
- **THEN** 返回非零错误码，调用方（C#）不会收到未捕获异常

### Requirement: 原始数组编组（不暴露 C++ 类型）

C ABI SHALL 使用原始 `double*`（或 `const double*`）与维度整数传递数据，不得在函数签名中暴露 `Eigen::MatrixXd`、`Eigen::VectorXd`、`std::vector` 或 SoA 结构体。

#### Scenario: 数据布局约定
- **WHEN** 调用 C ABI 算法入口
- **THEN** 输入数据为 sample-major（`data[sample * num_channels + channel]`），输出特征为 feature-major（`features_out[feature * num_channels + channel]`）

### Requirement: absolute_power C ABI 包装

系统 SHALL 提供 `int eeg_alg_absolute_power(const double* data, int num_samples, int num_channels, double fs, double apf, int nperseg, double* features_out)`，结果写入调用方预分配的 `14 × num_channels` 缓冲区；`apf` 特征等于传入的 `apf`。

#### Scenario: 与 C++ 结果一致
- **WHEN** 用同一组正弦信号分别调用 C ABI 与 `absolute_power::compute`
- **THEN** 两者 `14 × num_channels` 特征值在浮点精度内一致

### Requirement: 其余算法 C ABI 包装

系统 SHALL 提供 `eeg_alg_absolute_power_indiv`、`eeg_alg_sham_power`、`eeg_alg_phase_locking`、`eeg_alg_pac` 的 C ABI 包装，均以原始数组入参、调用方预分配输出；`phase_locking` 以 trial-major 的扁平数组 `epochs`（`num_trials × num_samples × num_channels`）输入。

#### Scenario: 各算法入口可导出并可调用
- **WHEN** 构建 DLL 后检查导出符号，并调用各入口
- **THEN** 每个入口均出现在导出符号中，返回 `0`（成功）

### Requirement: DSP 原语 C ABI 包装

系统 SHALL 提供 `eeg_alg_welch_psd`（输出 `freqs_out`/`psd_out`，长度为 `nfft/2 + 1`）与 `eeg_alg_band_power`（返回频带功率 `double`）的 C ABI 包装。

#### Scenario: welch_psd 与 band_power 跨语言可用
- **WHEN** C# 侧调用 `eeg_alg_welch_psd` 与 `eeg_alg_band_power` 处理正弦信号
- **THEN** 频带功率结果与理论值 `A²/2` 的误差在 ±25% 内

### Requirement: Windows DLL 导出

在 Windows/MSVC 下，C ABI 入口 SHALL 以 `__declspec(dllexport)` 导出（由 `EEG_ALG_API` 在定义 `EEG_ALG_BUILDING_LIBRARY` 时展开），使用 C 语言链接（`extern "C"`）与 `__cdecl` 调用约定。

#### Scenario: DLL 导出符号可被 P/Invoke 解析
- **WHEN** C# 用 `[DllImport("eeg_alg", CallingConvention = CallingConvention.Cdecl)]` 声明并调用 `eeg_alg_abi_version`
- **THEN** 解析成功并返回正确版本字符串

### Requirement: C# P/Invoke 测试

系统 SHALL 提供 C# 测试项目，通过 P/Invoke 调用 C ABI 入口，验证跨语言数据编组（`double[]` 与预分配输出缓冲区）与结果正确性。

#### Scenario: C# 端到端调用
- **WHEN** 在 Windows 上构建 `eeg_alg.dll` 并运行 C# 测试
- **THEN** 测试通过，`absolute_power`（或等价 DSP 原语）结果与 C++ 参考值一致
