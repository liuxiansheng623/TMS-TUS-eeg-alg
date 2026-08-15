#ifndef EEG_ALG_CAPI_H
#define EEG_ALG_CAPI_H

#include "eeg_alg_export.h"

// 错误码约定：0 = 成功；负值 = 失败。
#define EEG_ALG_OK 0
#define EEG_ALG_ERR_INVALID_ARG (-1)
#define EEG_ALG_ERR_INVALID_SAMPLING_RATE (-2)

#ifdef __cplusplus
extern "C" {
#endif

// 稳定 C ABI 入口：返回库 ABI 版本字符串。
EEG_ALG_API const char* eeg_alg_abi_version(void);

// DSP 原语。
// welch_psd：输出 freqs_out / psd_out（长度 nfft/2 + 1），实际长度写入 *n_out。
EEG_ALG_API int eeg_alg_welch_psd(const double* signal, int n, double fs, int nperseg,
                                   double* freqs_out, double* psd_out, int* n_out);
EEG_ALG_API double eeg_alg_band_power(const double* freqs, const double* psd, int n,
                                      double f_low, double f_high);

// 算法入口：输入 sample-major（data[sample * num_channels + channel]）；
// 输出 feature-major（features_out[feature * num_channels + channel]），由调用方预分配。
EEG_ALG_API int eeg_alg_absolute_power(const double* data, int num_samples, int num_channels,
                                       double fs, double apf, int nperseg, double* features_out);
EEG_ALG_API int eeg_alg_absolute_power_indiv(const double* data, int num_samples, int num_channels,
                                             double fs, int nperseg, double* features_out);
EEG_ALG_API int eeg_alg_sham_power(const double* data, int num_samples, int num_channels,
                                   double fs, double rdn_freq_pnt, int nperseg, double* features_out);
// epochs：trial-major 扁平数组（num_trials × num_samples × num_channels）。
EEG_ALG_API int eeg_alg_phase_locking(const double* epochs, int num_trials, int num_samples,
                                      int num_channels, double fs, double apf, double* features_out);
EEG_ALG_API int eeg_alg_pac(const double* data, int num_samples, int num_channels,
                            double fs, double apf, int num_bins, double* features_out);

#ifdef __cplusplus
}
#endif

#endif // EEG_ALG_CAPI_H
