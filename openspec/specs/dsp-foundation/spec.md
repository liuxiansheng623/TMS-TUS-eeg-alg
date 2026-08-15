# dsp-foundation Specification

## Purpose

本能力定义一套可被四个 EEG 算法（absolute_power、sham_power、phase_locking、pac_asymmetry）共享复用的 DSP 基座原语。涵盖 Hann 窗、Welch 单边 PSD、频带梯形积分功率、FFT 频域带通滤波（余弦过渡带）、解析信号 Hilbert 变换、带通与 Hilbert 合并版本、重心法 APF、FFT 尺寸与窗长工具函数、以及 FFTW plan 缓存与预热接口。全部基于 FFTW3（双精度）+ Eigen 实现，定义在命名空间 `dsp` 下，源码位于 `include/dsp_utils.h` 与 `src/dsp_utils.cpp`。

## Requirements

### Requirement: Hann 窗对称系数生成

`hann_window(n)` SHALL 返回长度为 `n` 的对称 Hann 窗系数向量，公式为 `w[i] = 0.5 * (1.0 - cos(2.0 * PI * i / (n - 1)))`，其中 `i ∈ [0, n-1]`，常量 `PI = 3.14159265358979323846`（`src/dsp_utils.cpp:16, 93-100`）。

#### Scenario: 常规窗长 n >= 2

- **WHEN** 调用 `hann_window(n)` 且 `n >= 2`
- **THEN** 返回长度为 `n` 的向量，首尾元素为 `0.0`，中心元素为 `1.0`（奇数 `n` 时精确为 1.0，偶数 `n` 时对称轴两侧相等且接近 1.0）

#### Scenario: n 等于 1

- **WHEN** 调用 `hann_window(1)`
- **THEN** 返回单元素向量 `[1.0]`

#### Scenario: n 小于等于 0

- **WHEN** 调用 `hann_window(n)` 且 `n <= 0`
- **THEN** 返回空向量（`size() == 0`）

---

### Requirement: FFT 尺寸计算（最小 2 的幂）

`next_power_of_2(n)` SHALL 返回满足 `p >= n` 的最小 2 的幂 `p`，算法为 `p = 1; while (p < n) p <<= 1; return p;`（`src/dsp_utils.cpp:107-111`）。

#### Scenario: n 为 2 的幂

- **WHEN** 调用 `next_power_of_2(n)` 且 `n` 为 2 的幂（如 1, 2, 4, 8, 256, 1024）
- **THEN** 返回值等于 `n`

#### Scenario: n 不是 2 的幂

- **WHEN** 调用 `next_power_of_2(n)` 且 `n` 不是 2 的幂（如 3, 5, 100, 257）
- **THEN** 返回值为大于 `n` 的最小 2 的幂（分别为 4, 8, 128, 512）

#### Scenario: n 小于等于 0

- **WHEN** 调用 `next_power_of_2(n)` 且 `n <= 0`
- **THEN** 返回 `1`（`while` 循环不进入）

---

### Requirement: Welch 分段长度自动夹紧

`auto_nperseg(signal_length, requested)` SHALL 返回 `min(requested, signal_length)`；`signal_length <= 0` 时返回 `0`（`src/dsp_utils.cpp:119-122`）。

#### Scenario: 正常信号长度大于请求值

- **WHEN** 调用 `auto_nperseg(1000, 256)`
- **THEN** 返回 `256`

#### Scenario: 信号长度小于请求值

- **WHEN** 调用 `auto_nperseg(100, 256)`
- **THEN** 返回 `100`

#### Scenario: 空信号

- **WHEN** 调用 `auto_nperseg(0, 256)` 或 `auto_nperseg(-5, 256)`
- **THEN** 返回 `0`

---

### Requirement: Welch 单边功率谱密度估计

`welch_psd(signal, fs, nperseg)` SHALL 以 50% 重叠、Hann 窗加权、去均值与最小二乘线性去趋势、零填充到 2 的幂的方式计算单边 PSD（`src/dsp_utils.cpp:132-209`）。

#### Scenario: 正常正弦信号输入

- **WHEN** 调用 `welch_psd(signal, fs, 256)` 且 `signal` 为长度足够的正弦波、`fs > 0`
- **THEN** 返回 `(freqs, psd)`，其中 `freqs` 长度为 `nfft/2 + 1`（`nfft = next_power_of_2(nperseg)`），`freqs[k] = k * fs / nfft`；`psd` 为非负值；2 Hz 正弦信号在目标频带的 `band_power` 结果与 `A²/2` 的误差在 ±25% 内

#### Scenario: 每段预处理：去均值与去线性趋势

- **WHEN** 输入信号为纯常量或纯线性趋势
- **THEN** 去均值和去趋势后残余功率 `< 1e-20`

#### Scenario: 退化输入：信号过短

- **WHEN** 调用 `welch_psd(signal, fs)` 且 `signal.size() < 2`
- **THEN** `auto_nperseg` 夹紧后 `nperseg < 2`，返回 `(Zero(1), Zero(1))` 单点零谱

#### Scenario: 退化输入：fs 小于等于 0

- **WHEN** 调用 `welch_psd(signal, 0.0)` 或 `welch_psd(signal, -1.0)`
- **THEN** 返回 `(Zero(1), Zero(1))` 单点零谱，不崩溃不抛异常

#### Scenario: 50% 重叠与段数计算

- **WHEN** 信号长度 `L = 1024`、`nperseg = 256`
- **THEN** `overlap = 128`、`step = 128`、`n_segs = (1024 - 256) / 128 + 1 = 7`

#### Scenario: 单边化缩放

- **WHEN** `welch_psd` 完成段间平均
- **THEN** 对 `k ∈ [1, n_freqs - 2]`（非 DC、非 Nyquist）的频率点 `psd[k] *= 2.0`；DC (`k=0`) 和 Nyquist (`k=n_freqs-1`) 不缩放

---

### Requirement: 频带梯形积分功率

`band_power(freqs, psd, f_low, f_high)` SHALL 在 `[f_low, f_high]` 内以梯形积分计算频带功率（`src/dsp_utils.cpp:219-241`）。

#### Scenario: 正常频带积分

- **WHEN** 调用 `band_power(freqs, psd, 8.0, 13.0)` 且 `freqs` 单调递增、`psd` 等长、`f_high > f_low`
- **THEN** 对每个相邻 bin `[f0, f1]` 求与 `[f_low, f_high]` 的交集 `[a, b]`，在交集上线性插值 PSD 得 `p_a`、`p_b`，梯形积分 `+= 0.5 * (p_a + p_b) * (b - a)`

#### Scenario: 频带部分与 bin 重叠

- **WHEN** 频带 `[f_low, f_high]` 仅与部分 bin 有交集
- **THEN** 通过 `a = max(f0, f_low)`、`b = min(f1, f_high)` 自然处理部分重叠；仅当 `b > a` 且 `f1 > f0` 时积分

#### Scenario: f_high 小于等于 f_low

- **WHEN** 调用 `band_power(freqs, psd, 13.0, 8.0)` 或 `band_power(freqs, psd, 10.0, 10.0)`
- **THEN** 直接返回 `0.0`

#### Scenario: 频率向量长度不足

- **WHEN** 调用 `band_power(freqs, psd, f_low, f_high)` 且 `freqs.size() < 2`
- **THEN** 返回 `0.0`

#### Scenario: 频带与所有 bin 无交集

- **WHEN** 频带 `[f_low, f_high]` 完全落在 `freqs` 范围之外
- **THEN** 所有 bin 的 `b <= a`，`power` 保持 `0.0`

---

### Requirement: FFT 频域带通滤波

`bandpass_filter(signal, fs, f_low, f_high, transition)` SHALL 通过 r2c 正变换 + 频域掩码（含余弦过渡带）+ 共轭镜像构造全长谱 + c2c 逆变换实现零相位带通滤波（`src/dsp_utils.cpp:252-309`）。

#### Scenario: 正常带通滤波

- **WHEN** 调用 `bandpass_filter(signal, fs, 9.0, 11.0, 0.5)` 且 `signal` 为含 50 Hz 分量的混合信号
- **THEN** 50 Hz 分量被抑制，滤波后信号 RMS 衰减 `< 5%`（相对于通带内分量）

#### Scenario: 频域掩码公式

- **WHEN** 对频率点 `f` 计算 `mask`
- **THEN** 通带 `[f_low, f_high]` 内 `mask = 1.0`；下过渡带 `[f_low - transition, f_low)` 内 `mask = 0.5 * (1 - cos(PI * (f - f_low + transition) / transition))`；上过渡带 `(f_high, f_high + transition]` 内 `mask = 0.5 * (1 + cos(PI * (f - f_high) / transition))`；其余 `mask = 0.0`

#### Scenario: transition 小于等于 0

- **WHEN** 调用 `bandpass_filter(signal, fs, f_low, f_high, 0.0)` 或负值
- **THEN** 过渡带分支条件 `transition > 0.0` 不满足，掩码变为矩形（通带 = 1，其余 = 0）

#### Scenario: 共轭镜像构造全长谱

- **WHEN** 正向 r2c FFT 完成后
- **THEN** `c_full[k] = c_out[k]`（`k < n_freqs`）；`c_full[k] = conj(c_out[nfft - k])`（`k >= n_freqs`），供 c2c 逆变换

#### Scenario: 逆变换归一化

- **WHEN** c2c 逆变换完成后
- **THEN** `result[i] = Re(c_res[i]) / nfft`，仅取实部

#### Scenario: 空信号或非法采样率

- **WHEN** 调用 `bandpass_filter(signal, fs, f_low, f_high)` 且 `n == 0` 或 `fs <= 0`
- **THEN** 返回 `Zero(n)` 零向量（`n == 0` 时为空向量）

---

### Requirement: 解析信号 Hilbert 变换

`hilbert_transform(signal)` SHALL 通过 r2c 正变换 + 解析频谱构造 + c2c 逆变换提取瞬时相位与瞬时振幅，不做零填充（`src/dsp_utils.cpp:316-369`）。

#### Scenario: 正常正弦信号

- **WHEN** 调用 `hilbert_transform(signal)` 且 `signal` 为带通后的正弦信号
- **THEN** 返回 `(phase, amplitude)`，中段平均振幅误差 `< 15%`，`phase` 值域为 `[-pi, pi]`（`atan2` 值域）

#### Scenario: 解析信号频谱构造（偶数长度）

- **WHEN** 信号长度 `m` 为偶数
- **THEN** DC (`k=0`): `c_full[0] = c_out[0]`，虚部置 0；正频率 `k ∈ [1, n_freqs-2]`: `c_full[k] = 2 * c_out[k]`；Nyquist (`k = m/2`): 不变且虚部置 0；负频率区 = 0

#### Scenario: 解析信号频谱构造（奇数长度）

- **WHEN** 信号长度 `m` 为奇数
- **THEN** 最高正频率 (`k = n_freqs - 1`) 乘 2 而非作为 Nyquist 不变处理

#### Scenario: 逆变换归一化与输出

- **WHEN** c2c 逆变换完成后
- **THEN** `re = c_res[i][0] / m`，`im = c_res[i][1] / m`；`phase[i] = atan2(im, re)`，`amp[i] = sqrt(re² + im²)`

#### Scenario: 空信号

- **WHEN** 调用 `hilbert_transform(signal)` 且 `n == 0`
- **THEN** 返回 `(Zero(0), Zero(0))`

---

### Requirement: 带通与 Hilbert 合并版本

`bandpass_analytic(signal, fs, f_low, f_high, transition)` SHALL 在单边谱上同时施加带通掩码并构造解析谱（正频率 ×2、负频率置 0），再做一次逆变换，等价于 `bandpass_filter() + hilbert_transform()` 但 FFT 次数减半（`src/dsp_utils.cpp:382-451`）。

#### Scenario: 与分步调用等价

- **WHEN** 对同一信号分别调用 `bandpass_analytic(signal, fs, f_low, f_high)` 与 `hilbert_transform(bandpass_filter(signal, fs, f_low, f_high))`
- **THEN** 两组 `(phase, amplitude)` 在浮点精度内一致

#### Scenario: 掩码加解析频谱合并

- **WHEN** 正向 r2c FFT 完成后
- **THEN** DC: `c_full[0] = mask_at(0) * c_out[0]`；正频率 `k ∈ [1, n_freqs-2]`: `c_full[k] = 2 * mask_at(k) * c_out[k]`；Nyquist (`k = nfft/2`): `c_full[nfft/2] = mask_at(nfft/2) * c_out[n_freqs-1]`（`nfft` 为 2 的幂恒偶数）；负频率保持 0

#### Scenario: 逆变换归一化与输出截断

- **WHEN** c2c 逆变换完成后
- **THEN** `re = c_res[i][0] / nfft`，`im = c_res[i][1] / nfft`；`phase[i] = atan2(im, re)`，`amp[i] = sqrt(re² + im²)`；仅输出前 `n` 个点（截断零填充部分）

#### Scenario: 空信号或非法采样率

- **WHEN** 调用 `bandpass_analytic(signal, fs, f_low, f_high)` 且 `n == 0` 或 `fs <= 0`
- **THEN** 返回 `(Zero(n), Zero(n))`（`n == 0` 时为空向量对）

---

### Requirement: 重心法 Alpha 峰值频率

`compute_apf_cog(freqs, psd, cog_low, cog_high)` SHALL 以 `APF = sum(f * PSD) / sum(PSD)` 计算指定区间内的重心频率，默认区间 `[8.0, 13.0]` Hz（`src/dsp_utils.cpp:461-474`）。

#### Scenario: 单峰正弦信号

- **WHEN** 调用 `compute_apf_cog(freqs, psd)` 且 `psd` 在 10 Hz 处有单峰
- **THEN** 返回 APF 与 10 Hz 的误差 `< 0.6 Hz`

#### Scenario: 区间内总功率近零

- **WHEN** 调用 `compute_apf_cog(freqs, psd, 8.0, 13.0)` 且 `[8.0, 13.0]` 内 `sum(PSD) < 1e-30`
- **THEN** 返回 `std::nan("")`（NaN）

#### Scenario: 默认参数

- **WHEN** 调用 `compute_apf_cog(freqs, psd)` 省略后两参数
- **THEN** `cog_low = 8.0`、`cog_high = 13.0`（Alpha 频带）

---

### Requirement: FFTW plan 缓存与预热

`warmup_r2c(n)` 与 `warmup_c2c_backward(n)` SHALL 在 `g_fftw_mutex` 锁内调用 `get_plan_locked` 预创建 plan 并写入全局缓存；`get_plan_locked` 先查缓存，未命中则用 `fftw_alloc` 创建临时对齐缓冲区、以 `FFTW_ESTIMATE` 创建 plan，释放临时缓冲区后写入缓存（`src/dsp_utils.cpp:64-85, 481-494`）。

#### Scenario: R2C plan 创建

- **WHEN** 调用 `warmup_r2c(n)` 且缓存中无 `(R2C, n)` 条目
- **THEN** 创建 `fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE)`，其中 `in = fftw_alloc_real(n)`、`out = fftw_alloc_complex(n/2 + 1)`，释放临时缓冲区后写入缓存

#### Scenario: C2C_Backward plan 创建

- **WHEN** 调用 `warmup_c2c_backward(n)` 且缓存中无 `(C2C_Backward, n)` 条目
- **THEN** 创建 `fftw_plan_dft_1d(n, in, out, FFTW_BACKWARD, FFTW_ESTIMATE)`，其中 `in/out = fftw_alloc_complex(n)`，释放临时缓冲区后写入缓存

#### Scenario: 缓存命中

- **WHEN** 调用 `warmup_r2c(n)` 或 `warmup_c2c_backward(n)` 且缓存中已有对应条目
- **THEN** 直接返回缓存 plan，不重新创建

#### Scenario: PlanKey 哈希

- **WHEN** 计算 `PlanKey{kind, n}` 的哈希
- **THEN** 结果为 `std::hash<long long>{}(static_cast<long long>(kind) * 1000003LL + n)`

---

### Requirement: 线程安全与并行执行语义

所有 DSP 函数 SHALL 在 `g_fftw_mutex` 锁内查询/创建 plan，在锁外执行 `fftw_execute_dft_r2c` 或 `fftw_execute_dft`，以支持多线程并行区内并发执行（`src/dsp_utils.cpp:40-41, 160-163, 263-266, 300-303, 327-330, 355-358, 394-398, 436-440`）。

#### Scenario: 锁内取 plan 锁外执行

- **WHEN** `welch_psd`、`bandpass_filter`、`hilbert_transform`、`bandpass_analytic` 调用 FFT
- **THEN** 用 `std::lock_guard<std::mutex>` 包裹 `get_plan_locked` 调用取得 plan，释放锁后再调用 `fftw_execute_dft_r2c` 或 `fftw_execute_dft`

#### Scenario: 并发执行无跨线程串扰

- **WHEN** 多线程并行调用 `welch_psd`（如 32 通道 × 8192 采样点）
- **THEN** 各线程结果互不污染

#### Scenario: Plan 复用不污染数据

- **WHEN** 连续两次调用 `welch_psd` 使用同尺寸 plan
- **THEN** 各自结果互不污染

#### Scenario: FFTW new-array execute

- **WHEN** plan 以临时缓冲区创建（`FFTW_ESTIMATE`）后通过 `fftw_execute_dft_r2c` 或 `fftw_execute_dft` 对任意同尺寸 `fftw_alloc` 缓冲区执行
- **THEN** 利用 `fftw_alloc` 保证的 SIMD 对齐，执行正确

---

### Requirement: FFTW 资源 RAII 管理

所有 FFTW 内存分配 SHALL 通过 `fftw_alloc_real` / `fftw_alloc_complex`（保证 SIMD 对齐），由 `unique_ptr` 加自定义 Deleter（`RealDeleter` / `CpxDeleter`，调用 `fftw_free`）管理，保证异常安全无泄漏（`src/dsp_utils.cpp:19-38`）。

#### Scenario: 正常执行后释放

- **WHEN** `welch_psd` 等函数正常返回
- **THEN** `RealPtr` 和 `CpxPtr` 析构时调用 `fftw_free`，无内存泄漏

#### Scenario: 异常路径释放

- **WHEN** 函数内部抛出异常
- **THEN** `unique_ptr` 析构自动调用 `fftw_free`，无泄漏

---

## Notes

- **命名空间**：所有公开 API 位于 `namespace dsp`；内部匿名命名空间含 RAII 封装、全局互斥量 `g_fftw_mutex`、`PlanKind` 枚举（`R2C`、`C2C_Backward`）、`PlanKey` / `PlanKeyHash`、`g_plan_cache` 以及 `get_plan_locked`。
- **Plan 缓存生命周期**：缓存在进程生命周期内复用，无驱逐策略；注释说明实际只会用到少数几种尺寸。
- **并行约定**：四个上层算法在通道乘采样数超过阈值时走并行路径；DSP 基座通过锁内取 plan + 锁外 execute 支持该并行。
- **测试钉住的行为底线**：`WelchSineBandPower` 要求 2 Hz 正弦 `band_power` 在 ±25% 内符合 `A²/2`；`CoGSinglePeak` 要求 10 Hz 正弦 APF 误差 `< 0.6 Hz`；`BandpassRejectsStopband` 要求 50 Hz 信号经 9-11 Hz 带通后 RMS 衰减 `< 5%`；`HilbertAmplitudeOfSine` 要求中段平均振幅误差 `< 15%`；`PlanReuseAcrossCalls` 要求 plan 复用不污染数据。