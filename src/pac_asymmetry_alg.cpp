/*相位-振幅耦合(PAC)算法实现（Tort 调制指数，逐通道）
对每个通道计算“低频相位调节高频振幅”的耦合强度(MI)。*/
#include "pac_asymmetry_alg.h"
#include "dsp_utils.h"

#include <vector>
#include <tuple>
#include <cmath>
#include <limits>

namespace pac_asymmetry {

namespace detail {

/**
 * @brief 根据瞬时相位和瞬时振幅序列计算 Tort 调制指数。
 * @param phase 输入：瞬时低频相位序列，单位弧度，通常位于 [-π,π]。
 * @param amplitude 输入：与 phase 等长的高频瞬时振幅序列。
 * @param num_bins 输入：相位分箱数量，要求至少为 2。
 * @return 输出：Tort MI；输入非法或相位箱覆盖不完整时返回 NaN。
 */
double tort_mi_from_phase_amplitude(const Eigen::VectorXd& phase,
                                    const Eigen::VectorXd& amplitude,
                                    int num_bins) {
    if (phase.size() != amplitude.size() || phase.size() == 0 || num_bins < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    constexpr double PI = 3.14159265358979323846;
    std::vector<double> bin_sum(static_cast<size_t>(num_bins), 0.0);
    std::vector<int> bin_count(static_cast<size_t>(num_bins), 0);
    const int n = static_cast<int>(phase.size());
    for (int i = 0; i < n; ++i) {
        int b = static_cast<int>(std::floor((phase[i] + PI) / (2.0 * PI) * num_bins));
        if (b < 0) b = 0;
        if (b >= num_bins) b = num_bins - 1;
        bin_sum[static_cast<size_t>(b)] += amplitude[i];
        ++bin_count[static_cast<size_t>(b)];
    }

    // Tort MI 使用每个相位箱内的平均振幅，而不是振幅总和。
    // 若某个相位箱没有样本，则无法构造完整的相位-振幅分布。
    std::vector<double> bin_mean(static_cast<size_t>(num_bins), 0.0);
    double total = 0.0;
    for (int j = 0; j < num_bins; ++j) {
        const int count = bin_count[static_cast<size_t>(j)];
        if (count == 0) return std::numeric_limits<double>::quiet_NaN();
        bin_mean[static_cast<size_t>(j)] =
            bin_sum[static_cast<size_t>(j)] / static_cast<double>(count);
        total += bin_mean[static_cast<size_t>(j)];
    }
    if (total < 1e-12) return 0.0;

    double H = 0.0;
    for (int j = 0; j < num_bins; ++j) {
        const double p = bin_mean[static_cast<size_t>(j)] / total;
        if (p > 0.0) H -= p * std::log(p);
    }
    const double Hmax = std::log(static_cast<double>(num_bins));
    return (Hmax - H) / Hmax;
}

} // namespace detail

namespace {

// 触发 OpenMP 并行的最小工作量（通道数×采样点）；低于此值串行执行更划算。
constexpr long kOmpMinWork = 1L << 17;

// 8 个耦合模式：(特征索引, 相位带下界, 相位带上界, 振幅带下界, 振幅带上界)
using ComboDef = std::tuple<int, double, double, double, double>;

/**
 * @brief 根据 APF 构造单通道 PAC 使用的 8 组“相位频带×振幅频带”。
 * @param apf 输入：Alpha 峰值频率，单位 Hz。
 * @return 输出：由特征索引和两组频带上下界组成的耦合模式列表。
 */
std::vector<ComboDef> make_combos(double apf) {
    return {
        {DELTA_LO_GAMMA, 1.0,       4.0,       30.0, 45.0},
        {DELTA_HI_GAMMA, 1.0,       4.0,       45.0, 70.0},
        {THETA_LO_GAMMA, 4.0,       apf - 2.0, 30.0, 45.0},
        {THETA_HI_GAMMA, 4.0,       apf - 2.0, 45.0, 70.0},
        {ALPHA_LO_GAMMA, apf - 2.0, apf + 2.0, 30.0, 45.0},
        {ALPHA_HI_GAMMA, apf - 2.0, apf + 2.0, 45.0, 70.0},
        {BETA_LO_GAMMA,  apf + 2.0, 20.0,      30.0, 45.0},
        {BETA_HI_GAMMA,  apf + 2.0, 20.0,      45.0, 70.0},
    };
}

/**
 * @brief 计算一个通道在一组低频相位带和高频振幅带上的 Tort MI。
 * @param x 输入：单通道时域信号。
 * @param fs 输入：采样率，单位 Hz。
 * @param phase_lo 输入：相位频带下界，单位 Hz。
 * @param phase_hi 输入：相位频带上界，单位 Hz。
 * @param amp_lo 输入：振幅频带下界，单位 Hz。
 * @param amp_hi 输入：振幅频带上界，单位 Hz。
 * @param num_bins 输入：相位分箱数量。
 * @return 输出：指定频带组合的 Tort MI；估计条件不足时可能返回 NaN。
 */
double modulation_index(const Eigen::VectorXd& x, double fs,
                        double phase_lo, double phase_hi,
                        double amp_lo, double amp_hi, int num_bins) {
    // 低频成分 → 瞬时相位（带通 + Hilbert 合并，一次正/逆 FFT）
    auto phase_res = dsp::bandpass_analytic(x, fs, phase_lo, phase_hi);
    const Eigen::VectorXd& phase = phase_res.first;
    // 高频成分 → 瞬时振幅（带通 + Hilbert 合并，一次正/逆 FFT）
    auto amp_res = dsp::bandpass_analytic(x, fs, amp_lo, amp_hi);
    const Eigen::VectorXd& amp = amp_res.second;

    return detail::tort_mi_from_phase_amplitude(phase, amp, num_bins);
}

} // namespace

/**
 * @brief 逐通道计算 8 种低频相位—高频振幅耦合的 Tort MI。
 * @param data 输入：连续 EEG 矩阵，行为采样点、列为通道。
 * @param fs 输入：采样率，单位 Hz。
 * @param apf 输入：用于确定 Theta/Alpha/Beta 边界的 Alpha 峰值频率，单位 Hz。
 * @param num_bins 输入：相位分箱数量，小于 2 时自动调整为 2。
 * @return 输出：8 种耦合模式乘通道数的单通道 PAC 结果。
 */
EEG_PAC_Asymmetry_SoA compute(const Eigen::MatrixXd& data, double fs, double apf,
                              int num_bins) {
    const int num_channels = static_cast<int>(data.cols());
    EEG_PAC_Asymmetry_SoA result(num_channels);
    if (num_bins < 2) num_bins = 2;

    const auto combos = make_combos(apf);

    // 并行前预热 plan（bandpass_analytic 用 r2c + c2c 逆变换，尺寸由信号长度决定）
    const int num_samples = static_cast<int>(data.rows());
    const int nfft = dsp::next_power_of_2(num_samples);
    dsp::warmup_r2c(nfft);
    dsp::warmup_c2c_backward(nfft);

    // 逐通道、逐耦合模式计算 PAC（调制指数 MI）；各通道写不同的 result.features[*][ch]；
    // 仅当工作量足够大时才并行，避免小输入下线程开销超过收益。
    const long work = static_cast<long>(num_channels) * num_samples;
    #pragma omp parallel for if(work >= kOmpMinWork)
    for (int ch = 0; ch < num_channels; ++ch) {
        const Eigen::VectorXd x = data.col(ch);
        for (const auto& [cidx, plo, phi, alo, ahi] : combos) {
            result.features[cidx][ch] = modulation_index(x, fs, plo, phi, alo, ahi, num_bins);
        }
    }
    return result;
}

} // namespace pac_asymmetry
