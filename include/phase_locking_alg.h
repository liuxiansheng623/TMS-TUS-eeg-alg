/*相位锁定算法声明（试次间相位一致性 ITPC）
适用：phase_locking_index*/
#ifndef PHASE_LOCKING_ALG_H
#define PHASE_LOCKING_ALG_H

#include <Eigen/Dense>
#include <vector>
#include "phase_locking.h"    // struct/ 下的输出结构体
#include "eeg_alg_export.h"

namespace phase_locking {

/**
 * 计算试次间相位一致性（ITPC, Inter-Trial Phase Coherence）
 *
 * 输入分段(epoch)数据。对每个通道、每个频带：
 *   1. 对每个 trial 的信号带通滤波；
 *   2. 希尔伯特变换提取瞬时相位 φ；
 *   3. ITPC = mean_t | mean_trials exp(i·φ(t,trial)) |，∈[0,1]。
 * ITPC=1 表示所有试次相位完全一致，≈0 表示相位随机。
 *
 * @param epochs 分段 EEG 数据，每个元素为一个 trial 的矩阵 (采样点 × 通道)，
 *               所有 trial 需等长且通道数一致
 * @param fs     采样率 (Hz)，要求 fs > 0
 * @param apf    Alpha 峰值频率 (Hz)，用于频带边界
 * @return EEG_Phase_Locking_SoA（8 频带 × 通道数）
 *
 * 频带定义（APF 相关）：
 *   DELTA 1–4 | THETA 4–(APF-2) | LO_ALPHA (APF-2)–APF | HI_ALPHA APF–(APF+2)
 *   ALPHA (APF-2)–(APF+2) | LO_BETA (APF+2)–20 | BETA 20–30 | GAMMA 30–45
 */
EEG_ALG_API EEG_Phase_Locking_SoA compute(const std::vector<Eigen::MatrixXd>& epochs,
                                          double fs, double apf);

/**
 * ──────────────── 输入 / 输出 / 使用方法 ────────────────
 * 输入：
 *   - epochs : std::vector<Eigen::MatrixXd>，每个元素为一个 trial 的矩阵
 *              (采样点 × 通道)，所有 trial 需等长、通道数一致
 *   - fs     : 采样率 (Hz)
 *   - apf    : Alpha 峰值频率 (Hz)
 * 输出：
 *   - EEG_Phase_Locking_SoA：8 频带 × 通道数，值为 ITPC ∈ [0,1]。
 * 使用方法：
 * @code
 *   std::vector<Eigen::MatrixXd> epochs;        // 每个元素 (采样点 × 通道)
 *   for (int t = 0; t < num_trials; ++t) epochs.push_back(epoch_t);
 *   auto res = phase_locking::compute(epochs, 256.0, 10.0);
 *   double alpha_itpc_ch0 = res.features[phase_locking::ALPHA][0];
 * @endcode
 */

} // namespace phase_locking

#endif // PHASE_LOCKING_ALG_H
