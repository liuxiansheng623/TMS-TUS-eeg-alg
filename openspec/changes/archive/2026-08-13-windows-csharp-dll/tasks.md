## 1. C ABI 门面

- [x] 1.1 扩展 `include/eeg_alg_capi.h`：声明版本、`welch_psd`、`band_power`、`absolute_power`、`absolute_power_indiv`、`sham_power`、`phase_locking`、`pac` 的 C 兼容入口
- [x] 1.2 在 `src/eeg_alg_capi.cpp` 实现包装：原始数组 ↔ Eigen 编组、调用 C++ API、写入调用方预分配输出、返回错误码

## 2. Windows DLL 构建

- [x] 2.1 `CMakeLists.txt` 增加 Windows 分支的 Eigen3/FFTW3 查找（经缓存变量指定路径）、GTest 可选、MSVC 编译选项 `/utf-8 /W4 /O2`
- [x] 2.2 确保 `EEG_ALG_BUILDING_LIBRARY` 生效，产出 `eeg_alg.dll`

## 3. 安装 Windows 依赖

- [x] 3.1 从 GitLab 下载 Eigen 3.4.0 头文件（GitHub 被墙，vcpkg bootstrap 失败，改用直达源）
- [x] 3.2 从 fftw.org 下载 FFTW 3.3.10 源码，用 MSVC + CMake/Ninja 编译为 `fftw3.dll`/`fftw3.lib`

## 4. C# P/Invoke 测试

- [x] 4.1 新增 `tests/csharp/` 项目，声明 `[DllImport]` 入口
- [x] 4.2 编写测试：`eeg_alg_abi_version` + `eeg_alg_welch_psd` + `eeg_alg_band_power` + `eeg_alg_absolute_power`，断言结果与 C++ 参考一致

## 5. Windows 构建与验证

- [x] 5.1 用 MSVC（vcvars64 + Ninja）构建 `eeg_alg.dll`
- [x] 5.2 运行 C# 测试并确认全部通过
