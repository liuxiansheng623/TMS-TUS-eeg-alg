#ifndef EEG_ALG_EXPORT_H
#define EEG_ALG_EXPORT_H

// 跨平台导出宏。
// 构建库时定义 EEG_ALG_BUILDING_LIBRARY，消费库时不定义。
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef EEG_ALG_BUILDING_LIBRARY
    #define EEG_ALG_API __declspec(dllexport)
  #else
    #define EEG_ALG_API __declspec(dllimport)
  #endif
  #define EEG_ALG_LOCAL
#else
  #define EEG_ALG_API __attribute__((visibility("default")))
  #define EEG_ALG_LOCAL __attribute__((visibility("hidden")))
#endif

#endif // EEG_ALG_EXPORT_H
