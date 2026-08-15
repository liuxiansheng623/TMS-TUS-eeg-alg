## Why

C# 封装目前只能源码引用，无法像普通 .NET 库一样分发；手动拷贝原生 DLL 也易错。打成 NuGet 包并携带原生二进制，消费者 `dotnet add package` 即可用。

## What Changes

- 配置 `EegAlg.csproj` 的打包元数据（PackageId、Version、描述）。
- 将原生二进制（`eeg_alg.dll`、`fftw3.dll`）收集到 `runtimes/win-x64/native/` 随包分发。
- 新增 `buildTransitive/EegAlg.targets`，使消费方 `dotnet run`/`build` 时自动复制原生 DLL。
- `dotnet pack` 产出 `.nupkg`，并用本地 NuGet 源验证消费。
- 提供新手使用教程。

## Capabilities

### New Capabilities
- `nuget-packaging`: 将 `EegAlg` 封装打包为带原生二进制的 NuGet 包。

### Modified Capabilities

（无。仅新增打包与文档，不改动 `csharp-api` 与算法。）

## Impact

- `csharp/EegAlg/EegAlg.csproj`、`csharp/EegAlg/runtimes/`、`csharp/EegAlg/buildTransitive/`。
- `csharp/README.md`（使用教程）。
- 打包/验证脚本。
