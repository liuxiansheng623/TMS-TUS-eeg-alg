# sham-power Specification

## Purpose

伪刺激功率（sham_power）能力用于 Sham 对照实验场景：以用户指定或内部随机生成的频率起点 `rdn_freq_pnt` 为基准，计算 EEG 各通道在 11 个随机频带及 50/100 Hz 工频带上的绝对功率，并将实际使用的随机起点回填到输出特征中，便于实验复现。源码文件映射：输出结构体定义于 `struct/sham_power.h`，算法声明于 `include/sham_power_alg.h`，算法实现于 `src/sham_power_alg.cpp`，底层 DSP 原语（`welch_psd`、`band_power`、`warmup_r2c` 等）由 `include/dsp_utils.h` / `src/dsp_utils.cpp` 提供。

## Requirements

### Requirement: Output Data Model

`sham_power` 命名空间 SHALL 提供枚举 `Sham_Feature_Index`（共 14 个值：`SHAM_BAND_0`(0) … `SHAM_BAND_10`(10)、`POWER_SUPPLY_50HZ`(11)、`POWER_SUPPLY_100HZ`(12)、`RDN_FREQ_PNT`(13)、`SHAM_FEATURE_COUNT`(14)）以及结构体 `Sham_Power_SoA`。`Sham_Power_SoA` SHALL 以构造参数 `ch_count` 初始化 `features`（`std::array<std::vector<double>, 14>`，每个 `vector` 长度 `ch_count`，初值 0.0）和 `num_channels`，并提供不做越界检查的可写访问方法 `double& at(size_t feature_idx, size_t ch_idx)`。定义见 `struct/sham_power.h`。

#### Scenario: 构造并访问 Sham 功率结果

- **WHEN** 以 `ch_count = 4` 构造 `Sham_Power_SoA`
- **THEN** `features` 数组包含 14 个 `vector`，每个长度 4，所有元素为 0.0
- **AND** `num_channels` 等于 4

#### Scenario: at() 方法不做越界检查

- **WHEN** 调用 `at(feature_idx, ch_idx)` 且索引合法
- **THEN** 返回 `features[feature_idx][ch_idx]` 的可写引用
- **AND** 调用方 MUST 自行保证两个索引均未越界（`at()` 无边界检查，见 `struct/sham_power.h:59`）

---

### Requirement: Sham Random Band Definitions

函数 `make_sham_bands(rdn)` SHALL 依据随机起点 `rdn` 生成 11 个随机频带：`SHAM_BAND_0` 的范围为 `[rdn + 0.0, rdn + 1.0]`（带宽 1 Hz）；`SHAM_BAND_k`（k = 1…10）的范围为 `[rdn + (4k − 3), rdn + (4k + 1)]`（各 4 Hz 宽）。完整边界表：BAND_1 = [rdn+1, rdn+5]，BAND_2 = [rdn+5, rdn+9]，…，BAND_10 = [rdn+37, rdn+41]。所有频带首尾相接、无重叠无间隙。实现见 `src/sham_power_alg.cpp:24-30`。

#### Scenario: BAND_0 频带边界

- **WHEN** `rdn = 20.0`
- **THEN** `SHAM_BAND_0` 范围为 [20.0, 21.0]，带宽 1 Hz

#### Scenario: BAND_1 至 BAND_10 频带边界

- **WHEN** `rdn = 20.0`
- **THEN** `SHAM_BAND_1` 范围为 [21.0, 25.0]，`SHAM_BAND_10` 范围为 [57.0, 61.0]
- **AND** 所有 `SHAM_BAND_k`（k=1…10）带宽均为 4 Hz

#### Scenario: 频带首尾相接无间隙

- **WHEN** 遍历 BAND_0 至 BAND_10
- **THEN** BAND_0 上界（rdn+1）等于 BAND_1 下界（rdn+1），BAND_k 上界等于 BAND_{k+1} 下界

---

### Requirement: Power Supply Band Definitions

函数 `make_sham_bands` SHALL 额外包含两个固定工频带：`POWER_SUPPLY_50HZ` 范围为 [49.0, 51.0] Hz（带宽 2 Hz），`POWER_SUPPLY_100HZ` 范围为 [99.0, 101.0] Hz（带宽 2 Hz）。这两个频带与 `rdn` 无关。实现见 `src/sham_power_alg.cpp:31-32`。

#### Scenario: 工频带固定不变

- **WHEN** 以任意 `rdn` 值调用 `make_sham_bands`
- **THEN** `POWER_SUPPLY_50HZ` 始终为 [49.0, 51.0] Hz，`POWER_SUPPLY_100HZ` 始终为 [99.0, 101.0] Hz

---

### Requirement: Random Start Point Resolution

`compute` 函数 SHALL 根据参数 `rdn_freq_pnt` 确定实际使用的随机起点 `rdn`：当 `rdn_freq_pnt < 0.0` 时，使用 `static std::mt19937`（以 `std::random_device` 播种，进程级单例）和 `std::uniform_real_distribution<double>(1.0, max_rdn)` 在 `[1.0, max_rdn]` 范围内随机生成 `rdn`，其中 `max_rdn = std::max(1.0, fs / 2.0 - 41.0)`（当 `fs >= 84` 时保证 `rdn + 41 < fs/2`；当 `fs < 84` 时 `rdn` 退化为 1.0）；当 `rdn_freq_pnt >= 0.0` 时，直接使用传入值，不做范围校验或夹紧。实现见 `src/sham_power_alg.cpp:52-58`。

#### Scenario: 自动随机生成起点（rdn_freq_pnt < 0）

- **WHEN** `rdn_freq_pnt = -1.0`（默认值）且 `fs = 256.0`
- **THEN** `max_rdn = max(1.0, 128.0 - 41.0) = 87.0`，`rdn` 从 `uniform(1.0, 87.0)` 生成
- **AND** 生成的 `rdn` 满足 `rdn >= 1.0` 且 `rdn + 41.0 < fs / 2.0`（即 `rdn + 41 < 128`；此保证仅在 `fs >= 84` 时成立）

#### Scenario: 低采样率下随机范围退化

- **WHEN** `rdn_freq_pnt < 0.0` 且 `fs < 84.0`（即 `fs/2 - 41 < 1`）
- **THEN** `max_rdn` 退化为 1.0，`uniform_real_distribution(1.0, 1.0)` 始终返回 `rdn = 1.0`

#### Scenario: 用户指定非负起点直接使用

- **WHEN** `rdn_freq_pnt = 20.0`
- **THEN** `rdn` 直接等于 20.0，不触发随机生成，不做范围校验

#### Scenario: 用户指定超出 Nyquist 的起点不校验

- **WHEN** `rdn_freq_pnt = 200.0` 且 `fs = 256.0`（Nyquist = 128 Hz）
- **THEN** `rdn` 直接等于 200.0，函数不抛出异常、不夹紧

---

### Requirement: Random Start Point Backfill

`compute` 函数 SHALL 在计算完成后将实际使用的 `rdn` 值写入 `result.features[RDN_FREQ_PNT][ch]`，所有通道写入相同的 `rdn` 值。实现见 `src/sham_power_alg.cpp:81`。

#### Scenario: 指定起点时回填

- **WHEN** `rdn_freq_pnt = 20.0`，通道数为 4
- **THEN** `result.features[RDN_FREQ_PNT][0..3]` 全部等于 20.0

#### Scenario: 自动随机起点时回填

- **WHEN** `rdn_freq_pnt = -1.0`，函数内部生成 `rdn = 42.5`
- **THEN** `result.features[RDN_FREQ_PNT][ch]` 对所有通道均等于 42.5

---

### Requirement: Welch PSD Per-Channel Computation

`compute` 函数 SHALL 对每个通道独立调用 `dsp::welch_psd(x, fs, nperseg)`，其中 `x` 为该通道的时域列向量。`welch_psd` 内部执行 `auto_nperseg` 夹紧（`nperseg = min(nperseg, signal_length)`）、去均值和线性趋势、50% 重叠分段、汉宁窗、零填充至 2 的幂，返回 `(freqs, psd)` 对。实现见 `src/sham_power_alg.cpp:73-74` 及 `src/dsp_utils.cpp:132-209`。

#### Scenario: 正常 Welch PSD 计算

- **WHEN** 输入为 2560 采样点、`fs = 256.0`、`nperseg = 256` 的单通道信号
- **THEN** `welch_psd` 返回 `(freqs, psd)` 对，`freqs` 为 0 至 Nyquist（128 Hz）的等间距频率向量

#### Scenario: nperseg 大于信号长度时自动夹紧

- **WHEN** `nperseg = 256` 但信号仅 100 个采样点
- **THEN** `auto_nperseg` 将 `nperseg` 夹紧为 100，`welch_psd` 以 100 为分段长度执行

---

### Requirement: Band Power Trapezoidal Integration with Nyquist Clamping

对每个频带 `[lo, hi]`，`compute` 函数 SHALL 先将上界夹紧到 Nyquist：`h = min(hi, nyquist)`。若 `h > lo`，则调用 `dsp::band_power(freqs, psd, lo, h)` 做梯形积分求功率（在 PSD 相邻频率 bin 之间线性插值，仅积分与目标频带有交集的部分）；否则写入 0.0。实现见 `src/sham_power_alg.cpp:78-79` 及 `src/dsp_utils.cpp:219-241`。

#### Scenario: 频带完全在 Nyquist 以内

- **WHEN** `rdn = 20.0`、`fs = 256.0`（Nyquist = 128 Hz），BAND_1 = [21.0, 25.0]
- **THEN** `h = min(25.0, 128.0) = 25.0 > 21.0`，正常调用 `band_power(freqs, psd, 21.0, 25.0)`

#### Scenario: 频带上界超出 Nyquist 时部分积分

- **WHEN** 某频带 `lo < Nyquist < hi`
- **THEN** `h = Nyquist`，`band_power` 仅积分 `[lo, Nyquist]` 部分的功率

#### Scenario: 频带下界超出 Nyquist 时写入零

- **WHEN** 某频带 `lo >= Nyquist`
- **THEN** `h = min(hi, nyquist) <= lo`，该频带功率写入 0.0

#### Scenario: 工频带超出 Nyquist

- **WHEN** `fs = 96.0`（Nyquist = 48 Hz），`POWER_SUPPLY_50HZ` 的 `lo = 49 >= 48`
- **THEN** `POWER_SUPPLY_50HZ` 功率为 0.0
- **AND** 当 `fs <= 198.0`（Nyquist <= 99 Hz）时 `POWER_SUPPLY_100HZ` 同理为 0.0

---

### Requirement: FFT Plan Warmup Before Parallel Region

在进入并行循环之前，`compute` 函数 SHALL 计算 `nfft = next_power_of_2(auto_nperseg(num_samples, nperseg))` 并调用 `dsp::warmup_r2c(nfft)` 预创建 FFTW r2c plan。这是 MUST 的，因为 FFTW planner 非线程安全，不能在并行区内首次创建 plan。实现见 `src/sham_power_alg.cpp:63-66`。

#### Scenario: 预热尺寸与实际计算一致

- **WHEN** `num_samples = 2560`、`nperseg = 256`
- **THEN** `auto_nperseg(2560, 256) = 256`，`next_power_of_2(256) = 256`，预热 `nfft = 256`
- **AND** 并行区内 `welch_psd` 内部 `next_power_of_2(auto_nperseg(2560, 256))` 同样得到 256，命中已缓存的 plan

#### Scenario: nperseg 大于信号长度时的预热

- **WHEN** `num_samples = 100`、`nperseg = 256`
- **THEN** `auto_nperseg(100, 256) = 100`，`next_power_of_2(100) = 128`，预热 `nfft = 128`

---

### Requirement: OpenMP Parallel Threshold

`compute` 函数 SHALL 使用 `#pragma omp parallel for if(work >= kOmpMinWork)` 控制并行，其中 `kOmpMinWork = 1L << 17 = 131072`，`work = num_channels × num_samples`。低于此阈值时串行执行。每个通道独立做 `welch_psd`、独立遍历频带、独立写入 `result.features[*][ch]`，不同通道之间无数据共享。实现见 `src/sham_power_alg.cpp:15, 70-71`。

#### Scenario: 达到并行阈值时启用 OpenMP

- **WHEN** `num_channels = 32`、`num_samples = 4096`（`work = 131072 >= kOmpMinWork`）
- **THEN** 并行循环启用，多通道同时处理

#### Scenario: 低于并行阈值时串行执行

- **WHEN** `num_channels = 2`、`num_samples = 512`（`work = 1024 < 131072`）
- **THEN** 并行条件不满足，循环串行执行

#### Scenario: 通道独立性保证无数据竞争

- **WHEN** 并行循环执行
- **THEN** 每个通道仅写 `result.features[*][ch]`（`ch` 为该线程的循环变量），不同通道写入不同内存位置
- **AND** 随机起点 `rdn` 为只读标量，并行区内无 RNG 访问（`static mt19937` 仅在串行区域使用）

---

### Requirement: Zero Initialization of All Outputs

`Sham_Power_SoA` 构造函数 SHALL 将 14 个特征向量全部初始化为 0.0。未被显式写入的位置（如超出 Nyquist 的工频带、零通道情况等）保持 0.0。实现见 `struct/sham_power.h:46-49`。

#### Scenario: 构造后所有特征值为零

- **WHEN** 以 `ch_count = 8` 构造 `Sham_Power_SoA`
- **THEN** `features[i][ch] == 0.0` 对所有 `i ∈ [0, 14)` 和 `ch ∈ [0, 8)` 成立

#### Scenario: 超出 Nyquist 的频带保持零值

- **WHEN** `fs = 80.0`（Nyquist = 40 Hz），`POWER_SUPPLY_50HZ` 和 `POWER_SUPPLY_100HZ` 均超出 Nyquist
- **THEN** 两工频带所有通道功率保持 0.0（初始化值）

---

### Requirement: Error Handling and Degenerate Inputs

`compute` 函数 SHALL 不抛出任何异常（无 `throw` 语句）。当 `fs <= 0` 或 `nperseg < 2` 时，底层 `welch_psd` 返回单点零谱 `(Zero(1), Zero(1))`，`band_power` 对单点 `freqs`（`n < 2`）返回 0.0，所有频带功率退化为 0.0。当 `data` 为 0 列（0 通道）时，并行循环不执行，直接返回空结果。当 `data` 为 0 行（0 采样点）时，`auto_nperseg(0, 256)` 返回 0，`next_power_of_2(0)` 返回 1，所有特征保持 0.0。实现见 `src/sham_power_alg.cpp:46-84` 及 `src/dsp_utils.cpp:119-121, 137-139, 223`。

#### Scenario: 零通道输入

- **WHEN** `data` 为 `(100 × 0)` 矩阵（0 通道）
- **THEN** `num_channels = 0`，构造 `Sham_Power_SoA(0)`（所有 `vector` 长度为 0），并行循环不执行，直接返回空结果

#### Scenario: 零采样点输入

- **WHEN** `data` 为 `(0 × 4)` 矩阵（0 采样点、4 通道）
- **THEN** `auto_nperseg(0, 256) = 0`，`next_power_of_2(0) = 1`，`warmup_r2c(1)` 创建尺寸为 1 的 plan
- **AND** 并行区内 `welch_psd` 因 `nperseg < 2` 返回单点零谱，`band_power` 返回 0.0，所有特征保持 0.0

#### Scenario: 采样率非正

- **WHEN** `fs = 0.0` 或 `fs = -100.0`
- **THEN** `welch_psd` 内部 `fs <= 0.0` 检查触发，返回单点零谱，所有频带功率为 0.0
- **AND** 函数不抛出异常（与 `absolute_power` 不同，后者在 `fs < 600` 时抛 `std::invalid_argument`）

#### Scenario: nperseg 小于 2

- **WHEN** `nperseg = 1` 或 `nperseg = 0`
- **THEN** `welch_psd` 内部 `nperseg < 2` 检查触发，返回单点零谱，所有频带功率为 0.0