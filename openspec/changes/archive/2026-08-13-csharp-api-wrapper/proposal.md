## Why

当前 C# 侧仅有裸 P/Invoke 声明（且只覆盖 4 个入口），数据编组、错误处理都由调用方手写，缺乏类型安全与地道 .NET 体验。为让 C# 应用能方便、安全地调用算法库，需提供一层完整封装。

## What Changes

- 新增 C# 类库 `csharp/EegAlg`，封装全部 8 个 C ABI 入口（`abi_version`、`welch_psd`、`band_power`、`absolute_power`、`absolute_power_indiv`、`sham_power`、`phase_locking`、`pac`）。
- 提供地道类型：特征/频带/耦合模式枚举、结果类型、`EegAlgException` 异常。
- 正确编组：字符串用 `IntPtr`（规避 `LPUTF8Str` 返回编组堆损坏）；输入 `double[,]`/`double[,,]` 展平为 sample-major；输出 feature-major 映射为强类型访问。
- 新增测试项目，覆盖全部 8 个入口。

## Capabilities

### New Capabilities
- `csharp-api`: 基于 C ABI 的、类型安全的 C# 封装 API。

### Modified Capabilities

（无。不改动 `c-api` 与各算法能力，仅在其上新增 C# 封装层。）

## Impact

- `csharp/EegAlg/`（新增）：类库与封装代码。
- `csharp/EegAlg.Tests/`（新增）：测试项目。
- `scripts/build_windows_msvc.bat`：改为构建/运行新测试项目。
