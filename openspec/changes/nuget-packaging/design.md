## Context

已有 `csharp/EegAlg` 类库与 `csharp/EegAlg.Tests` 测试，原生 `eeg_alg.dll`/`fftw3.dll` 已在 Windows 构建。现需打包分发。

## Goals / Non-Goals

**Goals:** 产出 `EegAlg` NuGet 包，含原生二进制，消费方开箱即用。

**Non-Goals:** 本次不做 linux-x64 原生二进制、不做私有源发布、不做 AOT/COM。

## Decisions

- 用 csproj 内嵌打包属性（`PackageId=EegAlg`、`Version=1.0.0`）+ `<None Pack=true PackagePath>` 打包 `runtimes/`。
- 用 `runtimes/win-x64/native/` 标准目录约定携带原生 DLL。
- 用 `buildTransitive/EegAlg.targets` 在消费方构建时复制原生 DLL（解决无 RID 时 `dotnet run` 找不到 DLL）。
- `dotnet pack -c Release -o <out>` 产出包，本地文件夹作为 NuGet 源验证。

## Risks / Trade-offs

- [无 RID 的 `dotnet run` 不自动复制 runtimes] → 用 buildTransitive targets 兜底。
- [linux-x64 未打包] → 教程注明后续扩展。

## Migration Plan

纯新增。回滚即删除打包配置与产物。

## Open Questions

- 后续是否发布到 nuget.org 或私有源。
