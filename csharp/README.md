# EegAlg 使用教程（NuGet 包）

`EegAlg` 是 EEG 信号分析算法库的 C# 封装，内置原生 DLL（`eeg_alg.dll` + `fftw3.dll`），在 **Windows x64** 上开箱即用。

## 前置条件

- Windows x64
- .NET SDK 8.0 或更高（[下载地址](https://dotnet.microsoft.com/download)）
- 已拿到 `EegAlg.<版本>.nupkg`（在本仓库的 `artifacts/nuget/` 目录下）

## 第一步：配置本地 NuGet 源（只需一次）

打开命令行（PowerShell / cmd），把本仓库的 `artifacts/nuget` 目录注册成一个 NuGet 源：

```powershell
dotnet nuget add source "E:\...\artifacts\nuget" --name EegAlgLocal
```

> 把路径换成你自己的仓库路径。

## 第二步：创建项目并引用包

```powershell
dotnet new console -n MyEegApp
cd MyEegApp
dotnet add package EegAlg --version 1.0.0 --source EegAlgLocal
```

## 第三步：写代码

把 `Program.cs` 改成下面这样（计算一个 2 Hz 正弦波的 delta 频带功率）：

```csharp
using EegAlg;

// 生成 2 Hz 正弦信号，采样率 256 Hz
double[] signal = new double[2560];
for (int i = 0; i < signal.Length; i++)
    signal[i] = Math.Sin(2.0 * Math.PI * 2.0 * i / 256.0);

// 计算功率谱，再求 1–4 Hz（delta）频带功率
var spec = EegAlgClient.WelchPsd(signal, fs: 256.0);
double deltaPower = EegAlgClient.BandPower(spec.Frequencies, spec.Power, 1.0, 4.0);

Console.WriteLine($"delta power = {deltaPower:F4}");
Console.WriteLine($"library version = {EegAlgClient.Version}");
```

## 第四步：运行

```powershell
dotnet run
```

看到类似 `delta power = 4.5000` 就说明原生 DLL 已自动复制并成功加载。

## API 一览

| 方法 | 作用 |
|------|------|
| `EegAlgClient.Version` | 库版本字符串 |
| `WelchPsd(signal, fs, nperseg)` | 功率谱密度（返回频率与功率数组） |
| `BandPower(freqs, psd, fLow, fHigh)` | 某频带的功率 |
| `AbsolutePower(data, fs, apf, nperseg)` | 绝对功率，14 个特征 × 通道 |
| `AbsolutePowerIndiv(data, fs, nperseg)` | 逐通道独立估计 APF |
| `ShamPower(data, fs, rdnFreqPnt, nperseg)` | 伪刺激功率 |
| `PhaseLocking(epochs, fs, apf)` | 跨试次相位一致性（ITPC），8 频带 |
| `Pac(data, fs, apf, numBins)` | 相位-振幅耦合（Tort MI），8 模式 |

## 数据约定

- 输入矩阵 `double[,]` 的形状是 **(采样点 × 通道)**。
- `PhaseLocking` 的输入 `double[,,]` 形状是 **(试次 × 采样点 × 通道)**。
- 特征结果用枚举索引，例如：

```csharp
var res = EegAlgClient.AbsolutePower(data, fs: 1024, apf: 10.0);
double loAlpha = res[AbsolutePowerFeature.LoAlpha, 0];  // 第 0 通道的低 alpha
double apf = res.GetApf(0);                            // 第 0 通道的 APF
```

## 常见问题

- **运行时 `DllNotFoundException`**：确认在 Windows x64 上运行，且包版本正确。
- **想重新打包**：先构建原生 DLL（见仓库 `scripts/build_windows_msvc.bat`），再执行 `dotnet pack csharp/EegAlg/EegAlg.csproj -c Release -o artifacts/nuget`。
