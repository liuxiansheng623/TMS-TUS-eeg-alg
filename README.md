# TMS-TUS EEG 特征算法库

面向 TMS-TUS 研究的脑电（EEG）信号特征提取算法库。基于 **C++17 / Eigen / FFTW3 / OpenMP** 实现，提供稳定的 **C ABI** 与 **C# (NuGet)** 封装，可在 Linux（WSL）与 Windows 上构建使用。

## 功能特性

| 算法 | 说明 | 输出 |
|------|------|------|
| **绝对功率** `absolute_power` | Welch PSD + APF 相关频带划分；支持全局 APF 或逐通道重心法（CoG）估计 APF | 14 特征 × 通道 |
| **伪刺激功率** `sham_power` | 以随机起点频率划分 11 个随机频带 + 50/100 Hz 工频 | 14 特征 × 通道 |
| **相位锁定** `phase_locking` | 跨试次相位一致性（ITPC），基于 Hilbert 瞬时相位 | 8 频带 × 通道 |
| **相位-振幅耦合** `pac` | 低频相位调制高频振幅的 Tort 调制指数（MI ∈ [0,1]） | 8 耦合模式 × 通道 |

底层 DSP 原语（`dsp_utils`）：Welch PSD、频带功率、FFT 带通滤波、希尔伯特变换（瞬时相位/振幅）、重心法 APF、FFTW plan 预热。

> 注：`pac_asymmetry` / `PAC_asymmetery.h` 中的 "Asymmetry" 为历史命名，当前实现仅做**逐通道 PAC**，不含左右半球不对称分析（详见 `openspec/specs/phase-amplitude-coupling/spec.md`）。

## 目录结构

```
.
├── src/            # 算法实现（dsp_utils + 4 个算法 + C API）
├── include/        # 算法声明与 C API 头文件
├── struct/         # 特征输出结构体（SoA 布局）
├── tests/          # GoogleTest 单元测试（C++ / C# 互操作）
├── csharp/         # C# 封装 EegAlg（NuGet 包，Windows x64）
├── scripts/        # 构建脚本（WSL / Windows MSVC）
├── openspec/       # 设计文档与规格（OpenSpec）
└── CMakeLists.txt  # 构建入口（库名 eeg_alg）
```

## 依赖

- **Linux / WSL**：CMake ≥ 3.14、g++（C++17）、Eigen3 ≥ 3.3、FFTW3（开发包）、OpenMP、GoogleTest（可选）
  ```bash
  sudo apt install cmake g++ libeigen3-dev libfftw3-dev libgtest-dev
  ```
- **Windows**：Visual Studio（MSVC x64）、CMake、Ninja；Eigen 头文件与 FFTW 源码需外部提供（路径在构建脚本中配置）

## 构建

### Linux / WSL（静态库或共享库）

```bash
# 默认静态库 libeeg_alg.a
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 共享库 libeeg_alg.so
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build -j

# 一键构建 + 测试（共享库）
./scripts/build_and_test_wsl.sh
```

### Windows（MSVC，构建 eeg_alg.dll + C# 互操作测试）

编辑 `scripts/build_windows_msvc.bat` 中的依赖路径（Eigen / FFTW 源码），然后运行：

```bat
scripts\build_windows_msvc.bat
```

## 快速上手

### C API（`include/eeg_alg_capi.h`）

数据约定：输入 **sample-major**（`data[sample × num_channels + channel]`）；输出 **feature-major**（`features_out[feature × num_channels + channel]`），由调用方预分配。返回值 0 表示成功，负值为错误码。

```c
#include "eeg_alg_capi.h"

int num_samples = 2560, num_channels = 8;
double fs = 256.0, apf = 10.0;
double* data = /* 采样点 × 通道的 EEG 数据 */;

// 绝对功率：14 特征 × 8 通道
double ap_out[14 * 8];
eeg_alg_absolute_power(data, num_samples, num_channels, fs, apf, 256, ap_out);

// 相位锁定：输入为 trial-major 扁平数组（试次 × 采样点 × 通道）
int num_trials = 20;
double* epochs = /* ... */;
double pl_out[8 * 8];
eeg_alg_phase_locking(epochs, num_trials, num_samples, num_channels, fs, apf, pl_out);

// 相位-振幅耦合（Tort MI）：8 模式 × 8 通道
double pac_out[8 * 8];
eeg_alg_pac(data, num_samples, num_channels, fs, apf, 18, pac_out);

// 库 ABI 版本
const char* ver = eeg_alg_abi_version();
```

### C++ API

```cpp
#include "absolute_power_alg.h"   // 输出结构体见 struct/absolute_power.h
#include "pac_asymmetry_alg.h"
#include "phase_locking_alg.h"

// 输入：行 = 采样点，列 = 通道
Eigen::MatrixXd data(num_samples, num_channels);

// 1) 绝对功率（全局 APF）
auto ap = absolute_power::compute(data, 1024.0, 10.0, 1024);
double delta_ch0 = ap.features[absolute_power::DELTA][0];

// 2) 个体化绝对功率（逐通道 CoG 估计 APF）
auto indiv = absolute_power::compute_indiv(data, 1024.0, 1024);
double apf_ch0 = indiv.get_apf(0);           // 该通道实际使用的 APF

// 3) 相位锁定（试次 × 采样点 × 通道）
std::vector<Eigen::MatrixXd> epochs;          // 每个试次一个矩阵
auto pl = phase_locking::compute(epochs, 1024.0, 10.0);
double itpc_alpha_ch0 = pl.features[phase_locking::ALPHA][0];

// 4) 相位-振幅耦合
auto pac = pac_asymmetry::compute(data, 1024.0, 10.0, 18);
double mi = pac.features[pac_asymmetry::DELTA_LO_GAMMA][0];  // MI ∈ [0,1]
```

### C#（NuGet 包，Windows x64）

详见 [`csharp/README.md`](csharp/README.md)。核心用法：

```csharp
var spec = EegAlgClient.WelchPsd(signal, fs: 256.0);
double deltaPower = EegAlgClient.BandPower(spec.Frequencies, spec.Power, 1.0, 4.0);
var res = EegAlgClient.AbsolutePower(data, fs: 1024, apf: 10.0);
double loAlpha = res[AbsolutePowerFeature.LoAlpha, 0];
```

## 频带定义

各算法频带均以 **APF（Alpha 峰值频率）** 为锚点（绝对功率 14 特征）：

| 特征 | 频带 (Hz) |
|------|-----------|
| Delta | 1–4 |
| Theta | 4–(APF−2) |
| Low Alpha | (APF−2)–APF |
| High Alpha | APF–(APF+2) |
| Low Beta | (APF+2)–20 |
| High Beta | 20–30 |
| Low Gamma | 30–45 |
| High Gamma | 45–70 |
| Very High Gamma | 95–150 |
| VVHF | 150–300 |
| Muscle | 20–250 |
| 50 Hz / 100 Hz 工频 | 49–51 / 99–101 |
| APF | 使用的 APF 值（或 CoG 估计值） |

相位锁定（8 频带）：Delta、Theta、Low Alpha、High Alpha、Alpha、Low Beta、Beta、Gamma。
PAC（8 模式）：{Delta, Theta, Alpha, Beta} 相位 × {Lo Gamma 30–45, Hi Gamma 45–70} 振幅。

## 测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

测试覆盖：Welch PSD 语义、频带功率、APF 估计、四个算法的输出维度/数值范围/边界输入、PAC 强耦合检测、噪声基线、C ABI、共享库符号导出与 C# 互操作。

## 设计文档

- 规格说明：[`openspec/specs/`](openspec/specs/)（dsp-foundation、absolute-power、sham-power、phase-locking、phase-amplitude-coupling、c-api、csharp-api、shared-library）
- 变更记录：[`openspec/changes/`](openspec/changes/)
- C# 封装教程：[`csharp/README.md`](csharp/README.md)
