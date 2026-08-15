## 1. 导出宏与 C ABI 种子

- [x] 1.1 新增 `include/eeg_alg_export.h`，定义 `EEG_ALG_API`（Linux `visibility("default")`，Windows `dllexport/dllimport`）与 `EEG_ALG_BUILDING_LIBRARY` 区分宏
- [x] 1.2 新增 `include/eeg_alg_capi.h` 与 `src/eeg_alg_capi.cpp`，提供 `extern "C" const char* eeg_alg_abi_version()`

## 2. 标注公开 API

- [x] 2.1 在 `include/dsp_utils.h` 的全部公开函数声明前标注 `EEG_ALG_API`
- [x] 2.2 在 `include/absolute_power_alg.h` 的 `compute` / `compute_indiv` 标注 `EEG_ALG_API`
- [x] 2.3 在 `include/sham_power_alg.h`、`include/phase_locking_alg.h`、`include/pac_asymmetry_alg.h` 的 `compute` 与 `detail::tort_mi_from_phase_amplitude` 标注 `EEG_ALG_API`

## 3. CMake 共享库构建

- [x] 3.1 在 `CMakeLists.txt` 增加 `option(BUILD_SHARED_LIBS ...)` 并将 `add_library(eeg_alg STATIC ...)` 改为可响应该选项的 `add_library(eeg_alg ...)`
- [x] 3.2 为 `eeg_alg` 设置 `CXX_VISIBILITY_PRESET hidden` 与 `VISIBILITY_INLINES_HIDDEN YES`
- [x] 3.3 对 `eeg_alg` 目标 `target_compile_definitions(... PRIVATE EEG_ALG_BUILDING_LIBRARY)`，并将 `src/eeg_alg_capi.cpp` 加入库源文件

## 4. 测试用例

- [x] 4.1 新增 `tests/test_shared_library.cpp`：`dlopen("libeeg_alg.so")` + `dlsym("eeg_alg_abi_version")` 并校验返回非空字符串
- [x] 4.2 在 `CMakeLists.txt` 中注册 `test_shared_library` 目标并链接 `eeg_alg` 与 `dl`

## 5. WSL 构建与验证

- [x] 5.1 新增 `scripts/build_and_test_wsl.sh`：`BUILD_SHARED_LIBS=ON` 配置、构建、`ctest`、`nm -D` 符号校验
- [x] 5.2 在 Ubuntu-22.04 WSL 中运行脚本，确认 `.so` 产物、全部测试通过、导出符号受控
