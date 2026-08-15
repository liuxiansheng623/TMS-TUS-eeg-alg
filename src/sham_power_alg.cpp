/*伪刺激(Sham)功率算法实现*/
#include "sham_power_alg.h"
#include "dsp_utils.h"

#include <vector>
#include <tuple>
#include <random>
#include <algorithm>

namespace sham_power {

namespace {

// 触发 OpenMP 并行的最小工作量（通道数×采样点）；低于此值串行执行更划算。
constexpr long kOmpMinWork = 1L << 17;

using BandDef = std::tuple<int, double, double>;   // (特征索引, 下界Hz, 上界Hz)

/**
 * @brief 依据随机起点构造 11 个 Sham 频带以及 50/100 Hz 工频带。
 * @param rdn 输入：当前对象使用的随机起点频率，单位 Hz。
 * @return 输出：由“特征索引、频带下界、频带上界”组成的频带列表。
 */
std::vector<BandDef> make_sham_bands(double rdn) {
    std::vector<BandDef> bands;
    bands.push_back({SHAM_BAND_0, rdn + 0.0, rdn + 1.0});        // 1 Hz 宽
    for (int k = 1; k <= 10; ++k) {
        // BAND_k: rdn+(4k-3) → rdn+(4k+1)，各 4 Hz 宽
        bands.push_back({SHAM_BAND_0 + k, rdn + (4.0 * k - 3.0), rdn + (4.0 * k + 1.0)});
    }
    bands.push_back({POWER_SUPPLY_50HZ,  49.0,  51.0});
    bands.push_back({POWER_SUPPLY_100HZ, 99.0, 101.0});
    return bands;
}

} // namespace

/**
 * @brief 计算各通道的 Sham 随机频带功率和工频功率。
 * @param data 输入：EEG 矩阵，行为采样点、列为通道。
 * @param fs 输入：采样率，单位 Hz。
 * @param rdn_freq_pnt 输入：随机起点频率；负值表示由函数内部随机生成。
 * @param nperseg 输入：Welch PSD 的分段长度。
 * @return 输出：14 个特征乘通道数的 Sham 功率结果，并包含实际使用的随机起点。
 */
Sham_Power_SoA compute(const Eigen::MatrixXd& data, double fs,
                       double rdn_freq_pnt, int nperseg) {
    const int num_channels = static_cast<int>(data.cols());
    Sham_Power_SoA result(num_channels);

    // 确定随机起点：传入为负则内部随机生成
    double rdn = rdn_freq_pnt;
    if (rdn < 0.0) {
        const double max_rdn = std::max(1.0, fs / 2.0 - 41.0);  // 保证 rdn+41 < fs/2
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<double> dist(1.0, max_rdn);
        rdn = dist(gen);
    }

    const auto bands = make_sham_bands(rdn);
    const double nyquist = fs / 2.0;

    // 并行前预热 plan（FFTW planner 非线程安全，不能在并行区内首次创建）
    const int num_samples = static_cast<int>(data.rows());
    const int nfft = dsp::next_power_of_2(dsp::auto_nperseg(num_samples, nperseg));
    dsp::warmup_r2c(nfft);

    // 各通道相互独立，写不同的 result.features[*][ch]，无数据竞争；
    // 仅当工作量足够大时才并行，避免小输入下线程开销超过收益。
    const long work = static_cast<long>(num_channels) * num_samples;
    #pragma omp parallel for if(work >= kOmpMinWork)
    for (int ch = 0; ch < num_channels; ++ch) {
        const Eigen::VectorXd x = data.col(ch);
        auto psd_pair = dsp::welch_psd(x, fs, nperseg);
        const Eigen::VectorXd& freqs = psd_pair.first;
        const Eigen::VectorXd& psd   = psd_pair.second;
        for (const auto& [idx, lo, hi] : bands) {
            const double h = std::min(hi, nyquist);
            result.features[idx][ch] = (h > lo) ? dsp::band_power(freqs, psd, lo, h) : 0.0;
        }
        result.features[RDN_FREQ_PNT][ch] = rdn;          // 回填实际随机起点
    }
    return result;
}

} // namespace sham_power
