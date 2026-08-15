# Phase-Locking Specification

## Purpose

计算试次间相位一致性（ITPC, Inter-Trial Phase Coherence）：对分段 EEG 数据（epochs），逐通道、逐频带衡量跨试次的瞬时相位锁定程度。ITPC=1 表示所有试次在某时间点相位完全一致，≈0 表示相位随机分散。输出 8 个频带 × 通道数的 ITPC 特征矩阵，用于后续神经调控效果评估。

**源码文件映射：**
- 输出结构体：`struct/phase_locking.h`（`EEG_Phase_Locking_SoA`、`Phase_Locking_Index` 枚举）
- 算法声明：`include/phase_locking_alg.h`（`phase_locking::compute`）
- 算法实现：`src/phase_locking_alg.cpp`
- DSP 依赖：`include/dsp_utils.h`（`bandpass_analytic`、`warmup_r2c`、`warmup_c2c_backward`、`next_power_of_2`）
- 测试：`tests/test_algorithms.cpp`（PhaseLocking 组，3 个测试用例）

## Requirements

### Requirement: ITPC 核心算法三步流程

`phase_locking::compute(epochs, fs, apf)` SHALL 对每个通道 ch、每个频带 [lo, hi] 执行以下三步计算：(1) 对每个 trial t 的信号 `epochs[t].col(ch)` 调用 `dsp::bandpass_analytic(x, fs, lo, hi)` 获取带通解析信号的瞬时相位 φ(t,n)；(2) 对每个时间点 n，累加跨 trial 的复指数 `sum[n] = Σ_{t=0}^{T-1} exp(i·φ(t,n))`；(3) 计算 `ITPC = (1/num_samples) · Σ_{n=0}^{num_samples-1} |sum[n] / T|`。（src/phase_locking_alg.cpp:66-84）

#### Scenario: 同相位正弦信号产生高 ITPC

- **WHEN** 输入 20 个 trial 的同相位 10Hz 正弦信号（fs=256, apf=10, 512 采样点 × 4 通道），调用 `phase_locking::compute`
- **THEN** ALPHA 频带（8–12Hz）的 ITPC 值 > 0.9
- **AND** 所有 8 个频带的 features 向量长度均等于通道数 4

#### Scenario: 随机相位信号产生低 ITPC

- **WHEN** 输入 20 个 trial 的随机初相位 10Hz 正弦信号（初相位在 [0, 2π) 均匀分布），调用 `phase_locking::compute`
- **THEN** ALPHA 频带的 ITPC 值 < 0.5

### Requirement: ITPC 归一化公式

ITPC 值 SHALL 严格按公式 `ITPC = (1/N) · Σ_{n=0}^{N-1} | (1/T) · Σ_{t=0}^{T-1} exp(i·φ(t,n)) |` 计算：先对 trial 维度取平均（除以 T），取复数模长，再对时间维度取平均（除以 num_samples）。值域理论为 [0,1]。（src/phase_locking_alg.cpp:79-83）

#### Scenario: 归一化顺序保证值域

- **WHEN** 对任意合法输入调用 `phase_locking::compute`
- **THEN** 每个输出 ITPC 值 ∈ [0,1]（数学上由三角不等式保证：每个 `exp(i·φ)` 模长为 1，平均值模长 ≤ 1）
- **AND** 代码无显式 clamp 操作

### Requirement: 8 频带定义表（APF 参数化）

`make_bands(apf)` SHALL 返回 8 个 `(index, lo, hi)` 三元组，定义如下：DELTA(1.0, 4.0)、THETA(4.0, apf-2.0)、LO_ALPHA(apf-2.0, apf)、HI_ALPHA(apf, apf+2.0)、ALPHA(apf-2.0, apf+2.0)、LO_BETA(apf+2.0, 20.0)、BETA(20.0, 30.0)、GAMMA(30.0, 45.0)。其中 ALPHA 频带为 LO_ALPHA ∪ HI_ALPHA 的合并频带，与 LO_ALPHA/HI_ALPHA 存在频域重叠。（src/phase_locking_alg.cpp:24-35）

#### Scenario: APF=10 时的频带边界

- **WHEN** apf = 10.0
- **THEN** 8 个频带边界分别为 DELTA[1,4]、THETA[4,8]、LO_ALPHA[8,10]、HI_ALPHA[10,12]、ALPHA[8,12]、LO_BETA[12,20]、BETA[20,30]、GAMMA[30,45]

#### Scenario: APF 参数化频带随 APF 变化

- **WHEN** apf 值改变（例如 apf = 12.0）
- **THEN** THETA 上界变为 10.0，LO_ALPHA 变为 [10,12]，HI_ALPHA 变为 [12,14]，ALPHA 变为 [10,14]，LO_BETA 下界变为 14.0
- **AND** DELTA、BETA、GAMMA 的边界保持固定不变

### Requirement: 复指数累加使用 std::polar

跨 trial 的相位累加 SHALL 采用 `sum[n] += std::polar(1.0, phase[n])`，等价于 `exp(i·φ) = cos(φ) + i·sin(φ)`，模长固定为 1.0。（src/phase_locking_alg.cpp:76）

#### Scenario: 单 trial 时 ITPC 退化为 1.0

- **WHEN** 仅输入 1 个 trial（T=1）
- **THEN** `sum[n]` 仅有一次累加，`|std::polar(1.0, φ) / 1.0| = 1.0` 恒成立
- **AND** 所有频带所有通道的 ITPC 均为 1.0（数学退化但不崩溃）

### Requirement: 使用 bandpass_analytic 合并带通与 Hilbert

对每个 trial 的单通道信号 SHALL 调用 `dsp::bandpass_analytic(x, fs, lo, hi)` 获得 `(inst_phase, inst_amplitude)` 对，仅使用 `.first`（瞬时相位），丢弃 `.second`（瞬时振幅）。一次正/逆 FFT 直接得带通信号的瞬时相位，比先 `bandpass_filter` 再 `hilbert_transform` 减少一半 FFT 次数。`transition` 参数使用默认值 0.5 Hz（未显式传入）。（src/phase_locking_alg.cpp:73-74, include/dsp_utils.h:87-89）

#### Scenario: 仅使用相位分量

- **WHEN** 对任意 trial 的任意通道信号调用 `dsp::bandpass_analytic`
- **THEN** 返回值 pair 的 `.first`（瞬时相位）用于后续 `std::polar` 累加
- **AND** 返回值 pair 的 `.second`（瞬时振幅）被丢弃，不参与 ITPC 计算

### Requirement: FFT plan 预热保证线程安全

在进入 OpenMP 并行区之前，SHALL 计算 `nfft = dsp::next_power_of_2(num_samples)`，然后调用 `dsp::warmup_r2c(nfft)` 和 `dsp::warmup_c2c_backward(nfft)` 预热 FFTW plan 缓存。这是因为 FFTW 的 planner 非线程安全，不能在并行区内首次创建 plan。（src/phase_locking_alg.cpp:58-60, include/dsp_utils.h:93-103）

#### Scenario: 预热在并行前完成

- **WHEN** `phase_locking::compute` 被调用且 epochs 非空
- **THEN** 在 `#pragma omp parallel for` 之前，先调用 `dsp::warmup_r2c(nfft)` 和 `dsp::warmup_c2c_backward(nfft)`
- **AND** `nfft` 等于 `dsp::next_power_of_2(num_samples)`
- **AND** 因 `dsp::bandpass_analytic` 内部 FFT 尺寸恒为 `next_power_of_2(信号长度)`、而每个 trial 单通道信号长度恒为 `num_samples`，预热 SHALL 覆盖并行区内全部 plan 尺寸，并行区内不会首次创建 plan

### Requirement: OpenMP 并行阈值

并行条件 SHALL 为 `work = num_channels × num_samples × T ≥ kOmpMinWork`，其中 `kOmpMinWork = 131072 (1<<17)`。低于此阈值串行执行。并行维度为通道（`#pragma omp parallel for` on ch loop），各通道写不同的 `result.features[*][ch]`，无数据竞争。（src/phase_locking_alg.cpp:16, 64-65）

#### Scenario: 大输入触发并行

- **WHEN** `num_channels × num_samples × T ≥ 131072`（例如 8 通道 × 512 采样点 × 40 trial = 163840）
- **THEN** OpenMP parallel for 条件为 true，通道维度并行执行

#### Scenario: 小输入退化为串行

- **WHEN** `num_channels × num_samples × T < 131072`（例如 4 通道 × 256 采样点 × 5 trial = 5120）
- **THEN** OpenMP parallel for 的 if 条件为 false，不创建线程，退化为串行执行

### Requirement: 输出结构体 SoA 布局

`EEG_Phase_Locking_SoA` SHALL 使用 `std::array<std::vector<double>, PL_FEATURE_COUNT> features`（SoA，Structure of Arrays）布局。外层固定 8 个频带（`PL_FEATURE_COUNT = 8`），内层按通道数动态分配。构造时全部初始化为 0.0。`num_channels` 字段设为 `ch_count`。（struct/phase_locking.h:34, 43-47）

#### Scenario: 构造指定通道数的结果对象

- **WHEN** 调用 `EEG_Phase_Locking_SoA(ch_count)` 构造结果对象
- **THEN** `num_channels == ch_count`
- **AND** `features` 包含 8 个 `std::vector<double>`，每个长度为 `ch_count`，所有元素初始值为 0.0

#### Scenario: at() 返回可写引用且无越界检查

- **WHEN** 调用 `result.at(feature_idx, ch_idx)`
- **THEN** 返回 `features[feature_idx][ch_idx]` 的可写引用
- **AND** 无越界检查，调用方负责保证两个索引均未越界（struct/phase_locking.h:54-58）

### Requirement: 频带枚举定义

`Phase_Locking_Index` 枚举 SHALL 定义 8 个频带索引常量：`DELTA=0`, `THETA=1`, `LO_ALPHA=2`, `HI_ALPHA=3`, `ALPHA=4`, `LO_BETA=5`, `BETA=6`, `GAMMA=7`，以及 `PL_FEATURE_COUNT=8`。注意：枚举中 `BETA=6` 注释为 'high beta'，而 `LO_BETA=5` 注释为 'low beta'；头文件声明处（include/phase_locking_alg.h:29）将 BETA 简单标注为 'BETA 20-30'，两处注释风格不完全一致。（struct/phase_locking.h:18-28）

#### Scenario: 枚举值与频带对应

- **WHEN** 使用 `phase_locking::ALPHA`（值 4）索引 `features`
- **THEN** 访问的是 ALPHA 频带（apf-2.0 到 apf+2.0）的 ITPC 结果
- **AND** `PL_FEATURE_COUNT` 等于 8，可用于遍历所有频带

### Requirement: 维度基准取自 epochs[0]

`num_channels` SHALL 取自 `epochs[0].cols()`，`num_samples` 取自 `epochs[0].rows()`，`T` 取自 `epochs.size()`。后续 trial 若维度不一致，代码不进行检查，由调用方保证所有 trial 等长且通道数一致。（src/phase_locking_alg.cpp:50-52, include/phase_locking_alg.h:21-22）

#### Scenario: 正常维度输入

- **WHEN** 输入 8 通道 × 256 采样点 × 5 trial 的等维 epochs
- **THEN** `num_channels = 8`，`num_samples = 256`，`T = 5`
- **AND** 输出 `result.num_channels == 8`，每个 `features[i].size() == 8`

#### Scenario: trial 维度不一致无检查

- **WHEN** 后续 trial 的 `rows()` 或 `cols()` 与 `epochs[0]` 不同
- **THEN** 代码直接按 `epochs[0]` 的维度访问所有 trial 的 `col(ch)` 和行数，可能产生越界或截断
- **AND** 无一致性检查（由调用方保证所有 trial 等长且通道数一致）

### Requirement: 空 epochs 列表返回零通道结果

当 `epochs.empty()` 为 true 时，SHALL 直接返回 `EEG_Phase_Locking_SoA(0)`，即 `num_channels=0`、8 个空 vector 的结果对象。不抛异常，不访问任何 trial 数据。（src/phase_locking_alg.cpp:48）

#### Scenario: 空输入不崩溃

- **WHEN** 传入空的 `std::vector<Eigen::MatrixXd>`（`epochs.empty() == true`）
- **THEN** 返回 `EEG_Phase_Locking_SoA(0)`
- **AND** `result.num_channels == 0`，8 个 `features[i]` 均为空 vector
- **AND** 不抛异常

### Requirement: 无显式输入校验

`compute` 函数 SHALL 不对 `fs`、`apf` 或输入信号做合法性校验。`fs ≤ 0`、不合理的 `apf`（如 `apf < 3` 导致 THETA 上界 < 1）、NaN 输入信号等异常情况，行为由 `dsp::bandpass_analytic` 及下游数学函数决定。（src/phase_locking_alg.cpp:46-87）

#### Scenario: fs 或 apf 异常值传递

- **WHEN** `fs ≤ 0` 或 `apf` 使频带下界 ≥ 上界
- **THEN** 参数直接传递给 `dsp::bandpass_analytic`，行为由该函数决定
- **AND** `phase_locking::compute` 本身不抛异常、不做校验

#### Scenario: NaN 输入信号传播

- **WHEN** 输入信号中包含 NaN 值
- **THEN** NaN 通过 FFT 传播到相位输出，`std::polar(1.0, NaN)` 产生 NaN 实/虚部，累加后 `std::abs()` 返回 NaN
- **AND** 最终 ITPC 为 NaN，不抛异常

### Requirement: 常量定义

文件内匿名命名空间 SHALL 定义 `constexpr double PI = 3.14159265358979323846` 和 `constexpr long kOmpMinWork = 1L << 17`（= 131072）。PI 常量在当前算法主体中未直接使用（相位由 `bandpass_analytic` 内部产出），属于预留常量。（src/phase_locking_alg.cpp:14, 16）

#### Scenario: 常量值正确

- **WHEN** 检查文件内匿名命名空间中的常量
- **THEN** `PI` 等于 3.14159265358979323846
- **AND** `kOmpMinWork` 等于 131072（即 `1L << 17`）

### Requirement: 计算性能与 FFT 调用量

算法 SHALL 对每个通道 × 频带 × trial 调用一次 `dsp::bandpass_analytic`，总计 `num_channels × 8 × T` 次 FFT 对（每次含 1 次 r2c 正变换 + 1 次 c2c 逆变换）。这是算法的主要计算瓶颈。OpenMP 仅在通道维度并行（外层 for ch），频带和 trial 均为串行内层循环。

#### Scenario: FFT 调用次数与输入规模成正比

- **WHEN** 输入 C 通道、T trial 的 epochs
- **THEN** 总共调用 `C × 8 × T` 次 `dsp::bandpass_analytic`
- **AND** 每次调用含 1 次 r2c 正变换 + 1 次 c2c 逆变换
