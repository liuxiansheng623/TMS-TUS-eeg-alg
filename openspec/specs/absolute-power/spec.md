# Absolute Power Specification

## Purpose

计算 EEG 信号的绝对功率（Absolute Power）：对每个通道做 Welch PSD 估计，然后按 13 个预定义频带（其中 4 个依赖 Alpha 峰值频率 APF）做梯形积分求带内功率，并将 APF 值一并存入输出。提供两种模式：`compute()` 使用全局统一 APF；`compute_indiv()` 逐通道用重心法（CoG）估计 APF。

源码文件映射：
- 输出结构体：`struct/absolute_power.h`
- 算法声明：`include/absolute_power_alg.h`
- 算法实现：`src/absolute_power_alg.cpp`
- DSP 工具函数：`include/dsp_utils.h`
- 测试：`tests/test_algorithms.cpp`

设计注记（informative）：枚举 `VVHI_FREQ` 命名中 "FREQ" 与其他波段名风格不一致；`VHI_GAMMA` 上界 70 Hz 与 `VVHI_FREQ` 下界 95 Hz 之间 70–95 Hz 无频带覆盖，属算法设计选择而非遗漏；`MUSCLE` [20,250] 为独立宽频特征，与 HI_BETA/LO_GAMMA/HI_GAMMA/VHI_GAMMA 重叠，非互斥分段。

## Requirements

### Requirement: 采样率前置校验

两个入口函数 `compute()` 和 `compute_indiv()` SHALL 在第一步调用 `validate_sampling_rate(fs)`：当 `!std::isfinite(fs)` 或 `fs < 2.0 × MAX_REQUIRED_FREQUENCY_HZ`（即 `fs < 600 Hz`）时，抛出 `std::invalid_argument`，消息为 `"absolute_power requires fs >= 600 Hz to compute the full 150-300 Hz band"`。此校验保证 Nyquist 频率 ≥ 300 Hz，从而覆盖最高频带 VVHI_FREQ 上界。`MAX_REQUIRED_FREQUENCY_HZ` 定义为 `300.0`（`src/absolute_power_alg.cpp:15`）。

#### Scenario: 拒绝 NaN 采样率

- **WHEN** 调用 `compute()` 或 `compute_indiv()` 且 `fs` 为 `NaN`
- **THEN** SHALL 抛出 `std::invalid_argument`

#### Scenario: 拒绝无穷大采样率

- **WHEN** 调用 `compute()` 或 `compute_indiv()` 且 `fs` 为 `+Inf` 或 `-Inf`
- **THEN** SHALL 抛出 `std::invalid_argument`

#### Scenario: 拒绝低于 600 Hz 的采样率

- **WHEN** 调用 `compute()` 或 `compute_indiv()` 且 `fs = 256`
- **THEN** SHALL 抛出 `std::invalid_argument`（`tests/test_algorithms.cpp:228-229`）

#### Scenario: 接受临界采样率 600 Hz

- **WHEN** 调用 `compute()` 或 `compute_indiv()` 且 `fs = 600`
- **THEN** SHALL 不抛出异常，正常执行计算（`tests/test_algorithms.cpp:230`）

---

### Requirement: 频带定义与特征枚举

系统 SHALL 定义 14 个特征索引（枚举 `Absolute_Power_Index`，`struct/absolute_power.h:19-37`），其中前 13 个为频带功率，第 14 个为 APF 值。`make_bands(apf)` SHALL 返回 13 个 `(特征索引, 下界Hz, 上界Hz)` 三元组（`src/absolute_power_alg.cpp:41-57`），频带边界如下：

- 固定频带：DELTA [1,4]、HI_BETA [20,30]、LO_GAMMA [30,45]、HI_GAMMA [45,70]、VHI_GAMMA [95,150]、VVHI_FREQ [150,300]、MUSCLE [20,250]、POWER_SUPPLY_50HZ [49,51]、POWER_SUPPLY_100HZ [99,101]
- APF 依赖频带：THETA [4, apf-2]、LO_ALPHA [apf-2, apf]、HI_ALPHA [apf, apf+2]、LO_BETA [apf+2, 20]

#### Scenario: 标准 APF 下的频带边界

- **WHEN** APF = 10.0 Hz
- **THEN** THETA 频带为 [4, 8]，LO_ALPHA 为 [8, 10]，HI_ALPHA 为 [10, 12]，LO_BETA 为 [12, 20]
- **AND** 固定频带边界不受 APF 影响

#### Scenario: 哨兵值 AP_FEATURE_COUNT

- **WHEN** 需要遍历全部特征或分配存储空间
- **THEN** SHALL 使用 `AP_FEATURE_COUNT`（值为 14）作为特征总数
- **AND** 枚举值依次为 `DELTA=0, THETA=1, LO_ALPHA=2, HI_ALPHA=3, LO_BETA=4, HI_BETA=5, LO_GAMMA=6, HI_GAMMA=7, VHI_GAMMA=8, VVHI_FREQ=9, MUSCLE=10, POWER_SUPPLY_50HZ=11, POWER_SUPPLY_100HZ=12, APF=13`

---

### Requirement: Welch PSD 估计

每通道 SHALL 调用 `dsp::welch_psd(x, fs, nperseg)` 进行功率谱密度估计（`src/absolute_power_alg.cpp:104,134`）。该方法先去均值和最小二乘线性趋势，使用汉宁窗，50% 重叠分段，零填充至下一个 2 的幂。返回 `(freqs, psd)` 对（`include/dsp_utils.h:31-42`）。

#### Scenario: 默认窗长

- **WHEN** 调用 `compute()` 或 `compute_indiv()` 且未指定 `nperseg`
- **THEN** SHALL 使用默认值 `nperseg = 256`

#### Scenario: 窗长大于信号长度

- **WHEN** 指定的 `nperseg` 大于信号采样点数
- **THEN** `dsp::auto_nperseg` SHALL 将 `nperseg` 夹紧到信号长度（`include/dsp_utils.h:117-123`）

---

### Requirement: 频带功率梯形积分

每个频带的功率 SHALL 通过 `dsp::band_power(freqs, psd, lo, hi)` 计算，该方法对 PSD 在 `[f_low, f_high]` 内做梯形积分，返回 V² 单位功率（`src/absolute_power_alg.cpp:72`，`include/dsp_utils.h:45-53`）。`fill_bands()` 遍历 `make_bands(apf)` 返回的全部 13 个频带，逐一调用 `band_power` 并写入 `result.features[idx][ch]`（`src/absolute_power_alg.cpp:71-72`）。

#### Scenario: 频带功率与理论值匹配

- **WHEN** 输入为已知振幅 A 的正弦波（频率落在某频带内）
- **THEN** 该频带的积分功率 SHALL 近似等于 A²/2，误差在 ±25% 以内（`tests/test_algorithms.cpp` BandPowerMatchesTheory）

#### Scenario: 空区间频带

- **WHEN** APF 参数使频带边界反转（例如 `apf = 3` 导致 THETA 上界 = 1 < 下界 = 4）
- **THEN** `band_power` 因 `f_high <= f_low` SHALL 返回 `0.0`，该频带功率记为 0（行为由 dsp-foundation 规格「频带梯形积分功率」钉住）
- **AND** `absolute_power` 层不做额外边界防御（`src/absolute_power_alg.cpp:41-57` 无边界校验）

---

### Requirement: compute() 全局 APF 模式

`compute(data, fs, apf, nperseg)` SHALL 不做 APF 估计，将传入的 `apf` 参数原样写入每个通道的 `features[APF][ch]`（`src/absolute_power_alg.cpp:74,105`）。`apf` 参数无范围校验（仅注释建议 [8,13]，`include/absolute_power_alg.h:19`）。所有通道共用同一个 APF 值计算频带边界。

#### Scenario: APF 值直接存储

- **WHEN** 调用 `compute()` 且传入 `apf = 10.0`
- **THEN** 所有通道的 `get_apf(ch)` SHALL 返回 `10.0`

#### Scenario: APF 参数超出建议范围

- **WHEN** 调用 `compute()` 且传入 `apf = 3.0`（超出建议范围 [8,13]）
- **THEN** SHALL 不做校验，直接使用；THETA 上界变为 1.0 < 下界 4.0，`band_power` SHALL 返回 `0.0`（见「空区间频带」场景）

---

### Requirement: compute_indiv() 逐通道 CoG APF 估计

`compute_indiv(data, fs, nperseg)` SHALL 对每个通道独立估计 APF（`src/absolute_power_alg.cpp:136-139`）：

1. 在 Welch PSD 之后，调用 `dsp::compute_apf_cog(freqs, psd, 8.0, 13.0)` 用重心法估计 APF：`APF = Σ(f·PSD) / Σ(PSD)`，积分区间 [8, 13] Hz（`include/dsp_utils.h:107`）
2. 若返回 `NaN`（频带内总功率 ≈ 0），回退为 `10.0`（`src/absolute_power_alg.cpp:137`）
3. 最终用 `std::clamp(apf, 8.0, 13.0)` 夹紧到 [8.0, 13.0]（`src/absolute_power_alg.cpp:138`）

#### Scenario: CoG 正常估计

- **WHEN** 通道信号在 [8,13] Hz 内有显著功率峰值（如 9 Hz 正弦波）
- **THEN** CoG 估计的 APF SHALL 在真实峰值频率 ±0.7 Hz 以内（`tests/test_algorithms.cpp` CoGDetectsDifferentPeaks）
- **AND** 9 Hz 峰值 SHALL 落入 LO_ALPHA 频带（`tests/test_algorithms.cpp` AlphaPeakInLoAlpha）

#### Scenario: CoG 返回 NaN（全零 PSD）

- **WHEN** 通道在 [8,13] Hz 内总功率 ≈ 0（如全零信号）
- **THEN** `compute_apf_cog` 返回 `NaN`，系统 SHALL 将其替换为 `10.0` 并夹紧到 [8,13]（最终 APF = 10.0）（`src/absolute_power_alg.cpp:137-138`）

#### Scenario: CoG 返回超出 [8,13] 的值

- **WHEN** `compute_apf_cog` 返回超出 [8.0, 13.0] 范围的 APF 值
- **THEN** `std::clamp` SHALL 将其夹紧到边界值（8.0 或 13.0）（`src/absolute_power_alg.cpp:138`）

---

### Requirement: FFT Plan 预热

在进入并行循环之前，系统 SHALL 计算 `nfft = next_power_of_2(auto_nperseg(num_samples, nperseg))` 并调用 `dsp::warmup_r2c(nfft)` 预创建 FFTW plan（`src/absolute_power_alg.cpp:95-96,125-126`）。原因：FFTW planner 非线程安全，不能在 OpenMP 并行区内首次创建（`src/absolute_power_alg.cpp:94`）。

#### Scenario: 预热保证并行安全

- **WHEN** 多通道数据触发 OpenMP 并行执行
- **THEN** FFTW plan SHALL 已在并行区外创建完毕，并行区内各线程不会触发 plan 首次创建

---

### Requirement: OpenMP 并行执行阈值

并行维度为通道维（各通道写不同的 `result.features[*][ch]`，无数据竞争）。工作量 `work = num_channels × num_samples`（`long` 类型）。系统 SHALL 仅当 `work >= kOmpMinWork`（`1L << 17 = 131072`）时启用 `#pragma omp parallel for`；否则串行执行（`src/absolute_power_alg.cpp:18,100-101,130-131`）。

#### Scenario: 低于阈值串行执行

- **WHEN** `work < 131072`（例如 8 通道 × 1024 采样 = 8192）
- **THEN** SHALL 串行执行，不启用 OpenMP

#### Scenario: 达到阈值并行执行

- **WHEN** `work >= 131072`（例如 32 通道 × 8192 采样 = 262144）
- **THEN** SHALL 启用 OpenMP 并行
- **AND** 跨通道结果 SHALL 与串行结果一致，误差在 1e-9 以内（`tests/test_algorithms.cpp:176-190` ParallelPathCorrectness）

#### Scenario: 线程安全无数据竞争

- **WHEN** OpenMP 并行执行时
- **THEN** 各线程写不同的 `result.features[*][ch]` 列，SHALL 无数据竞争（`src/absolute_power_alg.cpp:98-99`）

---

### Requirement: 输出结构体零初始化

`EEG_Absolute_Power_SoA` 构造时 SHALL 将所有 `14 × ch_count` 个值初始化为 `0.0`（`struct/absolute_power.h:52-56`）。

#### Scenario: 新构造的结果容器

- **WHEN** 以 `ch_count = 8` 构造 `EEG_Absolute_Power_SoA`
- **THEN** 全部 `14 × 8 = 112` 个值 SHALL 为 `0.0`

---

### Requirement: 高频带不额外做 Nyquist 夹紧

系统 SHALL NOT 对频带边界做 `std::min(band_hi, fs/2)` 等 Nyquist 夹紧逻辑。正确性完全依赖 `fs >= 600` 前置校验：保证 Nyquist ≥ 300 Hz ≥ 最高频带上界（VVHI_FREQ 上界 300 Hz）（`src/absolute_power_alg.cpp:41-57,26-30`）。

#### Scenario: 频带边界直接使用

- **WHEN** `fs = 600`（最低合法采样率），Nyquist = 300 Hz
- **THEN** VVHI_FREQ [150, 300] 上界恰好等于 Nyquist，SHALL 正常积分无截断

---

### Requirement: 索引访问无越界检查

`EEG_Absolute_Power_SoA::at(feature_idx, ch_idx)` 和 `get_apf(ch_idx)` 无运行时越界检查；行为未定义（`struct/absolute_power.h:63-64,73-74`）。调用方 MUST 负责保证索引合法。

#### Scenario: 合法索引访问

- **WHEN** `feature_idx ∈ [0, 13]` 且 `ch_idx ∈ [0, num_channels-1]`
- **THEN** `at()` SHALL 返回对应位置的可写引用，`get_apf()` SHALL 返回 `features[APF][ch_idx]`

#### Scenario: 越界索引访问

- **WHEN** `feature_idx >= 14` 或 `ch_idx >= num_channels`
- **THEN** 行为未定义（无 `at()` 抛异常或边界断言）