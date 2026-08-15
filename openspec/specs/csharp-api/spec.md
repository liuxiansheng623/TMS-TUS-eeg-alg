# csharp-api Specification

## Purpose

在 C ABI 之上提供类型安全的 C# 封装：定义特征/频带/耦合模式枚举与强类型结果，用 `IntPtr` 正确处理字符串编组、用 sample-major 展平处理矩阵编组，并把 C ABI 错误码映射为 `EegAlgException`，使 C# 应用无需接触原始指针与错误码即可调用算法库。

## Requirements

### Requirement: 覆盖全部 C ABI 入口

C# 封装 SHALL 提供与 8 个 C ABI 入口一一对应的公共方法：版本、`welch_psd`、`band_power`、`absolute_power`、`absolute_power_indiv`、`sham_power`、`phase_locking`、`pac`。

#### Scenario: 全入口可用
- **WHEN** 调用封装层的任意入口
- **THEN** 该入口成功调用对应 C ABI 函数，无缺失

### Requirement: 地道类型与结果类型

封装层 SHALL 定义特征/频带/耦合模式枚举（与 C++ 枚举索引一致），并为带特征输出的算法返回强类型结果（支持 `[feature, channel]` 索引与语义化访问）。

#### Scenario: 枚举索引与 C++ 一致
- **WHEN** 读取 `AbsolutePowerFeature.Apf` 的索引
- **THEN** 其值为 13，与 `absolute_power::APF` 一致

### Requirement: 错误码映射为异常

封装层 SHALL 将 C ABI 的非零返回码映射为 `EegAlgException`（含错误码与可读信息），不把原始错误码裸露给调用方。

#### Scenario: 非法采样率抛异常
- **WHEN** 以 `fs < 600` 调用 `AbsolutePower`
- **THEN** 抛出 `EegAlgException`，其 `ErrorCode` 为 `-2`

### Requirement: 正确字符串与数据编组

封装层 SHALL 用 `IntPtr` + `Marshal.PtrToStringUTF8` 处理 `const char*` 返回；输入矩阵 SHALL 展平为 sample-major `double[]`，输出 SHALL 按 feature-major 映射为强类型结果。

#### Scenario: 版本字符串正确读取
- **WHEN** 调用版本入口
- **THEN** 返回非空字符串且无堆损坏

### Requirement: 测试覆盖全部入口

测试项目 SHALL 覆盖全部 8 个入口，并校验数值结果（如 `band_power` 接近 `A²/2`、`absolute_power` 的 APF 回填）。

#### Scenario: 全量测试通过
- **WHEN** 在 Windows 构建并运行测试项目
- **THEN** 全部断言通过
