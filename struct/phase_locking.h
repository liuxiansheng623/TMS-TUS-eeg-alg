#ifndef PHASE_LOCKING_H
#define PHASE_LOCKING_H

/*特征列表结构体
适用：phase_locking_index
使用方法：
  phase_locking::EEG_Phase_Locking_SoA res(num_channels);      // 按通道数构造
  res.at(phase_locking::ALPHA, ch) = value;                    // 写：at(频段, 通道)
  double v = res.features[phase_locking::ALPHA][ch];           // 读：features[频段][通道]
*/
#include <array>
#include <vector>
#include <cstddef>

namespace phase_locking {

// 1.枚举定义 8 个指标的索引
enum Phase_Locking_Index : int {
    DELTA = 0, //0 - delta (δ波)：频率通常在 1–4 Hz。
    THETA = 1, //1 - theta (θ波)：频率通常在 4–(APF-2) Hz。
    LO_ALPHA = 2, //2 - low alpha (α波)：频率通常在 (APF-2)–APF Hz。
    HI_ALPHA = 3, //3 - high alpha (α波)：频率通常在 APF–(APF+2) Hz。
    ALPHA = 4, //4 - alpha (α波)：频率通常在 (APF-2)–(APF+2) Hz。
    LO_BETA = 5, //5 - low beta (β波)：频率通常在 (APF+2)–20 Hz。
    BETA = 6, //6 - high beta (β波)：频率通常在 20–30 Hz。
    GAMMA = 7, //7 - low gamma (γ波)：频率通常在 30–45 Hz。
    PL_FEATURE_COUNT = 8
};

// 2. SoA 结构体设计
struct EEG_Phase_Locking_SoA {
    // 核心：8 个频段/特征，每个特征包含 N 个通道的数据
    // 外层数组大小固定为 8 ，内层数组大小由通道数决定
    std::array<std::vector<double>, PL_FEATURE_COUNT> features;

    size_t num_channels;

    /**
     * @brief 创建相位锁定结果容器，并将全部 ITPC 值初始化为 0。
     * @param ch_count 输入：需要存储的 EEG 通道数量。
     * @return 输出：构造完成的结果对象，包含 8×ch_count 个零值。
     */
    explicit EEG_Phase_Locking_SoA(size_t ch_count) : num_channels(ch_count) {
        for (size_t i = 0; i < PL_FEATURE_COUNT; ++i) {
            features[i].resize(ch_count, 0.0); // 为每个特征分配 N 个通道的内存并清零
        }
    }

    /**
     * @brief 获取指定 ITPC 频带、指定通道的可写引用。
     * @param feature_idx 输入：相位锁定频带特征索引。
     * @param ch_idx 输入：通道索引。
     * @return 输出：对应 ITPC 值的可写引用。
     * @note 调用方负责保证两个索引均未越界。
     */
    double& at(size_t feature_idx, size_t ch_idx) {
        return features[feature_idx][ch_idx];
    }

};

} // namespace phase_locking

#endif // PHASE_LOCKING_H
