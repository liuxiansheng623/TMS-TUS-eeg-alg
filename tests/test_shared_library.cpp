// 共享库冒烟测试：验证 libeeg_alg.so 可被 dlopen 加载，导出的 C ABI 符号可被 dlsym 解析。
#include <gtest/gtest.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <string>
#endif

#ifndef EEG_ALG_SO_PATH
#error "EEG_ALG_SO_PATH must be defined by the build system"
#endif

TEST(SharedLibrary, DlopenDlsymAbiVersion) {
#ifdef _WIN32
    GTEST_SKIP() << "Windows DLL smoke test 属于后续 change";
#else
    void* handle = dlopen(EEG_ALG_SO_PATH, RTLD_NOW | RTLD_LOCAL);
    ASSERT_NE(handle, nullptr) << dlerror();
    dlerror();  // 清除残留错误

    using AbiVersionFn = const char* (*)();
    auto* fn = reinterpret_cast<AbiVersionFn>(
        dlsym(handle, "eeg_alg_abi_version"));
    ASSERT_NE(fn, nullptr) << dlerror();

    const char* ver = fn();
    ASSERT_NE(ver, nullptr);
    ASSERT_GT(std::string(ver).size(), 0u);

    ASSERT_EQ(dlclose(handle), 0);
#endif
}
