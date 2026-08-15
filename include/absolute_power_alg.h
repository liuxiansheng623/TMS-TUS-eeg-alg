/*绝对功率算法声明
适用：absolute_power（全局 APF）与 indiv_absolute_power（逐通道 CoG 计算 APF）*/
#ifndef ABSOLUTE_POWER_ALG_H
#define ABSOLUTE_POWER_ALG_H

#include <Eigen/Dense>
#include "absolute_power.h"   // struct/ 下的输出结构体
#include "eeg_alg_export.h"

namespace absolute_power {

/**
 * 计算绝对功率（全局 APF）
 *
 * 对每个通道做 Welch PSD 估计，再按依赖 APF 的频带边界计算各频带功率，
 * 所有通道共用同一个传入的 APF 值。
 *
 * @param data     EEG 数据矩阵，形状 (采样点 × 通道)
 * @param fs       采样率 (Hz)，要求 fs >= 600，以完整计算最高 300 Hz 的频带
 * @param apf      Alpha 峰值频率 (Hz)，用于确定频带边界，建议 [8,13]
 * @param nperseg  Welch 窗长，默认 256
 * @return EEG_Absolute_Power_SoA（14 特征 × 通道数）
 *
 * 频带定义（APF 相关）：
 *   DELTA 1–4 | THETA 4–(APF-2) | LO_ALPHA (APF-2)–APF | HI_ALPHA APF–(APF+2)
 *   LO_BETA (APF+2)–20 | HI_BETA 20–30 | LO_GAMMA 30–45 | HI_GAMMA 45–70
 *   VHI_GAMMA 95–150 | VVHI 150–300 | MUSCLE 20–250 | 50HZ 49–51 | 100HZ 99–101
 *   APF 特征：直接存储传入的 apf 值
 */
EEG_ALG_API EEG_Absolute_Power_SoA compute(const Eigen::MatrixXd& data, double fs,
                                           double apf, int nperseg = 256);

/**
 * 计算个体化绝对功率（逐通道 APF）
 *
 * 与 compute() 相同，但 APF 由每个通道独立计算：
 * 在 [8,13] Hz 内用重心法(CoG) APF = Σ(f·PSD)/Σ(PSD)；
 * 若计算失败(返回 NaN) 则回退为 10.0，并将结果夹紧到 [8,13]。
 *
 * @param data     EEG 数据矩阵，形状 (采样点 × 通道)
 * @param fs       采样率 (Hz)，要求 fs >= 600
 * @param nperseg  Welch 窗长，默认 256
 * @return EEG_Absolute_Power_SoA，其中 APF 特征为各通道独立值
 */
EEG_ALG_API EEG_Absolute_Power_SoA compute_indiv(const Eigen::MatrixXd& data, double fs,
                                                 int nperseg = 256);

/**
 * ──────────────── 输入 / 输出 / 使用方法 ────────────────
 * 输入：
 *   - data    : Eigen::MatrixXd，形状 (采样点 × 通道)，8 或 32 通道 EEG
 *   - fs      : 采样率 (Hz)
 *   - apf     : (仅 compute) 全局 Alpha 峰值频率 (Hz)
 *   - nperseg : Welch 窗长，默认 256（可选）
 * 输出：
 *   - EEG_Absolute_Power_SoA：14 个特征 × 通道数；
 *     用 result.features[特征索引][通道] 读取，APF 特征存所用 APF 值。
 * 使用方法：
 * @code
 *   Eigen::MatrixXd data(num_samples, num_channels);   // 行=采样点，列=通道
 *   // ... 填入 EEG 数据 ...
 *   // 1) 全局 APF：
 *   auto res = absolute_power::compute(data, 1024.0, 10.0, 1024);
 *   double delta_ch0 = res.features[absolute_power::DELTA][0];
 *   // 2) 个体化（逐通道 CoG 计算 APF）：
 *   auto indiv = absolute_power::compute_indiv(data, 1024.0, 1024);
 *   double apf_ch0 = indiv.get_apf(0);
 * @endcode
 */

} // namespace absolute_power

#endif // ABSOLUTE_POWER_ALG_H
