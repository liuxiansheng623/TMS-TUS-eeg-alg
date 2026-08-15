/*绝对功率算法实现*/
#include "absolute_power_alg.h"
#include "dsp_utils.h"

#include <vector>
#include <tuple>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace absolute_power {

namespace {

constexpr double MAX_REQUIRED_FREQUENCY_HZ = 300.0;

// 触发 OpenMP 并行的最小工作量（通道数×采样点）；低于此值串行执行更划算。
constexpr long kOmpMinWork = 1L << 17;

/**
 * @brief 校验采样率能否覆盖绝对功率协议中的最高频率 300 Hz。
 * @param fs 输入：采样率，单位 Hz。
 * @return 输出：无；fs 合法时正常返回。
 * @throws std::invalid_argument 当 fs 非有限值或 fs<600 Hz 时抛出。
 */
void validate_sampling_rate(double fs) {
    if (!std::isfinite(fs) || fs < 2.0 * MAX_REQUIRED_FREQUENCY_HZ) {
        throw std::invalid_argument(
            "absolute_power requires fs >= 600 Hz to compute the full 150-300 Hz band");
    }
}

// 频带定义：(特征索引, 下界Hz, 上界Hz)，上下界依赖 APF
using BandDef = std::tuple<int, double, double>;

/**
 * @brief 根据 APF 构造绝对功率算法使用的全部频带定义。
 * @param apf 输入：Alpha 峰值频率，单位 Hz。
 * @return 输出：由“特征索引、频带下界、频带上界”组成的频带列表。
 */
std::vector<BandDef> make_bands(double apf) {
    return {
        {DELTA,            1.0,      4.0},
        {THETA,            4.0,      apf - 2.0},
        {LO_ALPHA,         apf - 2.0, apf},
        {HI_ALPHA,         apf,      apf + 2.0},
        {LO_BETA,          apf + 2.0, 20.0},
        {HI_BETA,          20.0,     30.0},
        {LO_GAMMA,         30.0,     45.0},
        {HI_GAMMA,         45.0,     70.0},
        {VHI_GAMMA,        95.0,     150.0},
        {VVHI_FREQ,        150.0,    300.0},
        {MUSCLE,           20.0,     250.0},
        {POWER_SUPPLY_50HZ,  49.0,    51.0},
        {POWER_SUPPLY_100HZ, 99.0,    101.0},
    };
}

/**
 * @brief 根据给定 PSD 和 APF 填充一个通道的全部绝对功率特征。
 * @param res 输入/输出：待写入的绝对功率结果结构体。
 * @param freqs 输入：PSD 对应的频率坐标，单位 Hz。
 * @param psd 输入：单通道功率谱密度。
 * @param apf 输入：本通道使用的 Alpha 峰值频率，单位 Hz。
 * @param ch 输入：待写入的通道索引。
 * @return 输出：无；频带功率和 APF 被写入 res.features。
 */
void fill_bands(EEG_Absolute_Power_SoA& res,
                const Eigen::VectorXd& freqs, const Eigen::VectorXd& psd,
                double apf, int ch) {
    for (const auto& [idx, lo, hi] : make_bands(apf)) {
        res.features[idx][ch] = dsp::band_power(freqs, psd, lo, hi);
    }
    res.features[APF][ch] = apf;                          // APF 存值
}

} // namespace

/**
 * @brief 使用外部给定的统一 APF 计算各通道绝对功率。
 * @param data 输入：EEG 矩阵，行为采样点、列为通道。
 * @param fs 输入：采样率，单位 Hz，要求 fs>=600。
 * @param apf 输入：所有通道共用的 Alpha 峰值频率，单位 Hz。
 * @param nperseg 输入：Welch PSD 的分段长度。
 * @return 输出：14 个特征乘通道数的绝对功率结果。
 */
EEG_Absolute_Power_SoA compute(const Eigen::MatrixXd& data, double fs,
                               double apf, int nperseg) {
    validate_sampling_rate(fs);
    const int num_samples  = static_cast<int>(data.rows());
    const int num_channels = static_cast<int>(data.cols());
    EEG_Absolute_Power_SoA result(num_channels);

    // 并行前预热 plan（FFTW planner 非线程安全，不能在并行区内首次创建）
    const int nfft = dsp::next_power_of_2(dsp::auto_nperseg(num_samples, nperseg));
    dsp::warmup_r2c(nfft);

    // 各通道相互独立，写不同的 result.features[*][ch]，无数据竞争；
    // 仅当工作量足够大时才并行，避免小输入下线程开销超过收益。
    const long work = static_cast<long>(num_channels) * num_samples;
    #pragma omp parallel for if(work >= kOmpMinWork)
    for (int ch = 0; ch < num_channels; ++ch) {
        const Eigen::VectorXd x = data.col(ch);           // 取出单通道（连续内存）
        auto psd_pair = dsp::welch_psd(x, fs, nperseg);
        fill_bands(result, psd_pair.first, psd_pair.second, apf, ch);
    }
    return result;
}

/**
 * @brief 逐通道用 CoG 估计 APF，并据此计算个体化绝对功率。
 * @param data 输入：EEG 矩阵，行为采样点、列为通道。
 * @param fs 输入：采样率，单位 Hz，要求 fs>=600。
 * @param nperseg 输入：Welch PSD 的分段长度。
 * @return 输出：14 个特征乘通道数的个体化绝对功率结果，其中包含各通道 APF。
 */
EEG_Absolute_Power_SoA compute_indiv(const Eigen::MatrixXd& data, double fs,
                                     int nperseg) {
    validate_sampling_rate(fs);
    const int num_samples  = static_cast<int>(data.rows());
    const int num_channels = static_cast<int>(data.cols());
    EEG_Absolute_Power_SoA result(num_channels);

    // 并行前预热 plan（FFTW planner 非线程安全，不能在并行区内首次创建）
    const int nfft = dsp::next_power_of_2(dsp::auto_nperseg(num_samples, nperseg));
    dsp::warmup_r2c(nfft);

    // 各通道相互独立（APF 逐通道计算），写不同的 result.features[*][ch]；
    // 仅当工作量足够大时才并行，避免小输入下线程开销超过收益。
    const long work = static_cast<long>(num_channels) * num_samples;
    #pragma omp parallel for if(work >= kOmpMinWork)
    for (int ch = 0; ch < num_channels; ++ch) {
        const Eigen::VectorXd x = data.col(ch);
        auto psd_pair = dsp::welch_psd(x, fs, nperseg);
        // 逐通道用 CoG 计算 APF；失败回退 10.0，并夹紧到 [8,13]
        double apf = dsp::compute_apf_cog(psd_pair.first, psd_pair.second, 8.0, 13.0);
        if (std::isnan(apf)) apf = 10.0;
        apf = std::clamp(apf, 8.0, 13.0);
        fill_bands(result, psd_pair.first, psd_pair.second, apf, ch);
    }
    return result;
}

} // namespace absolute_power
