cmake_minimum_required(VERSION 3.14)
project(TMS_TUS_alg VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC 默认按 GBK 解析源码，UTF-8 中文注释会破坏预处理；显式按 UTF-8 解析。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 共享库开关：ON 产出 libeeg_alg.so，OFF（默认）产出静态库 libeeg_alg.a
option(BUILD_SHARED_LIBS "Build eeg_alg as a shared library" OFF)

# ───────────────────────── 依赖 ─────────────────────────
if(WIN32)
    # Windows：依赖由外部提供（Eigen 头文件源码 + FFTW 从源码构建），经缓存变量指定路径。
    foreach(_dep EIGEN3_INCLUDE_DIR FFTW3_INCLUDE_DIR FFTW3_LIBRARY)
        if(NOT DEFINED ${_dep})
            message(FATAL_ERROR "Windows build requires -D${_dep}=<path>")
        endif()
    endforeach()
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
    set(FFTW3_INCLUDE_DIRS "${FFTW3_INCLUDE_DIR}")
    set(FFTW3_LIBRARIES "${FFTW3_LIBRARY}")
    set(FFTW3_LIBRARY_DIRS "")
else()
    # Eigen3（头文件库）
    find_package(Eigen3 3.3 REQUIRED NO_MODULE)
    # FFTW3（Ubuntu 无官方 CMake config，用 pkg-config 查找）
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED fftw3)
endif()

# OpenMP（多通道并行）
find_package(OpenMP REQUIRED)

# ───────────────────── 算法库 eeg_alg ─────────────────────
add_library(eeg_alg
    src/dsp_utils.cpp
    src/absolute_power_alg.cpp
    src/sham_power_alg.cpp
    src/phase_locking_alg.cpp
    src/pac_asymmetry_alg.cpp
    src/eeg_alg_capi.cpp
)

target_include_directories(eeg_alg
    PUBLIC
        ${CMAKE_SOURCE_DIR}/struct     # 特征输出结构体
        ${CMAKE_SOURCE_DIR}/include    # 算法声明 + dsp_utils
    PRIVATE
        ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(eeg_alg
    PUBLIC
        Eigen3::Eigen
        OpenMP::OpenMP_CXX            # 多通道并行（同时提供 -fopenmp 编译选项）
    PRIVATE
        ${FFTW3_LIBRARIES}             # -lfftw3
)

target_link_directories(eeg_alg PRIVATE ${FFTW3_LIBRARY_DIRS})

if(MSVC)
    target_compile_options(eeg_alg PRIVATE /W4 /O2)
else()
    target_compile_options(eeg_alg PRIVATE -Wall -Wextra -O2)
endif()

# 受控符号可见性：仅导出 EEG_ALG_API 标注的符号
set_target_properties(eeg_alg PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
target_compile_definitions(eeg_alg PRIVATE EEG_ALG_BUILDING_LIBRARY)

# ─────────────────── 示例 demo（原 test.cpp）───────────────────
add_executable(test_demo src/test.cpp)
target_link_libraries(test_demo PRIVATE eeg_alg)
set_target_properties(test_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ─────────────────── GoogleTest 单元测试（可选）───────────────────
find_package(GTest QUIET)
if(GTest_FOUND AND NOT WIN32)
    enable_testing()

    add_executable(test_algorithms tests/test_algorithms.cpp)
    target_link_libraries(test_algorithms
        PRIVATE
            eeg_alg
            GTest::gtest_main
    )
    target_compile_options(test_algorithms PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>)
    set_target_properties(test_algorithms PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    include(GoogleTest)
    gtest_discover_tests(test_algorithms)

    # 共享库冒烟测试：仅 Linux（dlopen/dlsym）+ 共享库构建
    if(UNIX AND BUILD_SHARED_LIBS)
        add_executable(test_shared_library tests/test_shared_library.cpp)
        target_compile_definitions(test_shared_library PRIVATE
            EEG_ALG_SO_PATH="$<TARGET_FILE:eeg_alg>"
        )
        target_link_libraries(test_shared_library
            PRIVATE
                GTest::gtest_main
                ${CMAKE_DL_LIBS}
        )
        target_compile_options(test_shared_library PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>)
        set_target_properties(test_shared_library PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        gtest_discover_tests(test_shared_library)
    endif()
endif()
