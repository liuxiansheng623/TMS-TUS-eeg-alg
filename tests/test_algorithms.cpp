/*四个 EEG 算法的单元测试（GoogleTest）
用合成信号验证各算法的正确性。*/
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>
#include <utility>

#include "dsp_utils.h"
#include "absolute_power_alg.h"
#include "sham_power_alg.h"
#include "phase_locking_alg.h"
#include "pac_asymmetry_alg.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

/**
 * @brief 生成指定频率、振幅和初相位的正弦测试信号。
 * @param freq 输入：正弦频率，单位 Hz。
 * @param amp 输入：正弦峰值振幅。
 * @param fs 输入：采样率，单位 Hz。
 * @param n 输入：输出采样点数。
 * @param phase0 输入：初始相位，单位弧度。
 * @return 输出：长度为 n 的正弦信号向量。
 */
Eigen::VectorXd make_sine(double freq, double amp, double fs, int n, double phase0 = 0.0) {
    Eigen::VectorXd x(n);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        x[i] = amp * std::sin(2.0 * kPi * freq * t + phase0);
    }
    return x;
}

/**
 * @brief 将单通道测试信号复制为多通道 EEG 矩阵。
 * @param sig 输入：长度为 n 的单通道信号。
 * @param num_ch 输入：需要生成的通道数量。
 * @return 输出：形状为“n×num_ch”的矩阵，每列均为 sig。
 */
Eigen::MatrixXd replicate(const Eigen::VectorXd& sig, int num_ch) {
    Eigen::MatrixXd m(sig.size(), num_ch);
    for (int c = 0; c < num_ch; ++c) m.col(c) = sig;
    return m;
}

/**
 * @brief 使用固定随机种子生成可复现的标准高斯白噪声。
 * @param n 输入：输出采样点数。
 * @param seed 输入：伪随机数生成器种子。
 * @return 输出：长度为 n、均值 0、标准差 1 的高斯噪声向量。
 */
Eigen::VectorXd make_noise(int n, unsigned seed) {
    std::mt19937 gen(seed);
    std::normal_distribution<double> dist(0.0, 1.0);
    Eigen::VectorXd x(n);
    for (int i = 0; i < n; ++i) x[i] = dist(gen);
    return x;
}

} // namespace

// ============================== DSP 工具 ==============================

/** @brief 输入 2 Hz 正弦信号；输出为 delta 功率接近理论值且远端频带功率接近零的断言。 */
TEST(DspUtils, WelchSineBandPower) {
    const double fs = 256.0, A = 3.0, f0 = 2.0;
    const int n = 2560;
    auto x = make_sine(f0, A, fs, n);
    auto spec = dsp::welch_psd(x, fs, 256);
    const double p = dsp::band_power(spec.first, spec.second, 1.0, 4.0);
    EXPECT_NEAR(p, A * A / 2.0, 0.25 * (A * A / 2.0));     // ≈A²/2，±25%
    const double p_far = dsp::band_power(spec.first, spec.second, 20.0, 30.0);
    EXPECT_LT(p_far, 0.2);                                  // 远离频段应≈0
}

/** @brief 输入单一 10 Hz 谱峰；输出为 CoG APF 接近 10 Hz 的断言。 */
TEST(DspUtils, CoGSinglePeak) {
    const double fs = 256.0;
    auto x = make_sine(10.0, 1.0, fs, 2560);
    auto spec = dsp::welch_psd(x, fs, 256);
    EXPECT_NEAR(dsp::compute_apf_cog(spec.first, spec.second, 8.0, 13.0), 10.0, 0.6);
}

/** @brief 输入全零 PSD；输出为 CoG APF 返回 NaN 的断言。 */
TEST(DspUtils, CoGZeroPowerIsNaN) {
    Eigen::VectorXd freqs = Eigen::VectorXd::LinSpaced(10, 0.0, 9.0);
    Eigen::VectorXd psd = Eigen::VectorXd::Zero(10);
    EXPECT_TRUE(std::isnan(dsp::compute_apf_cog(freqs, psd, 8.0, 13.0)));
}

/** @brief 输入带常量偏置和线性趋势的信号；输出为 Welch 残余功率接近零的断言。 */
TEST(DspUtils, WelchRemovesMeanAndLinearTrend) {
    const double fs = 256.0;
    const int n = 2560;
    Eigen::VectorXd x(n);
    for (int i = 0; i < n; ++i) x[i] = 100.0 + 0.01 * static_cast<double>(i);
    auto spec = dsp::welch_psd(x, fs, 256);
    const double residual_power =
        dsp::band_power(spec.first, spec.second, 0.0, fs / 2.0);
    EXPECT_LT(residual_power, 1e-20);
}

/** @brief 输入 50 Hz 正弦并使用 9–11 Hz 带通；输出为阻带分量被充分抑制的断言。 */
TEST(DspUtils, BandpassRejectsStopband) {
    const double fs = 256.0;
    const int n = 2560;
    auto x = make_sine(50.0, 1.0, fs, n);                  // 50 Hz
    auto y = dsp::bandpass_filter(x, fs, 9.0, 11.0);        // 通带 9–11，应滤除 50Hz
    const double rms_in  = std::sqrt(x.squaredNorm() / n);
    const double rms_out = std::sqrt(y.squaredNorm() / n);
    EXPECT_LT(rms_out, 0.05 * rms_in);
}

/** @brief 输入已带通的 10 Hz 正弦；输出为 Hilbert 瞬时振幅接近原峰值的断言。 */
TEST(DspUtils, HilbertAmplitudeOfSine) {
    const double fs = 256.0, A = 3.0;
    const int n = 2560;
    auto x = make_sine(10.0, A, fs, n);
    auto filt = dsp::bandpass_filter(x, fs, 8.0, 12.0);
    auto hb = dsp::hilbert_transform(filt);
    const Eigen::VectorXd& amp = hb.second;
    const int a = n / 4, b = 3 * n / 4;                     // 取中间段避开边缘
    double mean = 0.0;
    for (int i = a; i < b; ++i) mean += amp[i];
    mean /= (b - a);
    EXPECT_NEAR(mean, A, 0.15 * A);
}

/** @brief 输入 10 Hz 正弦做 8–12 Hz 带通解析；输出为中段瞬时振幅接近峰值的断言。 */
TEST(DspUtils, BandpassAnalyticAmplitudeOfSine) {
    const double fs = 256.0, A = 3.0;
    const int n = 2560;
    auto x = make_sine(10.0, A, fs, n);
    auto an = dsp::bandpass_analytic(x, fs, 8.0, 12.0);
    const int a = n / 4, b = 3 * n / 4;                   // 取中间段避开边缘
    double mean = 0.0;
    for (int i = a; i < b; ++i) mean += an.second[i];
    mean /= (b - a);
    EXPECT_NEAR(mean, A, 0.15 * A);
}

/** @brief 输入 50 Hz 正弦做 9–11 Hz 带通解析；输出为阻带振幅被抑制接近零的断言。 */
TEST(DspUtils, BandpassAnalyticRejectsStopband) {
    const double fs = 256.0;
    const int n = 2560;
    auto x = make_sine(50.0, 1.0, fs, n);
    auto an = dsp::bandpass_analytic(x, fs, 9.0, 11.0);
    const double rms_amp = std::sqrt(an.second.squaredNorm() / n);
    EXPECT_LT(rms_amp, 0.05);
}

/** @brief 连续对两个不同信号做 Welch（触发 plan 缓存复用）；输出为两者各自峰值频带正确、互不污染的断言。 */
TEST(DspUtils, PlanReuseAcrossCalls) {
    const double fs = 256.0;
    const int n = 2560;
    auto x1 = make_sine(5.0, 2.0, fs, n);     // 5 Hz
    auto x2 = make_sine(20.0, 2.0, fs, n);    // 20 Hz
    auto s1 = dsp::welch_psd(x1, fs, 256);
    auto s2 = dsp::welch_psd(x2, fs, 256);    // 复用同尺寸 plan
    // 第一次结果：5 Hz 处功率占优
    EXPECT_GT(dsp::band_power(s1.first, s1.second, 4.0, 7.0),
              dsp::band_power(s1.first, s1.second, 18.0, 22.0));
    // 第二次结果（复用 plan 后）：20 Hz 处功率占优，且未被第一次污染
    EXPECT_GT(dsp::band_power(s2.first, s2.second, 18.0, 22.0),
              dsp::band_power(s2.first, s2.second, 4.0, 7.0));
}

// ============================== 绝对功率 ==============================

/** @brief 输入超过并行阈值的 32 通道数据；输出为并行路径下各通道结果一致且正确的断言。 */
TEST(AbsolutePower, ParallelPathCorrectness) {
    const double fs = 1024.0, apf = 10.0, A = 2.0;
    const int n = 8192, ch = 32;   // 32×8192 = 262144 ≥ 并行阈值 → 走并行路径
    auto data = replicate(make_sine(9.0, A, fs, n), ch);   // 9 Hz ∈ LO_ALPHA[8,10]
    auto res = absolute_power::compute(data, fs, apf, 1024);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    for (int c = 0; c < ch; ++c) {
        EXPECT_GT(res.features[absolute_power::LO_ALPHA][c], 0.0);
        EXPECT_DOUBLE_EQ(res.features[absolute_power::APF][c], apf);
    }
    // 所有通道输入相同 → 并行下各通道 LO_ALPHA 功率必须一致（无跨线程串扰）
    for (int c = 1; c < ch; ++c)
        EXPECT_NEAR(res.features[absolute_power::LO_ALPHA][c],
                    res.features[absolute_power::LO_ALPHA][0], 1e-9);
}

/** @brief 输入 9 Hz 多通道正弦；输出为 low-alpha 功率占优且 APF 正确回填的断言。 */
TEST(AbsolutePower, AlphaPeakInLoAlpha) {
    const double fs = 1024.0, apf = 10.0;
    const int n = 10240, ch = 8;
    auto x = make_sine(9.0, 2.0, fs, n);                   // 9Hz ∈ LO_ALPHA[8,10]
    auto data = replicate(x, ch);
    auto res = absolute_power::compute(data, fs, apf, 1024);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    EXPECT_GT(res.features[absolute_power::LO_ALPHA][0], res.features[absolute_power::DELTA][0]);
    EXPECT_GT(res.features[absolute_power::LO_ALPHA][0], res.features[absolute_power::HI_BETA][0]);
    for (int c = 0; c < ch; ++c)
        EXPECT_DOUBLE_EQ(res.features[absolute_power::APF][c], apf);
}

/** @brief 输入已知振幅的 2 Hz 正弦；输出为 delta 绝对功率符合 A²/2 的断言。 */
TEST(AbsolutePower, BandPowerMatchesTheory) {
    const double fs = 1024.0, apf = 10.0, A = 3.0;
    auto x = make_sine(2.0, A, fs, 10240);                 // delta 频段
    auto data = replicate(x, 4);
    auto res = absolute_power::compute(data, fs, apf, 1024);
    EXPECT_NEAR(res.features[absolute_power::DELTA][0], A * A / 2.0, 0.25 * (A * A / 2.0));
}

/** @brief 输入 32 通道 EEG；输出为结果通道数和特征数组维度正确的断言。 */
TEST(AbsolutePower, Channels32) {
    const double fs = 1024.0, apf = 10.0;
    const int n = 2048, ch = 32;
    auto data = replicate(make_sine(10.0, 1.0, fs, n), ch);
    auto res = absolute_power::compute(data, fs, apf, 1024);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    EXPECT_EQ(res.features[absolute_power::LO_ALPHA].size(), static_cast<size_t>(ch));
}

/** @brief 输入不足及临界采样率；输出为 fs<600 抛异常、fs=600 可计算的断言。 */
TEST(AbsolutePower, RejectsInsufficientSamplingRate) {
    Eigen::MatrixXd data = Eigen::MatrixXd::Zero(512, 1);
    EXPECT_THROW(absolute_power::compute(data, 256.0, 10.0), std::invalid_argument);
    EXPECT_THROW(absolute_power::compute_indiv(data, 256.0), std::invalid_argument);
    EXPECT_NO_THROW(absolute_power::compute(data, 600.0, 10.0));
}

// ============================ 个体化绝对功率 ============================

/** @brief 输入具有 9 Hz 和 11 Hz 谱峰的两个通道；输出为各通道 CoG APF 正确的断言。 */
TEST(IndivPower, CoGDetectsDifferentPeaks) {
    const double fs = 1024.0;
    const int n = 10240;
    Eigen::MatrixXd data(n, 2);
    data.col(0) = make_sine(9.0, 2.0, fs, n);
    data.col(1) = make_sine(11.0, 2.0, fs, n);
    auto res = absolute_power::compute_indiv(data, fs, 1024);
    EXPECT_NEAR(res.get_apf(0), 9.0, 0.7);
    EXPECT_NEAR(res.get_apf(1), 11.0, 0.7);
}

// ============================== 伪刺激功率 ==============================

/** @brief 输入固定随机起点和 22 Hz 正弦；输出为信号落入预期 Sham 频带的断言。 */
TEST(ShamPower, FixedRdnToneInBand) {
    const double fs = 256.0, rdn = 20.0;
    const int n = 2560, ch = 4;
    // BAND_1 = rdn+1..rdn+5 = 21..25；放 22Hz 音调
    auto x = make_sine(22.0, 2.0, fs, n);
    auto data = replicate(x, ch);
    auto res = sham_power::compute(data, fs, rdn);
    EXPECT_DOUBLE_EQ(res.features[sham_power::RDN_FREQ_PNT][0], rdn);
    EXPECT_GT(res.features[sham_power::SHAM_BAND_1][0], res.features[sham_power::SHAM_BAND_0][0]);
    EXPECT_GT(res.features[sham_power::SHAM_BAND_1][0], 0.5);
}

/** @brief 输入负随机起点标志；输出为自动生成的起点处于 Nyquist 允许范围内的断言。 */
TEST(ShamPower, AutoRdnInRange) {
    const double fs = 256.0;
    const int n = 512, ch = 2;
    auto data = replicate(make_noise(n, 42), ch);
    auto res = sham_power::compute(data, fs, -1.0);        // 自动随机
    const double rdn = res.features[sham_power::RDN_FREQ_PNT][0];
    EXPECT_GE(rdn, 1.0);
    EXPECT_LT(rdn + 41.0, fs / 2.0);
}

// ============================== 相位锁定(ITPC) ==============================

/** @brief 输入跨试次同相位的 10 Hz 信号；输出为 Alpha ITPC 接近 1 的断言。 */
TEST(PhaseLocking, ConsistentPhaseHighItpc) {
    const double fs = 256.0, apf = 10.0;
    const int n = 512, ch = 4, T = 20;
    std::vector<Eigen::MatrixXd> epochs;
    for (int t = 0; t < T; ++t)
        epochs.push_back(replicate(make_sine(10.0, 1.0, fs, n, 0.0), ch));  // 同相位
    auto res = phase_locking::compute(epochs, fs, apf);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    EXPECT_GT(res.features[phase_locking::ALPHA][0], 0.9);  // ALPHA(8–12) 含 10Hz
}

/** @brief 输入跨试次随机初相位的 10 Hz 信号；输出为 Alpha ITPC 较低的断言。 */
TEST(PhaseLocking, RandomPhaseLowItpc) {
    const double fs = 256.0, apf = 10.0;
    const int n = 512, ch = 4, T = 20;
    std::mt19937 gen(123);
    std::uniform_real_distribution<double> ud(0.0, 2.0 * kPi);
    std::vector<Eigen::MatrixXd> epochs;
    for (int t = 0; t < T; ++t)
        epochs.push_back(replicate(make_sine(10.0, 1.0, fs, n, ud(gen)), ch));
    auto res = phase_locking::compute(epochs, fs, apf);
    EXPECT_LT(res.features[phase_locking::ALPHA][0], 0.5);
}

/** @brief 输入 8 通道多试次信号；输出为全部 ITPC 特征维度正确的断言。 */
TEST(PhaseLocking, OutputDims) {
    const double fs = 256.0, apf = 10.0;
    const int n = 256, ch = 8, T = 5;
    std::vector<Eigen::MatrixXd> epochs;
    for (int t = 0; t < T; ++t)
        epochs.push_back(replicate(make_sine(10.0, 1.0, fs, n), ch));
    auto res = phase_locking::compute(epochs, fs, apf);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    for (int b = 0; b < 8; ++b)
        EXPECT_EQ(res.features[b].size(), static_cast<size_t>(ch));
}

// ============================ PAC（逐通道相位-振幅耦合）============================

/** @brief 输入样本数不同但平均振幅相同的相位箱；输出为 Tort MI 等于零的断言。 */
TEST(PacAsymmetry, TortMiUsesMeanAmplitudePerPhaseBin) {
    constexpr int bins = 4;
    constexpr double half_pi = kPi / 2.0;
    const int counts[bins] = {1, 2, 3, 4};
    Eigen::VectorXd phase(10);
    Eigen::VectorXd amplitude = Eigen::VectorXd::Constant(10, 2.0);

    int pos = 0;
    for (int b = 0; b < bins; ++b) {
        const double bin_center = -kPi + (static_cast<double>(b) + 0.5) * half_pi;
        for (int i = 0; i < counts[b]; ++i) phase[pos++] = bin_center;
    }

    // 每箱样本数不同，但每箱平均振幅相同，因此正确的 Tort MI 必须为 0。
    const double mi =
        pac_asymmetry::detail::tort_mi_from_phase_amplitude(phase, amplitude, bins);
    EXPECT_NEAR(mi, 0.0, 1e-12);
}

/** @brief 输入强耦合与无耦合通道；输出为强耦合通道 Tort MI 显著更高的断言。 */
TEST(PacAsymmetry, StrongPacChannelHighMI) {
    const double fs = 256.0, apf = 10.0;
    const int n = 2560;
    // 通道0：真实 delta(2Hz) 振荡 + 受其相位调制的 logamma(40Hz) 振幅 → 强 PAC
    // 通道1：delta + gamma 但 gamma 振幅未被调制 → 无 PAC
    Eigen::MatrixXd data(n, 2);
    for (int i = 0; i < n; ++i) {
        const double tt = static_cast<double>(i) / fs;
        const double delta = std::sin(2.0 * kPi * 2.0 * tt);
        const double gamma = std::sin(2.0 * kPi * 40.0 * tt);
        data(i, 0) = delta + (1.0 + 0.9 * delta) * gamma;   // 强 delta–gamma PAC
        data(i, 1) = delta + gamma;                          // 无 PAC
    }
    auto res = pac_asymmetry::compute(data, fs, apf);
    const double mi0 = res.features[pac_asymmetry::DELTA_LO_GAMMA][0];
    const double mi1 = res.features[pac_asymmetry::DELTA_LO_GAMMA][1];
    EXPECT_GT(mi0, mi1);                                     // 强 PAC 通道 MI 更高
    EXPECT_GT(mi0 - mi1, 0.02);                              // 且与无耦合通道差距明显
    EXPECT_GT(mi0, 0.05);                                    // 检测到耦合（MI 绝对值通常较小）
    EXPECT_TRUE(std::isfinite(mi0));
}

/** @brief 输入单通道白噪声；输出为各 PAC 模式结果有限、范围合法且总体较低的断言。 */
TEST(PacAsymmetry, NoiseLowMI) {
    const double fs = 256.0, apf = 10.0;
    const int n = 2560;
    Eigen::MatrixXd data(n, 1);
    data.col(0) = make_noise(n, 7);
    auto res = pac_asymmetry::compute(data, fs, apf);
    for (int c = 0; c < 8; ++c) {
        const double mi = res.features[c][0];
        EXPECT_TRUE(std::isfinite(mi));
        EXPECT_GE(mi, 0.0);
        EXPECT_LE(mi, 1.0);
    }
    // 白噪声的 delta–logamma PAC 应较低
    EXPECT_LT(res.features[pac_asymmetry::DELTA_LO_GAMMA][0], 0.3);
}

/** @brief 输入 4 通道白噪声；输出为 PAC 结果维度及数值范围正确的断言。 */
TEST(PacAsymmetry, OutputDimsAndRange) {
    const double fs = 256.0, apf = 10.0;
    const int n = 1024, ch = 4;
    Eigen::MatrixXd data(n, ch);
    for (int c = 0; c < ch; ++c) data.col(c) = make_noise(n, 20 + c);
    auto res = pac_asymmetry::compute(data, fs, apf);
    EXPECT_EQ(res.num_channels, static_cast<size_t>(ch));
    for (int c = 0; c < 8; ++c) {
        EXPECT_EQ(res.features[c].size(), static_cast<size_t>(ch));
        for (int k = 0; k < ch; ++k) {
            EXPECT_GE(res.features[c][k], 0.0);
            EXPECT_LE(res.features[c][k], 1.0);
        }
    }
}
