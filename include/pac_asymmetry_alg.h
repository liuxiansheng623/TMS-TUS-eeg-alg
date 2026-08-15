/*相位-振幅耦合(PAC)算法声明（Tort 调制指数，逐通道）
适用：PAC_asymmetry
计算每个通道上“低频相位调节高频振幅”的耦合强度（调制指数 MI）。*/
#ifndef PAC_ASYMMETRY_ALG_H
#define PAC_ASYMMETRY_ALG_H

#include <Eigen/Dense>
#include <vector>
#include "PAC_asymmetery.h"   // struct/ 下的输出结构体（文件名保留原拼写）
#include "eeg_alg_export.h"

namespace pac_asymmetry {

namespace detail {

/**
 * @brief 根据瞬时相位和瞬时振幅计算 Tort MI。
 * @param phase 输入：低频瞬时相位序列，单位弧度。
 * @param amplitude 输入：与 phase 等长的高频瞬时振幅序列。
 * @param num_bins 输入：相位分箱数量，要求至少为 2。
 * @return 输出：Tort MI；输入非法或相位箱覆盖不完整时返回 NaN。
 * @note 该函数作为公式级测试入口；业务代码通常应调用 compute()。
 */
EEG_ALG_API double tort_mi_from_phase_amplitude(const Eigen::VectorXd& phase,
                                                const Eigen::VectorXd& amplitude,
                                                int num_bins);

} // namespace detail

/**
 * 计算逐通道相位-振幅耦合（PAC, Phase-Amplitude Coupling）
 *
 * 对每个通道，量化“低频脑电波的相位(Phase) 调节 高频脑电波的振幅(Amplitude)”
 * 的耦合强度，采用 Tort 调制指数(MI)：
 *   MI = (ln N − H(P)) / ln N，
 *   其中先计算每个低频相位箱内的平均高频振幅，再归一化得到分布 P，
 *   H 为香农熵；MI ∈ [0,1]，
 *   越大表示低频相位对高频振幅的调制越强。
 *   若输入没有覆盖全部相位箱，则返回 NaN，表示该段数据不足以可靠估计 MI。
 *
 * @param data     EEG 连续数据矩阵，形状 (采样点 × 通道)
 * @param fs       采样率 (Hz)，要求 fs > 0
 * @param apf      Alpha 峰值频率 (Hz)，用于频带边界
 * @param num_bins 相位分箱数，默认 18（每箱 20°）
 * @return EEG_PAC_Asymmetry_SoA（8 耦合模式 × 通道数），
 *         features[耦合模式][通道] 即该通道在该模式下的 PAC(MI) 值
 *
 * 8 个耦合模式（相位带 × 振幅带）：
 *   DELTA(1–4) / THETA(4–(APF-2)) / ALPHA((APF-2)–(APF+2)) / BETA((APF+2)–20)
 *   ×  LO_GAMMA(30–45) / HI_GAMMA(45–70)
 */
EEG_ALG_API EEG_PAC_Asymmetry_SoA compute(const Eigen::MatrixXd& data, double fs, double apf,
                                          int num_bins = 18);

/**
 * ──────────────── 输入 / 输出 / 使用方法 ────────────────
 * 输入：
 *   - data     : Eigen::MatrixXd，形状 (采样点 × 通道) 的连续 EEG
 *   - fs       : 采样率 (Hz)
 *   - apf      : Alpha 峰值频率 (Hz)
 *   - num_bins : 相位分箱数，默认 18（可选）
 * 输出：
 *   - EEG_PAC_Asymmetry_SoA：8 耦合模式 × 通道数，
 *     features[模式][通道] 为该通道的 PAC 调制指数 MI ∈ [0,1]。
 * 使用方法：
 * @code
 *   auto res = pac_asymmetry::compute(data, 256.0, 10.0);
 *   double mi = res.features[pac_asymmetry::DELTA_LO_GAMMA][0];  // 第0通道 delta–lo_gamma 的 PAC
 * @endcode
 */

} // namespace pac_asymmetry

#endif // PAC_ASYMMETRY_ALG_H
