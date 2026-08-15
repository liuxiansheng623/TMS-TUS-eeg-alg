## Context

已有 8 个 C ABI 入口与一个裸 P/Invoke 冒烟测试。现需在其上提供类型安全的 C# 封装，覆盖全部入口，并规避已发现的编组陷阱（`LPUTF8Str` 返回编组堆损坏）。

## Goals / Non-Goals

**Goals:**
- 提供静态类 `EegAlgClient`，封装全部 8 个入口。
- 定义枚举、结果类型、异常。
- 正确处理字符串与矩阵编组。

**Non-Goals:**
- 不改动 C ABI 与算法行为。
- 不引入第三方 NuGet 依赖（避免联网还原）。

## Decisions

### Decision 1: internal `Native` + public `EegAlgClient`

将 P/Invoke 声明集中在 `internal static class Native`，公共 API 只暴露 `EegAlgClient`，避免调用方直接接触原始指针与错误码。

### Decision 2: 字符串用 `IntPtr`

`const char*` 返回用 `IntPtr` 接收，再 `Marshal.PtrToStringUTF8`。已证明 `[return: MarshalAs(LPUTF8Str)]` 在 .NET 8 下触发堆损坏。

### Decision 3: 输入矩阵展平为 sample-major

公共 API 接受 `double[,]`（sample × channel）或 `double[,,]`（trial × sample × channel），内部展平为 `double[]`（sample-major）再传给 Native。

### Decision 4: 输出 feature-major → 强类型结果

结果类型内部存 `double[,]`（feature × channel），提供 `this[feature, channel]` 与语义化访问（如 `GetApf(channel)`）。

### Decision 5: 异常映射

`0` → 成功；`-1` → `EegAlgException("invalid argument")`；`-2` → `EegAlgException("invalid sampling rate")`。

## Risks / Trade-offs

- [矩阵展平有 O(n) 开销] → 对 EEG 规模可接受，换取地道 API。
- [枚举索引与 C++ 需保持同步] → 测试断言索引值一致。

## Migration Plan

纯新增。现有 `tests/csharp` 冒烟测试保留，新增 `csharp/EegAlg` 与 `csharp/EegAlg.Tests`。

## Open Questions

- 后续是否打包为 NuGet（部署期决定）。
