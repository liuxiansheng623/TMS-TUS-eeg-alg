## 1. 打包配置

- [x] 1.1 配置 `EegAlg.csproj` 打包元数据与 `runtimes/` 打包
- [x] 1.2 新增 `buildTransitive/EegAlg.targets` 复制原生 DLL

## 2. 原生二进制

- [x] 2.1 收集 `eeg_alg.dll`、`fftw3.dll` 到 `runtimes/win-x64/native/`

## 3. 打包与验证

- [x] 3.1 `dotnet pack` 产出 `.nupkg`
- [x] 3.2 新建消费者工程，用本地源安装并 `dotnet run` 验证

## 4. 教程

- [x] 4.1 编写 `csharp/README.md` 新手使用教程
