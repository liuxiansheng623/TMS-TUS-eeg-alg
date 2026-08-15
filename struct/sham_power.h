#ifndef SHAM_POWER_H
#define SHAM_POWER_H

/*特征列表结构体
适用：sham_power_index
使用方法：
  sham_power::Sham_Power_SoA res(num_channels);                // 按通道数构造
  res.at(sham_power::SHAM_BAND_1, ch) = value;                 // 写：at(特征, 通道)
  double v = res.features[sham_power::SHAM_BAND_0][ch];        // 读：features[特征][通道]
  double rdn = res.features[sham_power::RDN_FREQ_PNT][ch];     // 读取实际随机起点
*/
#include <array>
#include <vector>

namespace sham_power {

// 1. 定义 Sham 特征的索引枚举
enum Sham_Feature_Index : int {
    SHAM_BAND_0 = 0,      // 随机频段 0 指定的频率RDN_FREQ_PNT->RDN_FREQ_PNT+1Hz
    SHAM_BAND_1 = 1,      // 随机频段 1 指定的频率RDN_FREQ_PNT+1->RDN_FREQ_PNT+5Hz
    SHAM_BAND_2 = 2,      // 随机频段 2 指定的频率RDN_FREQ_PNT+5->RDN_FREQ_PNT+9Hz
    SHAM_BAND_3 = 3,      // 随机频段 3 指定的频率RDN_FREQ_PNT+9->RDN_FREQ_PNT+13Hz
    SHAM_BAND_4 = 4,      // 随机频段 4 指定的频率RDN_FREQ_PNT+13->RDN_FREQ_PNT+17Hz
    SHAM_BAND_5 = 5,      // 随机频段 5 指定的频率RDN_FREQ_PNT+17->RDN_FREQ_PNT+21Hz
    SHAM_BAND_6 = 6,      // 随机频段 6 指定的频率RDN_FREQ_PNT+21->RDN_FREQ_PNT+25Hz
    SHAM_BAND_7 = 7,      // 随机频段 7 指定的频率RDN_FREQ_PNT+25->RDN_FREQ_PNT+29Hz
    SHAM_BAND_8 = 8,      // 随机频段 8 指定的频率RDN_FREQ_PNT+29->RDN_FREQ_PNT+33Hz
    SHAM_BAND_9 = 9,      // 随机频段 9 指定的频率RDN_FREQ_PNT+33->RDN_FREQ_PNT+37Hz
    SHAM_BAND_10 = 10,    // 随机频段 10 指定的频率RDN_FREQ_PNT+37->RDN_FREQ_PNT+41Hz
    POWER_SUPPLY_50HZ = 11,
    POWER_SUPPLY_100HZ = 12,
    RDN_FREQ_PNT = 13,    // 随机起点频率值
    SHAM_FEATURE_COUNT = 14 // 总特征数量
};

// 2. Sham Power SoA 结构体
struct Sham_Power_SoA {
    std::array<std::vector<double>, SHAM_FEATURE_COUNT> features;
    size_t num_channels;

    /**
     * @brief 创建 Sham 功率结果容器，并将全部特征值初始化为 0。
     * @param ch_count 输入：需要存储的 EEG 通道数量。
     * @return 输出：构造完成的结果对象，包含 14×ch_count 个零值。
     */
    explicit Sham_Power_SoA(size_t ch_count) : num_channels(ch_count) {
        for (int i = 0; i < SHAM_FEATURE_COUNT; ++i) {
            features[i].resize(ch_count, 0.0);
        }
    }

    /**
     * @brief 获取指定 Sham 特征、指定通道的可写引用。
     * @param feature_idx 输入：Sham 特征索引。
     * @param ch_idx 输入：通道索引。
     * @return 输出：对应特征值的可写引用。
     * @note 调用方负责保证两个索引均未越界。
     */
    double& at(size_t feature_idx, size_t ch_idx) {
        return features[feature_idx][ch_idx];
    }
};

} // namespace sham_power

#endif // SHAM_POWER_H
