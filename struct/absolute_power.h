#ifndef ABSOLUTE_POWER_H
#define ABSOLUTE_POWER_H

/*特征列表结构体
绝对能量：适用absolute_power和indiv_absolute_power
使用方法：
  absolute_power::EEG_Absolute_Power_SoA res(num_channels);    // 按通道数构造
  res.at(absolute_power::LO_ALPHA, ch) = value;                // 写：at(特征, 通道)
  double v = res.features[absolute_power::DELTA][ch];          // 读：features[特征][通道]
  double apf = res.get_apf(ch);                                // 语义化读取 APF
*/
#include <array>
#include <vector>
#include <cstddef>

namespace absolute_power {

// 1.枚举定义 14 个指标的索引
enum Absolute_Power_Index : int {
    DELTA = 0, //0 - delta (δ波)：频率通常在 1–4 Hz。
    THETA = 1, //1 - theta (θ波)：频率通常在 4–(APF-2) Hz。
    LO_ALPHA = 2, //2 - low alpha (α波)：频率通常在 (APF-2)–APF Hz。
    HI_ALPHA = 3, //3 - high alpha (α波)：频率通常在 APF–(APF+2) Hz。
    LO_BETA = 4, //4 - low beta (β波)：频率通常在 (APF+2)–20 Hz。
    HI_BETA = 5, //5 - high beta (β波)：频率通常在 20–30 Hz。
    LO_GAMMA = 6, //6 - low gamma (γ波)：频率通常在 30–45 Hz。
    HI_GAMMA = 7, //7 - high gamma (γ波)：频率通常在 45–70 Hz。
    VHI_GAMMA = 8, //8 - very high gamma (γ波)：频率通常在 95–150 Hz。
    VVHI_FREQ = 9, //9 - very very high frequency (VVHF)：频率通常在 150–300 Hz。
    MUSCLE = 10, //10 - muscle activity：肌肉活动 ：频率通常为20-250Hz，主要是肌电信号的频率范围。
    POWER_SUPPLY_50HZ = 11, //11 - power supply frequency (50 Hz)：电源频率 (50 Hz)
    POWER_SUPPLY_100HZ = 12, //12 - power supply frequency (100 Hz)：电源频率 (100 Hz)
    APF = 13,//APF (Alpha Peak Frequency, Alpha峰值频率)：
            //指在Alpha频段（8-13Hz）内，功率谱密度（PSD）达到最大值时对应的确切频率。
            //如果是直接计算，则用输入的APF值。如果是计算个体的绝对功率，则利用CoG来计算.
    AP_FEATURE_COUNT = 14
};

// 2. SoA 结构体设计
struct EEG_Absolute_Power_SoA {
    // 核心：14 个频段/特征，每个特征包含 N 个通道的数据
    // 外层数组大小固定为 14，内层数组大小由通道数决定
    std::array<std::vector<double>, AP_FEATURE_COUNT> features;

    size_t num_channels;

    /**
     * @brief 创建绝对功率结果容器，并将全部特征值初始化为 0。
     * @param ch_count 输入：需要存储的 EEG 通道数量。
     * @return 输出：构造完成的结果对象，包含 14×ch_count 个零值。
     */
    explicit EEG_Absolute_Power_SoA(size_t ch_count) : num_channels(ch_count) {
        for (size_t i = 0; i < AP_FEATURE_COUNT; ++i) {
            features[i].resize(ch_count, 0.0); // 为每个特征分配 N 个通道的内存并清零
        }
    }

    /**
     * @brief 获取指定特征、指定通道的可写引用。
     * @param feature_idx 输入：绝对功率特征索引。
     * @param ch_idx 输入：通道索引。
     * @return 输出：对应特征值的可写引用。
     * @note 调用方负责保证两个索引均未越界。
     */
    double& at(size_t feature_idx, size_t ch_idx) {
        return features[feature_idx][ch_idx];
    }

    /**
     * @brief 读取指定通道实际使用或估计得到的 APF。
     * @param ch_idx 输入：通道索引。
     * @return 输出：该通道的 Alpha 峰值频率，单位 Hz。
     * @note 调用方负责保证通道索引未越界。
     */
    double get_apf(size_t ch_idx) const {
        return features[APF][ch_idx];
    }
};

} // namespace absolute_power

#endif // ABSOLUTE_POWER_H
