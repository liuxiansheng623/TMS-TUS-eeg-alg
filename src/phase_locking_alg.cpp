/*相位锁定算法实现（试次间相位一致性 ITPC）*/
#include "phase_locking_alg.h"
#include "dsp_utils.h"

#include <vector>
#include <tuple>
#include <complex>
#include <cmath>

namespace phase_locking {

namespace {

constexpr double PI = 3.14159265358979323846;
// 触发 OpenMP 并行的最小工作量（通道×采样点×试次）；低于此值串行执行更划算。
constexpr long kOmpMinWork = 1L << 17;
using BandDef = std::tuple<int, double, double>;   // (特征索引, 下界Hz, 上界Hz)

/**
 * @brief 根据 APF 构造单通道 ITPC 使用的 8 个分析频带。
 * @param apf 输入：Alpha 峰值频率，单位 Hz。
 * @return 输出：由“特征索引、频带下界、频带上界”组成的频带列表。
 */
std::vector<BandDef> make_bands(double apf) {
    return {
        {DELTA,    1.0,       4.0},
        {THETA,    4.0,       apf - 2.0},
        {LO_ALPHA, apf - 2.0, apf},
        {HI_ALPHA, apf,       apf + 2.0},
        {ALPHA,    apf - 2.0, apf + 2.0},
        {LO_BETA,  apf + 2.0, 20.0},
        {BETA,     20.0,      30.0},
        {GAMMA,    30.0,      45.0},
    };
}

} // namespace

/**
 * @brief 逐通道、逐频带计算跨试次相位一致性 ITPC。
 * @param epochs 输入：多个试次的 EEG 矩阵；每个矩阵均为“采样点×通道”。
 * @param fs 输入：采样率，单位 Hz。
 * @param apf 输入：用于确定个体化频带边界的 Alpha 峰值频率，单位 Hz。
 * @return 输出：8 个频带乘通道数的 ITPC 结果；空试次列表返回零通道结果。
 */
EEG_Phase_Locking_SoA compute(const std::vector<Eigen::MatrixXd>& epochs,
                              double fs, double apf) {
    if (epochs.empty()) return EEG_Phase_Locking_SoA(0);

    const int num_channels = static_cast<int>(epochs[0].cols());
    const int num_samples  = static_cast<int>(epochs[0].rows());
    const int T            = static_cast<int>(epochs.size());
    EEG_Phase_Locking_SoA result(num_channels);

    const auto bands = make_bands(apf);

    // 并行前预热 plan（bandpass_analytic 用 r2c + c2c 逆变换，尺寸由 epoch 长度决定）
    const int nfft = dsp::next_power_of_2(num_samples);
    dsp::warmup_r2c(nfft);
    dsp::warmup_c2c_backward(nfft);

    // 各通道相互独立，写不同的 result.features[*][ch]，无数据竞争；
    // 仅当工作量（通道×采样点×试次）足够大时才并行，避免小输入下线程开销超过收益。
    const long work = static_cast<long>(num_channels) * num_samples * T;
    #pragma omp parallel for if(work >= kOmpMinWork)
    for (int ch = 0; ch < num_channels; ++ch) {
        for (const auto& [idx, lo, hi] : bands) {
            // 每个时间点跨 trial 的复指数累加：Σ exp(i·φ)
            Eigen::VectorXcd sum = Eigen::VectorXcd::Zero(num_samples);
            for (int t = 0; t < T; ++t) {
                const Eigen::VectorXd x = epochs[t].col(ch);
                // 带通 + Hilbert 合并：一次正/逆 FFT 直接得带通信号的瞬时相位
                auto analytic = dsp::bandpass_analytic(x, fs, lo, hi);
                const Eigen::VectorXd& phase = analytic.first;
                for (int n = 0; n < num_samples; ++n)
                    sum[n] += std::polar(1.0, phase[n]);   // exp(i·φ)
            }
            // ITPC = 时间平均的 |跨trial平均复向量|
            double itpc = 0.0;
            for (int n = 0; n < num_samples; ++n)
                itpc += std::abs(sum[n] / static_cast<double>(T));
            itpc /= static_cast<double>(num_samples);
            result.features[idx][ch] = itpc;
        }
    }
    return result;
}

} // namespace phase_locking
