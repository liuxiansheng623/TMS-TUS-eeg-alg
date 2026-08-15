#ifndef PAC_ASYMMETERY_H
#define PAC_ASYMMETERY_H

/*特征列表结构体
适用：PAC_asymmetery
在神经科学中，PAC（Phase-Amplitude Coupling）
指的是低频脑电波的相位（Phase）调节高频脑电波的振幅
（Amplitude）的现象。本结构体存储每个通道在各耦合模式下的 PAC 强度（调制指数 MI ∈ [0,1]）。
使用方法：
  pac_asymmetry::EEG_PAC_Asymmetry_SoA res(num_channels);      // 按通道数构造
  res.at(pac_asymmetry::DELTA_LO_GAMMA, ch) = value;           // 写：at(耦合模式, 通道)
  double v = res.features[pac_asymmetry::DELTA_LO_GAMMA][ch];  // 读：features[模式][通道]
*/
#include <array>
#include <vector>
#include <cstddef>

namespace pac_asymmetry {

// 1.枚举定义 8 个指标的索引
enum PAC_Asymmetry_Index : int {
    DELTA_LO_GAMMA = 0,     // Delta 相位 - LoGamma 振幅 (1, 4)-(30, 45) Hz
    DELTA_HI_GAMMA = 1,     // Delta 相位 - HiGamma 振幅 (1, 4)-(45, 70) Hz
    THETA_LO_GAMMA = 2,     // Theta 相位 - LoGamma 振幅 (4, APF - 2)-(30, 45) Hz
    THETA_HI_GAMMA = 3,     // Theta 相位 - HiGamma 振幅 (4, APF - 2)-(45, 70) Hz
    ALPHA_LO_GAMMA = 4,     // Alpha 相位 - LoGamma 振幅 (APF - 2, APF + 2)-(30, 45) Hz
    ALPHA_HI_GAMMA = 5,     // Alpha 相位 - HiGamma 振幅 (APF - 2, APF + 2)-(45, 70) Hz
    BETA_LO_GAMMA = 6,      // Beta 相位 - LoGamma 振幅 (APF + 2, 20)-(30, 45) Hz
    BETA_HI_GAMMA = 7,      // Beta 相位 - HiGamma 振幅 (APF + 2, 20)-(45, 70) Hz
    PAC_FEATURE_COUNT = 8
};

// 2. SoA 结构体设计
struct EEG_PAC_Asymmetry_SoA {
    // 核心：8 个频段/特征，每个特征包含 N 个通道的数据
    // 外层数组大小固定为 8 ，内层数组大小由通道数决定
    std::array<std::vector<double>, PAC_FEATURE_COUNT> features;

    size_t num_channels;

    /**
     * @brief 创建 PAC 结果容器，并将全部调制指数初始化为 0。
     * @param ch_count 输入：需要存储的 EEG 通道数量。
     * @return 输出：构造完成的结果对象，包含 8×ch_count 个零值。
     */
    explicit EEG_PAC_Asymmetry_SoA(size_t ch_count) : num_channels(ch_count) {
        for (size_t i = 0; i < PAC_FEATURE_COUNT; ++i) {
            features[i].resize(ch_count, 0.0); // 为每个特征分配 N 个通道的内存并清零
        }
    }

    /**
     * @brief 获取指定 PAC 耦合模式、指定通道的可写引用。
     * @param feature_idx 输入：PAC 耦合模式索引。
     * @param ch_idx 输入：通道索引。
     * @return 输出：对应调制指数的可写引用。
     * @note 调用方负责保证两个索引均未越界。
     */
    double& at(size_t feature_idx, size_t ch_idx) {
        return features[feature_idx][ch_idx];
    }

};

} // namespace pac_asymmetry

#endif // PAC_ASYMMETERY_H
