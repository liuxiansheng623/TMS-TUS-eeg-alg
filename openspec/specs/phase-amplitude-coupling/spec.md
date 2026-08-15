# phase-amplitude-coupling Specification

## Purpose

逐通道量化 EEG 信号中"低频脑电波相位调节高频脑电波振幅"的耦合强度，采用 Tort 调制指数（MI ∈ [0,1]）。当前实现仅做逐通道 PAC 计算，不做左右半球不对称分析；结构体、枚举与命名空间中的 "Asymmetry" 字样为历史命名债，规格如实记录现状。核心源码位于 `src/pac_asymmetry_alg.cpp`，声明位于 `include/pac_asymmetry_alg.h`，输出结构体位于 `struct/PAC_asymmetery.h`。

## Requirements

### Requirement: Tort MI 核心算法

`tort_mi_from_phase_amplitude` SHALL 按以下公式链计算 Tort MI（∈ [0,1]）：（1）相位分箱 `b = floor((phase[i]+PI)/(2PI)*num_bins)`，夹紧到 `[0,num_bins-1]`；（2）每箱平均振幅 `bin_mean[j] = bin_sum[j]/bin_count[j]`；（3）归一化 `p[j] = bin_mean[j]/total`；（4）香农熵 `H = -Σ p*ln(p)`（仅 p>0）；（5）`MI = (ln(num_bins)-H)/ln(num_bins)`。源码 `src/pac_asymmetry_alg.cpp:22-61`。

#### Scenario: 平均振幅语义验证

- **WHEN** 各相位箱样本数不同但每箱平均振幅相同（如 4 箱分别有 1/2/3/4 个样本，振幅均为 2.0）
- **THEN** MI SHALL 等于 0.0（精度 1e-12），钉住"使用均值而非总和"语义
- **AND** 对应测试 `PacAsymmetry.TortMiUsesMeanAmplitudePerPhaseBin`

#### Scenario: 归一化概率分布计算

- **WHEN** 所有相位箱均有样本且 `total >= 1e-12`
- **THEN** `p[j]` SHALL 由 `bin_mean[j] / total` 计算，而非 `bin_sum[j] / total`

#### Scenario: 香农熵仅计入正概率

- **WHEN** 某箱的 `p[j] == 0`
- **THEN** 该箱 SHALL 不参与熵求和（`p * ln(p)` 项被跳过）

### Requirement: Tort MI 输入校验与边界返回

`tort_mi_from_phase_amplitude` MUST 在输入非法时返回 `NaN`，在 `total` 极小时返回 `0.0`，在相位越界时将箱索引夹紧。实现位置：`src/pac_asymmetry_alg.cpp:25-52`。

#### Scenario: 长度不匹配

- **WHEN** `phase.size() != amplitude.size()`
- **THEN** 返回值 SHALL 为 `NaN`

#### Scenario: 空输入

- **WHEN** `phase.size() == 0`
- **THEN** 返回值 SHALL 为 `NaN`

#### Scenario: num_bins 不足（底层函数）

- **WHEN** 传入 `tort_mi_from_phase_amplitude` 的 `num_bins < 2`
- **THEN** 返回值 SHALL 为 `NaN`

#### Scenario: 某相位箱无样本

- **WHEN** 存在 `bin_count[j] == 0` 的箱
- **THEN** 返回值 SHALL 为 `NaN`，表示相位覆盖不完整、无法可靠估计 MI

#### Scenario: total 极小

- **WHEN** `total < 1e-12`
- **THEN** 返回值 SHALL 为 `0.0`，避免除零

#### Scenario: 相位值越出标准区间

- **WHEN** `phase[i]` 超出 `[-π, π]` 区间
- **THEN** 箱索引 `b` SHALL 被夹紧到 `[0, num_bins-1]`，不产生越界访问

### Requirement: 8 个耦合模式定义

`pac_asymmetry` MUST 定义 8 个耦合模式（4 相位带 × 2 振幅带），由枚举 `PAC_Asymmetry_Index` 索引：DELTA(1–4 Hz)、THETA(4–(APF-2) Hz)、ALPHA((APF-2)–(APF+2) Hz)、BETA((APF+2)–20 Hz) 分别配对 LO_GAMMA(30–45 Hz) 和 HI_GAMMA(45–70 Hz)。常量 `PAC_FEATURE_COUNT = 8`。定义位置：`struct/PAC_asymmetery.h:21-31`，频带构造：`src/pac_asymmetry_alg.cpp:78-89`。

#### Scenario: 默认 APF=10 Hz 时的频带边界

- **WHEN** `apf = 10.0`
- **THEN** 四相位带 SHALL 分别为 DELTA(1–4)、THETA(4–8)、ALPHA(8–12)、BETA(12–20) Hz
- **AND** 两振幅带 SHALL 分别为 LO_GAMMA(30–45)、HI_GAMMA(45–70) Hz

#### Scenario: APF 影响 Theta/Alpha/Beta 边界

- **WHEN** `apf` 值改变
- **THEN** THETA 上界、ALPHA 上下界、BETA 下界 SHALL 随 `apf` 联动变化
- **AND** DELTA 频带与振幅频带 SHALL 保持不变

### Requirement: compute 逐通道 PAC 流程

`pac_asymmetry::compute(data, fs, apf, num_bins)` SHALL 对 `data`（采样点×通道）的每一通道，使用 `dsp::bandpass_analytic` 提取低频瞬时相位（`.first`）和高频瞬时振幅（`.second`），再调用 `tort_mi_from_phase_amplitude` 得到 MI，写入 `result.features[耦合模式索引][通道索引]`。`num_bins` 默认 18；若 `< 2` 自动调整为 2。实现位置：`src/pac_asymmetry_alg.cpp:125-150`。

#### Scenario: num_bins 默认值

- **WHEN** 调用 `compute(data, fs, apf)` 未传 `num_bins`
- **THEN** `num_bins` SHALL 取默认值 18（每箱 20°）

#### Scenario: num_bins 自动调整

- **WHEN** 传入 `compute()` 的 `num_bins < 2`
- **THEN** `num_bins` SHALL 被自动调整为 2（不返回 NaN）

#### Scenario: 带通滤波提取相位与振幅

- **WHEN** 对单通道信号计算某一耦合模式的 MI
- **THEN** 低频带通结果 SHALL 取 `.first`（瞬时相位），高频带通结果 SHALL 取 `.second`（瞬时振幅）
- **AND** 两者均来自 `dsp::bandpass_analytic` 的返回对

#### Scenario: 结果布局

- **WHEN** `compute()` 完成计算
- **THEN** `result.features[cidx][ch]` SHALL 存放第 `ch` 通道在第 `cidx` 耦合模式下的 MI 值
- **AND** 各通道写不同的列，无跨通道数据混写

### Requirement: 强 PAC 检测能力

系统 MUST 能够检测出合成信号中的真实相位-振幅耦合，MI 值显著高于无耦合基线。对应测试：`PacAsymmetry.StrongPacChannelHighMI`（`tests/test_algorithms.cpp:336-356`）。

#### Scenario: 合成 delta-gamma 调制信号

- **WHEN** 通道 0 包含 2 Hz delta 振荡 + 受 delta 相位调制的 40 Hz gamma 振幅，通道 1 包含相同 delta + gamma 但无调制
- **THEN** 通道 0 的 `DELTA_LO_GAMMA` MI SHALL 高于通道 1
- **AND** 两通道 MI 差值 SHALL 大于 0.02
- **AND** 通道 0 的 MI SHALL 大于 0.05

### Requirement: 噪声基线低 MI

白噪声输入 MUST 产生有限的、总体较低的 MI 值，所有 8 模式 MI ∈ [0,1]。对应测试：`PacAsymmetry.NoiseLowMI`（`tests/test_algorithms.cpp:359-373`）。

#### Scenario: 单通道白噪声

- **WHEN** 输入为 2560 点单通道白噪声（`fs=256, apf=10`）
- **THEN** 所有 8 个模式的 MI SHALL 为有限值且 ∈ [0,1]
- **AND** `DELTA_LO_GAMMA` 的 MI SHALL 小于 0.3

### Requirement: 输出维度与数值范围

`compute()` 返回的 `EEG_PAC_Asymmetry_SoA` MUST 具有正确的维度（`num_channels` 等于输入通道数，`features` 内层长度均等于通道数）且所有 MI ∈ [0,1]。对应测试：`PacAsymmetry.OutputDimsAndRange`（`tests/test_algorithms.cpp:376-390`）。

#### Scenario: 多通道白噪声输出校验

- **WHEN** 输入为 4 通道白噪声（1024 点，`fs=256, apf=10`）
- **THEN** `result.num_channels` SHALL 等于 4
- **AND** 所有 8 个 `features[c].size()` SHALL 等于 4
- **AND** 每个 MI 值 SHALL ∈ [0,1]

### Requirement: FFT 预热与 OpenMP 并行

`compute()` MUST 在并行循环前预热 FFT plan（`warmup_r2c(nfft)` + `warmup_c2c_backward(nfft)`，`nfft = next_power_of_2(num_samples)`），因为 FFTW planner 非线程安全。当 `num_channels × num_samples >= kOmpMinWork (131072)` 时启用 `#pragma omp parallel for`；否则串行执行。并行区内各通道写不同的 `result.features[*][ch]`，无共享写入，线程安全。实现位置：`src/pac_asymmetry_alg.cpp:68,134-142`。

#### Scenario: 小输入串行执行

- **WHEN** `num_channels × num_samples < 131072`
- **THEN** 外层通道循环 SHALL 串行执行（OpenMP `if` 条件为 false）

#### Scenario: 大输入并行执行

- **WHEN** `num_channels × num_samples >= 131072`
- **THEN** 外层通道循环 SHALL 使用 `#pragma omp parallel for` 并行
- **AND** 各通道 SHALL 仅写入自己的 `result.features[*][ch]`，保证线程安全

#### Scenario: FFT plan 预热

- **WHEN** `compute()` 被调用且 `num_samples > 0`
- **THEN** SHALL 在进入并行区前调用 `dsp::warmup_r2c(nfft)` 和 `dsp::warmup_c2c_backward(nfft)`

### Requirement: 空输入与无验证行为

`compute()` MUST 在 0 通道输入时返回空结果；对 `fs > 0`、`data` 行数、APF 合理性不做显式校验，前置条件仅在文档中声明。`at()` 不做越界检查，调用方负责保证索引合法。

#### Scenario: 0 通道输入

- **WHEN** `data` 为 0 列矩阵（`num_channels = 0`）
- **THEN** `compute()` SHALL 返回 `num_channels = 0` 的空结果，内层 for 循环不执行

#### Scenario: at() 越界访问

- **WHEN** 调用 `at(feature_idx, ch_idx)` 时索引超出合法范围
- **THEN** 行为 SHALL 为未定义（无边界检查，调用方负责保证索引合法）

#### Scenario: compute 无输入验证

- **WHEN** 传入 `fs <= 0`、空行矩阵或不合理的 APF
- **THEN** `compute()` SHALL 不做显式异常抛出，结果可能为 NaN 或未定义

### Requirement: 结果结构体初始化

`EEG_PAC_Asymmetry_SoA(ch_count)` MUST 将 `features` 的全部 8×`ch_count` 个 `double` 初始化为 `0.0`，并记录 `num_channels = ch_count`。定义位置：`struct/PAC_asymmetery.h:46-50`。

#### Scenario: 构造后全零

- **WHEN** 以 `ch_count = N` 构造 `EEG_PAC_Asymmetry_SoA`
- **THEN** `num_channels` SHALL 等于 `N`
- **AND** 所有 `features[i][j]`（`i ∈ [0,7]`，`j ∈ [0,N)`）SHALL 等于 `0.0`

### Requirement: 历史命名债标注

结构体 `EEG_PAC_Asymmetry_SoA`、枚举 `PAC_Asymmetry_Index`、命名空间 `pac_asymmetry`、头文件名 `PAC_asymmetery.h`（含拼写错误 'Asymmetery'）均保留 "Asymmetry" 字样。当前实现 MUST 仅做逐通道 PAC 计算，不包含任何左右半球不对称分析逻辑。规格与文档 SHALL 如实标注此命名债，避免后续维护者误解。

#### Scenario: 命名与实际行为一致

- **WHEN** 阅读或维护 PAC 相关代码
- **THEN** 开发者 SHALL 理解 "Asymmetry" 为历史遗留命名，当前功能仅为逐通道 PAC
- **AND** 任何新增的不对称分析功能 MUST 在实现后更新此规格