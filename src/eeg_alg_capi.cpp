#include "eeg_alg_capi.h"

#include <Eigen/Dense>

#include <stdexcept>
#include <vector>

#include "absolute_power_alg.h"
#include "dsp_utils.h"
#include "pac_asymmetry_alg.h"
#include "phase_locking_alg.h"
#include "sham_power_alg.h"

namespace {

Eigen::MatrixXd to_matrix(const double* data, int num_samples, int num_channels) {
    Eigen::MatrixXd m(num_samples, num_channels);
    for (int s = 0; s < num_samples; ++s) {
        for (int ch = 0; ch < num_channels; ++ch) {
            m(s, ch) = data[static_cast<size_t>(s) * num_channels + ch];
        }
    }
    return m;
}

template <typename SoA>
void write_features(const SoA& res, int num_channels, double* out) {
    const int nf = static_cast<int>(res.features.size());
    for (int f = 0; f < nf; ++f) {
        for (int ch = 0; ch < num_channels; ++ch) {
            out[static_cast<size_t>(f) * num_channels + ch] = res.features[f][ch];
        }
    }
}

} // namespace

extern "C" {

EEG_ALG_API const char* eeg_alg_abi_version(void) {
    return "eeg_alg-1.0.0";
}

EEG_ALG_API int eeg_alg_welch_psd(const double* signal, int n, double fs, int nperseg,
                                   double* freqs_out, double* psd_out, int* n_out) {
    if (!signal || n <= 0 || !freqs_out || !psd_out || !n_out) return EEG_ALG_ERR_INVALID_ARG;
    Eigen::VectorXd x(n);
    for (int i = 0; i < n; ++i) x[i] = signal[i];
    auto spec = dsp::welch_psd(x, fs, nperseg);
    const int m = static_cast<int>(spec.first.size());
    for (int i = 0; i < m; ++i) {
        freqs_out[i] = spec.first[i];
        psd_out[i] = spec.second[i];
    }
    *n_out = m;
    return EEG_ALG_OK;
}

EEG_ALG_API double eeg_alg_band_power(const double* freqs, const double* psd, int n,
                                      double f_low, double f_high) {
    if (!freqs || !psd || n <= 0) return 0.0;
    Eigen::VectorXd f(n), p(n);
    for (int i = 0; i < n; ++i) {
        f[i] = freqs[i];
        p[i] = psd[i];
    }
    return dsp::band_power(f, p, f_low, f_high);
}

EEG_ALG_API int eeg_alg_absolute_power(const double* data, int num_samples, int num_channels,
                                       double fs, double apf, int nperseg, double* features_out) {
    if (!data || num_samples <= 0 || num_channels <= 0 || !features_out) return EEG_ALG_ERR_INVALID_ARG;
    try {
        auto m = to_matrix(data, num_samples, num_channels);
        auto res = absolute_power::compute(m, fs, apf, nperseg);
        write_features(res, num_channels, features_out);
        return EEG_ALG_OK;
    } catch (const std::invalid_argument&) {
        return EEG_ALG_ERR_INVALID_SAMPLING_RATE;
    } catch (...) {
        return EEG_ALG_ERR_INVALID_ARG;
    }
}

EEG_ALG_API int eeg_alg_absolute_power_indiv(const double* data, int num_samples, int num_channels,
                                             double fs, int nperseg, double* features_out) {
    if (!data || num_samples <= 0 || num_channels <= 0 || !features_out) return EEG_ALG_ERR_INVALID_ARG;
    try {
        auto m = to_matrix(data, num_samples, num_channels);
        auto res = absolute_power::compute_indiv(m, fs, nperseg);
        write_features(res, num_channels, features_out);
        return EEG_ALG_OK;
    } catch (const std::invalid_argument&) {
        return EEG_ALG_ERR_INVALID_SAMPLING_RATE;
    } catch (...) {
        return EEG_ALG_ERR_INVALID_ARG;
    }
}

EEG_ALG_API int eeg_alg_sham_power(const double* data, int num_samples, int num_channels,
                                   double fs, double rdn_freq_pnt, int nperseg, double* features_out) {
    if (!data || num_samples <= 0 || num_channels <= 0 || !features_out) return EEG_ALG_ERR_INVALID_ARG;
    try {
        auto m = to_matrix(data, num_samples, num_channels);
        auto res = sham_power::compute(m, fs, rdn_freq_pnt, nperseg);
        write_features(res, num_channels, features_out);
        return EEG_ALG_OK;
    } catch (...) {
        return EEG_ALG_ERR_INVALID_ARG;
    }
}

EEG_ALG_API int eeg_alg_phase_locking(const double* epochs, int num_trials, int num_samples,
                                      int num_channels, double fs, double apf, double* features_out) {
    if (!epochs || num_trials <= 0 || num_samples <= 0 || num_channels <= 0 || !features_out)
        return EEG_ALG_ERR_INVALID_ARG;
    std::vector<Eigen::MatrixXd> trials(num_trials);
    for (int t = 0; t < num_trials; ++t) {
        trials[t] = Eigen::MatrixXd(num_samples, num_channels);
        for (int s = 0; s < num_samples; ++s) {
            for (int ch = 0; ch < num_channels; ++ch) {
                const size_t idx = (static_cast<size_t>(t) * num_samples + s) * num_channels + ch;
                trials[t](s, ch) = epochs[idx];
            }
        }
    }
    auto res = phase_locking::compute(trials, fs, apf);
    write_features(res, num_channels, features_out);
    return EEG_ALG_OK;
}

EEG_ALG_API int eeg_alg_pac(const double* data, int num_samples, int num_channels,
                            double fs, double apf, int num_bins, double* features_out) {
    if (!data || num_samples <= 0 || num_channels <= 0 || !features_out) return EEG_ALG_ERR_INVALID_ARG;
    try {
        auto m = to_matrix(data, num_samples, num_channels);
        auto res = pac_asymmetry::compute(m, fs, apf, num_bins);
        write_features(res, num_channels, features_out);
        return EEG_ALG_OK;
    } catch (...) {
        return EEG_ALG_ERR_INVALID_ARG;
    }
}

} // extern "C"
