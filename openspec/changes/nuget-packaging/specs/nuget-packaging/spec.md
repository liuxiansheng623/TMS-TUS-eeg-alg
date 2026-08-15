## ADDED Requirements

### Requirement: 打包元数据

`EegAlg` 项目 SHALL 定义 `PackageId`、`Version`、`Description` 等 NuGet 打包元数据。

#### Scenario: 打包信息完整
- **WHEN** 执行 `dotnet pack`
- **THEN** 产出的包包含 `EegAlg` 包 ID 与语义化版本号

### Requirement: 原生二进制随包

包 SHALL 在 `runtimes/win-x64/native/` 下包含 `eeg_alg.dll` 与 `fftw3.dll`。

#### Scenario: 包内包含原生 DLL
- **WHEN** 解包 `.nupkg`
- **THEN** `runtimes/win-x64/native/eeg_alg.dll` 与 `fftw3.dll` 存在

### Requirement: 消费方自动复制

消费方在 Windows 构建或运行 SHALL 自动把原生 DLL 复制到输出目录，无需手动拷贝。

#### Scenario: dotnet run 可找到 DLL
- **WHEN** 消费方 `dotnet add package EegAlg` 后执行 `dotnet run`
- **THEN** P/Invoke 成功加载 `eeg_alg` 且无 `DllNotFoundException`

### Requirement: 教程文档

项目 SHALL 提供新手使用教程，覆盖本地源配置、安装引用、最小代码示例。

#### Scenario: 教程可照着跑通
- **WHEN** 新手按教程步骤操作
- **THEN** 能成功引用并调用 `EegAlgClient`
