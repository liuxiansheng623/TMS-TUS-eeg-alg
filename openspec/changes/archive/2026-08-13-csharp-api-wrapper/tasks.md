## 1. 类库骨架

- [x] 1.1 新增 `csharp/EegAlg/EegAlg.csproj`（net8.0、x64 类库）
- [x] 1.2 新增 `csharp/EegAlg.Tests/EegAlg.Tests.csproj`（引用 EegAlg 的控制台测试）

## 2. 封装实现

- [x] 2.1 `Native.cs`：8 个 `[DllImport]` 声明（字符串用 `IntPtr`）
- [x] 2.2 `Features.cs`：特征/频带/耦合模式枚举与特征数常量
- [x] 2.3 `Results.cs`：WelchPsdResult 与各算法强类型结果
- [x] 2.4 `EegAlgException.cs`：异常与错误码
- [x] 2.5 `EegAlgClient.cs`：公共静态 API + 展平/编组 + 异常映射

## 3. 测试与验证

- [x] 3.1 测试覆盖全部 8 个入口并校验数值结果
- [x] 3.2 更新 `build_windows_msvc.bat` 构建并运行新测试
- [x] 3.3 在 Windows 运行测试并确认全部通过
